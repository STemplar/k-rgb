#include "core/logitech_hidpp20_device.h"
#include "core/logitech_g610_g810_keymap.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace krgb {

LogitechHIDPP20Device::~LogitechHIDPP20Device() {
    close();
}

bool LogitechHIDPP20Device::parseHex(const std::string& value, int& out) {
    if(value.empty()) {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(value.c_str(), &end, 16);
    if(end == value.c_str() || *end != '\0' || errno != 0) {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

namespace {

std::string readTextFile(const fs::path& path) {
    std::ifstream in(path);
    if(!in) {
        return {};
    }
    std::string value;
    std::getline(in, value);
    while(!value.empty() &&
          (value.back() == '\r' || value.back() == '\n' ||
           value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

bool parseHidUevent(const fs::path& devlink, int& vid, int& pid,
                    std::string& hidName) {
    std::ifstream uevent(devlink / "uevent");
    if(!uevent) {
        return false;
    }

    vid = -1;
    pid = -1;
    hidName.clear();

    std::string line;
    while(std::getline(uevent, line)) {
        if(line.rfind("HID_ID=", 0) == 0) {
            const std::string body = line.substr(7);
            const auto p1 = body.find(':');
            const auto p2 = p1 == std::string::npos
                ? std::string::npos : body.find(':', p1 + 1);
            if(p1 != std::string::npos && p2 != std::string::npos) {
                const std::string vidText =
                    body.substr(p1 + 1, p2 - p1 - 1);
                const std::string pidText = body.substr(p2 + 1);

                char* end = nullptr;
                errno = 0;
                const long parsedVid = std::strtol(vidText.c_str(), &end, 16);
                if(end != vidText.c_str() && *end == '\0' && errno == 0) {
                    vid = static_cast<int>(parsedVid);
                }

                end = nullptr;
                errno = 0;
                const long parsedPid = std::strtol(pidText.c_str(), &end, 16);
                if(end != pidText.c_str() && *end == '\0' && errno == 0) {
                    pid = static_cast<int>(parsedPid);
                }
            }
        } else if(line.rfind("HID_NAME=", 0) == 0) {
            hidName = line.substr(9);
        }
    }

    return vid >= 0 && pid >= 0;
}

void fillUsbIdentity(const fs::path& realHidPath,
                     LogitechUsbIdentity& identity) {
    fs::path parent = realHidPath;
    for(int depth = 0; depth < 8 && !parent.empty(); ++depth) {
        if(identity.interfaceNumber < 0) {
            const std::string value = readTextFile(parent / "bInterfaceNumber");
            if(!value.empty()) {
                char* end = nullptr;
                errno = 0;
                const long parsed = std::strtol(value.c_str(), &end, 16);
                if(end && *end == '\0' && errno == 0) {
                    identity.interfaceNumber = static_cast<int>(parsed);
                }
            }
        }

        if(identity.manufacturer.empty()) {
            identity.manufacturer = readTextFile(parent / "manufacturer");
        }
        if(identity.product.empty()) {
            identity.product = readTextFile(parent / "product");
        }
        if(identity.serial.empty()) {
            identity.serial = readTextFile(parent / "serial");
        }

        if(!identity.manufacturer.empty() && !identity.product.empty() &&
           identity.interfaceNumber >= 0) {
            break;
        }
        parent = parent.parent_path();
    }
}

} // namespace

LogitechLightingColorCapability
LogitechHIDPP20Device::colorCapabilityForProductId(std::uint16_t productId) {
    const auto* known = logitechKnownDeviceForProductId(productId);
    return known ? known->colorCapability
                 : LogitechLightingColorCapability::Unknown;
}

LogitechLightingColorCapability LogitechHIDPP20Device::colorCapability() const {
    return colorCapabilityForProductId(usbIdentity_.productId);
}

std::string LogitechHIDPP20Device::displayName() const {
    if(!usbIdentity_.product.empty()) {
        return usbIdentity_.product;
    }
    if(const auto* known = logitechKnownDeviceForProductId(usbIdentity_.productId)) {
        return known->displayName;
    }
    if(!usbIdentity_.hidName.empty()) {
        return usbIdentity_.hidName;
    }

    char fallback[48]{};
    std::snprintf(fallback, sizeof(fallback),
                  "Logitech HID++ keyboard (046d:%04x)",
                  static_cast<unsigned>(usbIdentity_.productId));
    return fallback;
}

bool LogitechHIDPP20Device::ensureFeatureSet(std::string* err) {
    if(featureSetEnumerated_) {
        return true;
    }

    discoveredFeatures_.clear();

    // ROOT is always feature 0x0000 at runtime index 0 and is not included in
    // FeatureSet.GetCount(). Keep it explicit rather than querying
    // FeatureSet.GetFeatureID(0), which is not a valid non-root index.
    discoveredFeatures_.push_back({
        kFeatureRoot, 0x00, 0x00, 0x00, false
    });

    // Bootstrap through ROOT once: Feature Set itself is feature 0x0001.
    LogitechHIDPP20FeatureInfo featureSet;
    if(!getFeature(kFeatureSet, featureSet, err)) {
        discoveredFeatures_.clear();
        return false;
    }
    if(featureSet.index == 0) {
        if(err) {
            *err = "HID++ feature 0x0001 (Feature Set) is not supported";
        }
        discoveredFeatures_.clear();
        return false;
    }

    // ROOT.getFeature() provides authoritative Feature Set metadata including
    // the feature-set version, so insert it before enumerating the remaining
    // runtime feature indexes.
    discoveredFeatures_.push_back({
        kFeatureSet,
        featureSet.index,
        featureSet.type,
        featureSet.version,
        featureSet.versionKnown
    });

    // FeatureSet.GetCount (function 0). Count excludes ROOT; non-root runtime
    // indexes start at 1 and run through count inclusive.
    RawReport countRaw{};
    std::size_t countRawSize = 0;
    if(!requestLongRaw(featureSet.index, 0x00, nullptr, 0,
                       countRaw, countRawSize, err)) {
        return false;
    }
    if(countRawSize < 5) {
        if(err) {
            *err = "HID++ Feature Set GetCount returned a short response";
        }
        return false;
    }

    const std::uint16_t nonRootCount =
        static_cast<std::uint16_t>(countRaw[4]);
    discoveredFeatures_.reserve(static_cast<std::size_t>(nonRootCount) + 1u);

    // Feature Set v1 added featureVersion to GetFeatureID(). Version 0 only
    // returns featureID + featureType. ROOT.getFeature() still gave us the
    // Feature Set's own version above, so we can parse the response correctly.
    const bool featureVersionReturned =
        featureSet.versionKnown && featureSet.version >= 1;

    for(std::uint16_t index = 1; index <= nonRootCount; ++index) {
        const std::uint8_t queryIndex = static_cast<std::uint8_t>(index);

        // The Feature Set entry itself was already obtained through ROOT with
        // complete metadata. Avoid duplicating it if its runtime index appears
        // in the normal enumeration.
        if(queryIndex == featureSet.index) {
            continue;
        }

        RawReport featureRaw{};
        std::size_t featureRawSize = 0;
        if(!requestLongRaw(featureSet.index, 0x01,
                           &queryIndex, 1,
                           featureRaw, featureRawSize, err)) {
            if(err && !err->empty()) {
                *err = "HID++ Feature Set GetFeatureID index " +
                       std::to_string(index) + ": " + *err;
            }
            discoveredFeatures_.clear();
            return false;
        }
        if(featureRawSize < 7) {
            if(err) {
                *err = "HID++ Feature Set GetFeatureID returned a short response";
            }
            discoveredFeatures_.clear();
            return false;
        }

        LogitechHIDPP20Feature feature;
        feature.featureId =
            (static_cast<std::uint16_t>(featureRaw[4]) << 8) |
             static_cast<std::uint16_t>(featureRaw[5]);
        feature.index = queryIndex;
        feature.type = featureRaw[6];
        if(featureVersionReturned) {
            if(featureRawSize < 8) {
                if(err) {
                    *err = "HID++ Feature Set v1+ GetFeatureID omitted feature version";
                }
                discoveredFeatures_.clear();
                return false;
            }
            feature.version = featureRaw[7];
            feature.versionKnown = true;
        }
        discoveredFeatures_.push_back(feature);
    }

    featureSetEnumerated_ = true;
    return true;
}

bool LogitechHIDPP20Device::enumerateFeatures(
    std::vector<LogitechHIDPP20Feature>& features, std::string* err) {

    features.clear();
    if(!ensureFeatureSet(err)) {
        return false;
    }
    features = discoveredFeatures_;
    return true;
}

bool LogitechHIDPP20Device::getCapabilities(
    LogitechHIDPP20Capabilities& capabilities, std::string* err) {

    capabilities = {};

    if(!getProtocolInfo(capabilities.protocol, err)) {
        return false;
    }
    if(capabilities.protocol.protocolNumber < 2) {
        if(err) {
            *err = "device is not HID++ 2.0+";
        }
        return false;
    }

    return enumerateFeatures(capabilities.features, err);
}

bool LogitechHIDPP20Device::getDiscoveredFeature(
    std::uint16_t featureId, LogitechHIDPP20FeatureInfo& info,
    std::string* err) {

    info = {};
    if(!ensureFeatureSet(err)) {
        return false;
    }

    for(const auto& feature : discoveredFeatures_) {
        if(feature.featureId == featureId) {
            if(!feature.isUsableByEndUserSoftware()) {
                // Preserve the feature in diagnostics but do not let normal
                // capability code activate features Logitech marks hidden,
                // engineering or manufacturing/compliance-only.
                return true;
            }
            info.index = feature.index;
            info.type = feature.type;
            info.version = feature.version;
            info.versionKnown = feature.versionKnown;
            return true;
        }
    }

    // Unsupported is not a transport failure; preserve ROOT.getFeature()
    // semantics by returning success with index == 0.
    return true;
}

bool LogitechHIDPP20Device::getLightingFeatures(
    LogitechHIDPP20LightingFeatures& features, std::string* err) {

    features = {};

    const std::uint16_t ids[] = {
        kFeatureBrightnessControl,
        kFeatureColorLedEffects,
        kFeatureRgbEffects,
        kFeaturePerKeyLighting,
        kFeaturePerKeyLighting2,
    };
    LogitechHIDPP20FeatureInfo* out[] = {
        &features.brightness8040,
        &features.colorLedEffects8070,
        &features.rgbEffects8071,
        &features.perKey8080,
        &features.perKey8081,
    };

    for(std::size_t i = 0; i < std::size(ids); ++i) {
        if(!getDiscoveredFeature(ids[i], *out[i], err)) {
            return false;
        }
    }
    return true;
}


bool LogitechHIDPP20Device::getDeviceTypeAndName(
    LogitechHIDPP20DeviceTypeInfo& info, std::string* err) {

    info = {};
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeatureDeviceTypeAndName, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ feature 0x0005 (Device Type And Name) is not supported";
        return false;
    }

    RawReport typeRaw{};
    std::size_t typeSize = 0;
    if(!requestLongRaw(feature.index, 0x02, nullptr, 0, typeRaw, typeSize, err)) {
        return false;
    }
    if(typeSize < 5) {
        if(err) *err = "HID++ 0x0005 GetDeviceType returned a short response";
        return false;
    }
    info.deviceType = typeRaw[4];

    RawReport countRaw{};
    std::size_t countSize = 0;
    if(!requestLongRaw(feature.index, 0x00, nullptr, 0,
                       countRaw, countSize, err)) {
        return false;
    }
    if(countSize < 5) {
        if(err) *err = "HID++ 0x0005 GetDeviceNameCount returned a short response";
        return false;
    }

    const std::size_t nameLength = countRaw[4];
    info.name.clear();
    info.name.reserve(nameLength);
    for(std::size_t offset = 0; offset < nameLength;) {
        const std::uint8_t param = static_cast<std::uint8_t>(offset);
        RawReport nameRaw{};
        std::size_t nameSize = 0;
        if(!requestLongRaw(feature.index, 0x01, &param, 1,
                           nameRaw, nameSize, err)) {
            return false;
        }
        if(nameSize <= 4) {
            if(err) *err = "HID++ 0x0005 GetDeviceName returned an empty response";
            return false;
        }

        const std::size_t available = nameSize - 4;
        const std::size_t take = std::min(available, nameLength - offset);
        for(std::size_t i = 0; i < take; ++i) {
            const char ch = static_cast<char>(nameRaw[4 + i]);
            if(ch != '\0') {
                info.name.push_back(ch);
            }
        }
        offset += take;
    }
    return true;
}

bool LogitechHIDPP20Device::getKeyboardLayout(
    LogitechHIDPP20KeyboardLayoutInfo& info, std::string* err) {

    info = {};

    // 0x4540 has a documented read-only getter whose first response byte is
    // Logitech's keyboard-layout country code. Despite overlapping values with
    // USB HID bCountryCode, the enumeration is Logitech-private; map it through
    // logitech_keyboard_layout.h. Prefer 0x4540 when present. 0x4520 is retained
    // in the feature catalogue, but its version-dependent wire format is not guessed.
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeatureKeyboardInternationalLayouts, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) {
            *err = "HID++ feature 0x4540 (Keyboard International Layouts) is not supported";
        }
        return false;
    }

    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(feature.index, 0x00, nullptr, 0, raw, rawSize, err)) {
        return false;
    }
    if(rawSize < 5) {
        if(err) *err = "HID++ 0x4540 GetLayout returned a short response";
        return false;
    }

    info.featureId = kFeatureKeyboardInternationalLayouts;
    info.countryCode = raw[4];
    return true;
}

bool LogitechHIDPP20Device::getBrightnessInfo(
    LogitechHIDPP20BrightnessInfo& info, std::string* err) {

    info = {};
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeatureBrightnessControl, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ feature 0x8040 (Brightness Control) is not supported";
        return false;
    }
    if(feature.version < 1) {
        if(err) *err = "HID++ 0x8040 version is older than the documented v1 getter format";
        return false;
    }

    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(feature.index, 0x00, nullptr, 0, raw, rawSize, err)) {
        return false;
    }
    if(rawSize < 11) {
        if(err) *err = "HID++ 0x8040 GetInfo returned a short response";
        return false;
    }

    // HID++ 0x8040 v1 encodes the 16-bit steps field split around caps/min.
    info.maximum = (static_cast<std::uint16_t>(raw[4]) << 8) | raw[5];
    info.steps = (static_cast<std::uint16_t>(raw[10]) << 8) | raw[6];
    info.capabilities = raw[7];
    info.minimum = (static_cast<std::uint16_t>(raw[8]) << 8) | raw[9];

    RawReport currentRaw{};
    std::size_t currentSize = 0;
    std::string ignored;
    if(requestLongRaw(feature.index, 0x01, nullptr, 0,
                      currentRaw, currentSize, &ignored) && currentSize >= 6) {
        info.current =
            (static_cast<std::uint16_t>(currentRaw[4]) << 8) | currentRaw[5];
        info.hasCurrent = true;
    }
    return true;
}

bool LogitechHIDPP20Device::getDisableKeysInfo(
    LogitechHIDPP20DisableKeysInfo& info, std::string* err) {

    info = {};
    bool found = false;

    LogitechHIDPP20FeatureInfo fixed;
    if(!getDiscoveredFeature(kFeatureDisableKeys, fixed, err)) {
        return false;
    }
    if(fixed.index != 0) {
        found = true;
        RawReport caps{};
        std::size_t capsSize = 0;
        if(!requestLongRaw(fixed.index, 0x00, nullptr, 0,
                           caps, capsSize, err)) {
            return false;
        }
        if(capsSize < 5) {
            if(err) *err = "HID++ 0x4521 GetCapabilities returned a short response";
            return false;
        }
        info.disableableMask = caps[4];

        RawReport state{};
        std::size_t stateSize = 0;
        std::string ignored;
        if(requestLongRaw(fixed.index, 0x01, nullptr, 0,
                          state, stateSize, &ignored) && stateSize >= 5) {
            info.disabledMask = state[4];
            info.hasDisabledMask = true;
        }
    }

    LogitechHIDPP20FeatureInfo byUsage;
    if(!getDiscoveredFeature(kFeatureDisableKeysByUsage, byUsage, err)) {
        return false;
    }
    if(byUsage.index != 0) {
        found = true;
        RawReport caps{};
        std::size_t capsSize = 0;
        if(!requestLongRaw(byUsage.index, 0x00, nullptr, 0,
                           caps, capsSize, err)) {
            return false;
        }
        if(capsSize < 5) {
            if(err) *err = "HID++ 0x4522 GetCapabilities returned a short response";
            return false;
        }
        info.maxDisabledUsages = caps[4];
    }

    if(!found) {
        if(err) *err = "HID++ keyboard-disable features are not supported";
        return false;
    }
    return true;
}

bool LogitechHIDPP20Device::getReprogrammableControls(
    LogitechHIDPP20ControlsInfo& info, std::string* err) {

    info = {};
    LogitechHIDPP20FeatureInfo feature;
    std::uint16_t featureId = 0;

    // Prefer the newest control table the device actually reports.
    for(int id = 0x1b04; id >= 0x1b00; --id) {
        LogitechHIDPP20FeatureInfo candidate;
        if(!getDiscoveredFeature(static_cast<std::uint16_t>(id), candidate, err)) {
            return false;
        }
        if(candidate.index != 0) {
            feature = candidate;
            featureId = static_cast<std::uint16_t>(id);
            break;
        }
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ reprogrammable-controls feature is not supported";
        return false;
    }

    RawReport countRaw{};
    std::size_t countSize = 0;
    if(!requestLongRaw(feature.index, 0x00, nullptr, 0,
                       countRaw, countSize, err)) {
        return false;
    }
    if(countSize < 5) {
        if(err) *err = "HID++ reprogrammable-controls GetCount returned a short response";
        return false;
    }

    info.featureId = featureId;
    const std::uint8_t count = countRaw[4];
    info.controls.reserve(count);

    for(std::uint16_t i = 0; i < count; ++i) {
        const std::uint8_t index = static_cast<std::uint8_t>(i);
        RawReport raw{};
        std::size_t rawSize = 0;
        if(!requestLongRaw(feature.index, 0x01, &index, 1,
                           raw, rawSize, err)) {
            return false;
        }
        if(rawSize < 9) {
            if(err) *err = "HID++ reprogrammable-controls GetCidInfo returned a short response";
            return false;
        }

        LogitechHIDPP20ControlInfo item;
        item.controlId = (static_cast<std::uint16_t>(raw[4]) << 8) | raw[5];
        item.taskId = (static_cast<std::uint16_t>(raw[6]) << 8) | raw[7];
        item.flags = raw[8];
        if(rawSize >= 13 && featureId == 0x1b04) {
            item.position = raw[9];
            item.group = raw[10];
            item.groupMask = raw[11];
            item.flags |= static_cast<std::uint16_t>(raw[12]) << 8;
        }
        info.controls.push_back(item);
    }
    return true;
}

bool LogitechHIDPP20Device::getColorLed8070Info(
    LogitechHIDPP20ColorLedInfo& info, std::string* err) {

    info = {};
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeatureColorLedEffects, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ feature 0x8070 (Color LED Effects) is not supported";
        return false;
    }

    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(feature.index, 0x00, nullptr, 0, raw, rawSize, err)) {
        return false;
    }
    if(rawSize < 9) {
        if(err) *err = "HID++ 0x8070 GetInfo returned a short response";
        return false;
    }

    info.zoneCount = raw[4];
    info.nvCapabilities =
        (static_cast<std::uint16_t>(raw[5]) << 8) | raw[6];
    info.extCapabilities =
        (static_cast<std::uint16_t>(raw[7]) << 8) | raw[8];
    info.zones.reserve(info.zoneCount);

    for(std::uint16_t z = 0; z < info.zoneCount; ++z) {
        const std::uint8_t zone = static_cast<std::uint8_t>(z);
        RawReport zoneRaw{};
        std::size_t zoneSize = 0;
        if(!requestLongRaw(feature.index, 0x01, &zone, 1,
                           zoneRaw, zoneSize, err)) {
            return false;
        }
        if(zoneSize < 9) {
            if(err) *err = "HID++ 0x8070 GetZoneInfo returned a short response";
            return false;
        }

        LogitechHIDPP20ColorLedZoneInfo zoneInfo;
        zoneInfo.zoneIndex = zoneRaw[4];
        zoneInfo.location =
            (static_cast<std::uint16_t>(zoneRaw[5]) << 8) | zoneRaw[6];
        zoneInfo.effectCount = zoneRaw[7];
        zoneInfo.persistencyCapabilities = zoneRaw[8];
        zoneInfo.effects.reserve(zoneInfo.effectCount);

        for(std::uint16_t e = 0; e < zoneInfo.effectCount; ++e) {
            const std::uint8_t params[2] = {
                zone, static_cast<std::uint8_t>(e)
            };
            RawReport effectRaw{};
            std::size_t effectSize = 0;
            if(!requestLongRaw(feature.index, 0x02, params, sizeof(params),
                               effectRaw, effectSize, err)) {
                return false;
            }
            if(effectSize < 12) {
                if(err) *err = "HID++ 0x8070 GetZoneEffectInfo returned a short response";
                return false;
            }

            zoneInfo.effects.push_back({
                effectRaw[4],
                effectRaw[5],
                static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(effectRaw[6]) << 8) |
                     effectRaw[7]),
                static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(effectRaw[8]) << 8) |
                     effectRaw[9]),
                static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(effectRaw[10]) << 8) |
                     effectRaw[11])
            });
        }
        info.zones.push_back(std::move(zoneInfo));
    }
    return true;
}

bool LogitechHIDPP20Device::getColorLed8070NvConfig(
    std::uint16_t capability,
    LogitechHIDPP20ColorLedNvConfig& config,
    std::string* err) {

    config = {};
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeatureColorLedEffects, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ feature 0x8070 (Color LED Effects) is not supported";
        return false;
    }

    const std::uint8_t params[2] = {
        static_cast<std::uint8_t>(capability >> 8),
        static_cast<std::uint8_t>(capability & 0xff),
    };
    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(feature.index, 0x04, params, sizeof(params),
                       raw, rawSize, err)) {
        return false;
    }
    if(rawSize < 9) {
        if(err) *err = "HID++ 0x8070 GetNvConfig returned a short response";
        return false;
    }

    config.capability =
        (static_cast<std::uint16_t>(raw[4]) << 8) | raw[5];
    config.state = raw[6];
    config.param1 = raw[7];
    config.param2 = raw[8];
    return true;
}

bool LogitechHIDPP20Device::getColorLed8070EffectSettings(
    std::uint8_t zoneIndex, std::uint8_t persistence,
    LogitechHIDPP20ColorLedEffectSettings& settings,
    std::string* err) {

    settings = {};
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeatureColorLedEffects, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ feature 0x8070 (Color LED Effects) is not supported";
        return false;
    }

    const std::uint8_t params[2] = {zoneIndex, persistence};
    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(feature.index, 0x09, params, sizeof(params),
                       raw, rawSize, err)) {
        return false;
    }
    if(rawSize < 12) {
        if(err) *err = "HID++ 0x8070 GetEffectSettings returned a short response";
        return false;
    }

    settings.zoneIndex = raw[4];
    settings.persistence = persistence;
    settings.r = raw[5];
    settings.g = raw[6];
    settings.b = raw[7];
    settings.periodMs =
        (static_cast<std::uint16_t>(raw[8]) << 8) | raw[9];
    settings.brightness = raw[10];
    settings.effectParam = raw[11];
    return true;
}

bool LogitechHIDPP20Device::getColorLed8070ZoneEffect(
    std::uint8_t zoneIndex, std::uint8_t persistence,
    LogitechHIDPP20ColorLedZoneEffectState& state,
    std::string* err) {

    state = {};
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeatureColorLedEffects, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ feature 0x8070 (Color LED Effects) is not supported";
        return false;
    }

    const std::uint8_t params[2] = {zoneIndex, persistence};
    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(feature.index, 0x0e, params, sizeof(params),
                       raw, rawSize, err)) {
        return false;
    }
    if(rawSize < 16) {
        if(err) *err = "HID++ 0x8070 GetZoneEffect returned a short response";
        return false;
    }

    state.zoneIndex = raw[4];
    state.persistence = persistence;
    state.effectIndex = raw[5];
    for(std::size_t i = 0; i < state.params.size(); ++i) {
        state.params[i] = raw[6 + i];
    }
    return true;
}

bool LogitechHIDPP20Device::getPerKey8081Info(
    LogitechHIDPP20PerKey8081Info& info, std::string* err) {

    info = {};
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeaturePerKeyLighting2, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ feature 0x8081 (Per Key Lighting 2) is not supported";
        return false;
    }

    for(std::uint16_t page = 0; page < 3; ++page) {
        const std::uint8_t params[3] = {
            0x00, static_cast<std::uint8_t>(page), 0x00
        };
        RawReport raw{};
        std::size_t rawSize = 0;
        if(!requestLongRaw(feature.index, 0x00, params, sizeof(params),
                           raw, rawSize, err)) {
            return false;
        }
        if(rawSize < 20) {
            if(err) *err = "HID++ 0x8081 zone-presence response is too short";
            return false;
        }

        for(std::uint16_t byte = 0; byte < 14; ++byte) {
            const std::uint8_t bits = raw[6 + byte];
            for(std::uint16_t bit = 0; bit < 8; ++bit) {
                if((bits & (1u << bit)) == 0) {
                    continue;
                }
                const std::uint16_t zoneId = page * 112u + byte * 8u + bit;
                if(zoneId <= 255u) {
                    info.zoneIds.push_back(static_cast<std::uint8_t>(zoneId));
                }
            }
        }
    }
    return true;
}

bool LogitechHIDPP20Device::getRgbEffects8071Info(
    LogitechHIDPP20RgbEffectsInfo& info, std::string* err) {

    info = {};
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeatureRgbEffects, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ feature 0x8071 (RGB Effects) is not supported";
        return false;
    }

    const std::uint8_t deviceParams[3] = {0xff, 0xff, 0x00};
    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(feature.index, 0x00, deviceParams, sizeof(deviceParams),
                       raw, rawSize, err)) {
        return false;
    }
    if(rawSize < 12) {
        if(err) *err = "HID++ 0x8071 GetInfo(device) returned a short response";
        return false;
    }

    info.clusterCount = raw[6];
    info.nvCapabilities =
        (static_cast<std::uint16_t>(raw[7]) << 8) | raw[8];
    info.extCapabilities =
        (static_cast<std::uint16_t>(raw[9]) << 8) | raw[10];
    info.multiClusterEffectCount = raw[11];
    info.clusters.reserve(info.clusterCount);

    for(std::uint16_t cidx = 0; cidx < info.clusterCount; ++cidx) {
        const std::uint8_t clusterParams[3] = {
            static_cast<std::uint8_t>(cidx), 0xff, 0x00
        };
        RawReport clusterRaw{};
        std::size_t clusterSize = 0;
        if(!requestLongRaw(feature.index, 0x00,
                           clusterParams, sizeof(clusterParams),
                           clusterRaw, clusterSize, err)) {
            return false;
        }
        if(clusterSize < 12) {
            if(err) *err = "HID++ 0x8071 GetInfo(cluster) returned a short response";
            return false;
        }

        LogitechHIDPP20RgbClusterInfo cluster;
        cluster.clusterIndex = clusterRaw[4];
        cluster.location =
            (static_cast<std::uint16_t>(clusterRaw[6]) << 8) | clusterRaw[7];
        cluster.effectCount = clusterRaw[8];
        cluster.displayPersistencyCapabilities = clusterRaw[9];
        cluster.effectPersistency = clusterRaw[10] != 0;
        cluster.multiLedPattern = clusterRaw[11] != 0;
        cluster.effects.reserve(cluster.effectCount);

        for(std::uint16_t e = 0; e < cluster.effectCount; ++e) {
            const std::uint8_t effectParams[3] = {
                static_cast<std::uint8_t>(cidx),
                static_cast<std::uint8_t>(e),
                0x00
            };
            RawReport effectRaw{};
            std::size_t effectSize = 0;
            if(!requestLongRaw(feature.index, 0x00,
                               effectParams, sizeof(effectParams),
                               effectRaw, effectSize, err)) {
                return false;
            }
            if(effectSize < 12) {
                if(err) *err = "HID++ 0x8071 GetInfo(effect) returned a short response";
                return false;
            }

            cluster.effects.push_back({
                effectRaw[4],
                effectRaw[5],
                static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(effectRaw[6]) << 8) |
                     effectRaw[7]),
                static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(effectRaw[8]) << 8) |
                     effectRaw[9]),
                static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(effectRaw[10]) << 8) |
                     effectRaw[11])
            });
        }

        info.clusters.push_back(std::move(cluster));
    }

    return true;
}

bool LogitechHIDPP20Device::getReportRateInfo(
    LogitechHIDPP20ReportRateInfo& info, std::string* err) {

    info = {};

    LogitechHIDPP20FeatureInfo extended;
    if(!getDiscoveredFeature(kFeatureExtendedAdjustableReportRate, extended, err)) {
        return false;
    }
    if(extended.index != 0) {
        RawReport raw{};
        std::size_t rawSize = 0;
        if(!requestLongRaw(extended.index, 0x01, nullptr, 0,
                           raw, rawSize, err)) {
            return false;
        }
        if(rawSize < 6) {
            if(err) *err = "HID++ 0x8061 GetActualReportRateList returned a short response";
            return false;
        }
        info.supportedMask =
            (static_cast<std::uint16_t>(raw[4]) << 8) | raw[5];
        return true;
    }

    LogitechHIDPP20FeatureInfo legacy;
    if(!getDiscoveredFeature(kFeatureAdjustableReportRate, legacy, err)) {
        return false;
    }
    if(legacy.index == 0) {
        if(err) *err = "HID++ report-rate feature is not supported";
        return false;
    }

    RawReport listRaw{};
    std::size_t listSize = 0;
    if(!requestLongRaw(legacy.index, 0x00, nullptr, 0,
                       listRaw, listSize, err)) {
        return false;
    }
    if(listSize < 5) {
        if(err) *err = "HID++ 0x8060 GetReportRateList returned a short response";
        return false;
    }
    info.supportedMask = listRaw[4];

    RawReport currentRaw{};
    std::size_t currentSize = 0;
    std::string ignored;
    if(requestLongRaw(legacy.index, 0x01, nullptr, 0,
                      currentRaw, currentSize, &ignored) && currentSize >= 5) {
        info.current = currentRaw[4];
        info.hasCurrent = true;
    }
    return true;
}

bool LogitechHIDPP20Device::getModeStatusInfo(
    LogitechHIDPP20ModeStatusInfo& info, std::string* err) {

    info = {};
    LogitechHIDPP20FeatureInfo feature;
    if(!getDiscoveredFeature(kFeatureModeStatus, feature, err)) {
        return false;
    }
    if(feature.index == 0) {
        if(err) *err = "HID++ feature 0x8090 (Mode Status) is not supported";
        return false;
    }
    if(feature.version < 1) {
        if(err) *err = "HID++ 0x8090 version is older than the documented v1 getter format";
        return false;
    }

    RawReport statusRaw{};
    std::size_t statusSize = 0;
    if(!requestLongRaw(feature.index, 0x00, nullptr, 0,
                       statusRaw, statusSize, err)) {
        return false;
    }
    if(statusSize < 6) {
        if(err) *err = "HID++ 0x8090 GetModeStatus returned a short response";
        return false;
    }
    info.status0 = statusRaw[4];
    info.status1 = statusRaw[5];

    RawReport capsRaw{};
    std::size_t capsSize = 0;
    if(!requestLongRaw(feature.index, 0x02, nullptr, 0,
                       capsRaw, capsSize, err)) {
        return false;
    }
    if(capsSize < 6) {
        if(err) *err = "HID++ 0x8090 GetDeviceConfig returned a short response";
        return false;
    }
    info.capabilities =
        (static_cast<std::uint16_t>(capsRaw[4]) << 8) | capsRaw[5];
    return true;
}

bool LogitechHIDPP20Device::probeKeyboard(std::string* err) {
    LogitechHIDPP20Capabilities capabilities;
    if(!getCapabilities(capabilities, err)) {
        return false;
    }

    LogitechHIDPP20LightingFeatures features;
    if(!getLightingFeatures(features, err)) {
        return false;
    }
    if(!features.hasKnownLightingFeature()) {
        if(err) {
            *err = "HID++ 2.0 endpoint reports no known keyboard lighting feature";
        }
        return false;
    }

    // When 0x8080 is present, verify that the feature actually exposes the
    // keyboard key type before accepting it as a per-key keyboard endpoint.
    if(features.perKey8080.index != 0) {
        LogitechHIDPP20PerKeyInfo info;
        if(!getPerKey8080Info(info, err)) {
            return false;
        }
        if((info.typeFlags & 0x0001) != 0) {
            return true;
        }
    }

    // Prefer device-reported identity when 0x0005 is available. DeviceType 0
    // is a keyboard, so an unknown Logitech PID can still be accepted without a
    // model table entry.
    if(capabilities.findFeature(kFeatureDeviceTypeAndName) != nullptr) {
        LogitechHIDPP20DeviceTypeInfo typeInfo;
        std::string typeError;
        if(getDeviceTypeAndName(typeInfo, &typeError) &&
           typeInfo.deviceType == 0x00) {
            return true;
        }
    }

    // Older endpoints may not expose 0x0005. For feature families that do not
    // provide a parsed keyboard topology, retain the known-PID fallback to avoid
    // treating an RGB mouse/headset as a keyboard.
    if(logitechKnownDeviceForProductId(usbIdentity_.productId) != nullptr) {
        return true;
    }

    if(err) {
        *err = "lighting features found, but endpoint is not proven to be a keyboard";
    }
    return false;
}

bool LogitechHIDPP20Device::openKeyboard(std::string* err) {
    close();

    const fs::path base = "/sys/class/hidraw";
    std::error_code ec;
    if(!fs::is_directory(base, ec)) {
        if(err) {
            *err = "hidraw sysfs class is unavailable";
        }
        return false;
    }

    std::vector<std::string> names;
    for(const auto& entry : fs::directory_iterator(base, ec)) {
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());

    std::string lastProbeError;
    for(const auto& name : names) {
        const fs::path devlink = base / name / "device";

        int vid = -1;
        int pid = -1;
        std::string hidName;
        if(!parseHidUevent(devlink, vid, pid, hidName) ||
           vid != kVendorId) {
            continue;
        }

        LogitechUsbIdentity identity;
        identity.vendorId = static_cast<std::uint16_t>(vid);
        identity.productId = static_cast<std::uint16_t>(pid);
        identity.hidName = hidName;

        const fs::path real = fs::canonical(devlink, ec);
        if(!ec) {
            fillUsbIdentity(real, identity);
        } else {
            ec.clear();
        }

        std::string openError;
        const std::string devicePath = std::string("/dev/") + name;
        if(!openPath(devicePath, &openError)) {
            lastProbeError = openError;
            continue;
        }

        usbIdentity_ = identity;

        std::string probeError;
        if(probeKeyboard(&probeError)) {
            return true;
        }

        lastProbeError = displayName() + ": " + probeError;
        close();
    }

    if(err) {
        *err = "No Logitech HID++ 2.0 lighting keyboard found";
        if(!lastProbeError.empty()) {
            *err += " (last probe: " + lastProbeError + ")";
        }
    }
    return false;
}

bool LogitechHIDPP20Device::openPath(const std::string& devicePath, std::string* err) {
    close();

    const int fd = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if(fd < 0) {
        if(err) {
            *err = "open " + devicePath + ": " + std::strerror(errno);
        }
        return false;
    }

    fd_ = fd;
    path_ = devicePath;
    return true;
}

void LogitechHIDPP20Device::close() {
    if(fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    path_.clear();
    usbIdentity_ = {};
    featureSetEnumerated_ = false;
    discoveredFeatures_.clear();
}

void LogitechHIDPP20Device::normalizeLightingColor(
    std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) const {
    if(!isMonochrome()) {
        return;
    }

    // G610 has white LEDs. Its HID++ payload still has three color bytes, but
    // the established G610 protocol representation uses the first byte as
    // intensity and leaves the other two at zero. Collapse arbitrary RGB input
    // to a single brightness so the common API remains usable.
    const std::uint8_t intensity = std::max({r, g, b});
    r = intensity;
    g = 0;
    b = 0;
}

bool LogitechHIDPP20Device::requestLongRaw(
    std::uint8_t featureIndex, std::uint8_t function,
    const std::uint8_t* params, std::size_t paramCount,
    RawReport& response, std::size_t& responseSize,
    std::string* err) {

    response.fill(0);
    responseSize = 0;

    if(fd_ < 0 || paramCount > 16) {
        if(err) {
            *err = "invalid HID++ request";
        }
        return false;
    }

    LongReport request{};
    request[0] = kLongReportId;
    request[1] = kDefaultDeviceIndex;
    request[2] = featureIndex;
    request[3] = static_cast<std::uint8_t>((function << 4) | kSoftwareId);
    for(std::size_t i = 0; i < paramCount; ++i) {
        request[4 + i] = params[i];
    }

    // Drop stale asynchronous reports before issuing a synchronous request.
    std::uint8_t stale[64];
    while(::read(fd_, stale, sizeof(stale)) > 0) {
    }

    const ssize_t written = ::write(fd_, request.data(), request.size());
    if(written != static_cast<ssize_t>(request.size())) {
        if(err) {
            *err = "HID++ write failed: " + std::string(std::strerror(errno));
        }
        return false;
    }

    pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;

    // Long requests can legitimately answer with a 0x12 very-long frame
    // (notably 0x8080 GetKeyColors), so preserve the complete raw response.
    for(int attempt = 0; attempt < 8; ++attempt) {
        const int ready = ::poll(&pfd, 1, 125);
        if(ready < 0) {
            if(errno == EINTR) {
                continue;
            }
            if(err) {
                *err = "HID++ poll failed: " + std::string(std::strerror(errno));
            }
            return false;
        }
        if(ready == 0) {
            continue;
        }

        std::uint8_t buffer[64]{};
        const ssize_t count = ::read(fd_, buffer, sizeof(buffer));
        if(count < 4) {
            continue;
        }

        const std::uint8_t reportId = buffer[0];
        if(reportId != kShortReportId &&
           reportId != kLongReportId &&
           reportId != kVeryLongReportId) {
            continue;
        }
        if(buffer[1] != kDefaultDeviceIndex) {
            continue;
        }

        if(buffer[2] != featureIndex ||
           (buffer[3] & 0x0f) != kSoftwareId ||
           (buffer[3] >> 4) != function) {
            continue;
        }

        responseSize = std::min<std::size_t>(
            response.size(), static_cast<std::size_t>(count));
        std::copy(buffer, buffer + responseSize, response.begin());
        return true;
    }

    if(err) {
        *err = "HID++ request timed out";
    }
    return false;
}

bool LogitechHIDPP20Device::requestLong(
    std::uint8_t featureIndex, std::uint8_t function,
    const std::uint8_t* params, std::size_t paramCount,
    LongReport& response, std::string* err) {

    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(featureIndex, function, params, paramCount,
                       raw, rawSize, err)) {
        return false;
    }

    response.fill(0);
    const std::size_t copyCount = std::min<std::size_t>(response.size(), rawSize);
    std::copy(raw.begin(), raw.begin() + copyCount, response.begin());
    return true;
}

bool LogitechHIDPP20Device::writeVeryLong(
    std::uint8_t featureIndex, std::uint8_t function,
    const std::uint8_t* params, std::size_t paramCount,
    std::string* err) {

    if(fd_ < 0 || paramCount > 60) {
        if(err) {
            *err = "invalid HID++ very-long request";
        }
        return false;
    }

    VeryLongReport request{};
    request[0] = kVeryLongReportId;
    request[1] = kDefaultDeviceIndex;
    request[2] = featureIndex;
    request[3] = static_cast<std::uint8_t>((function << 4) | kSoftwareId);
    for(std::size_t i = 0; i < paramCount; ++i) {
        request[4 + i] = params[i];
    }

    const ssize_t written = ::write(fd_, request.data(), request.size());
    if(written != static_cast<ssize_t>(request.size())) {
        if(err) {
            *err = "HID++ very-long write failed: " + std::string(std::strerror(errno));
        }
        return false;
    }
    return true;
}

bool LogitechHIDPP20Device::getProtocolInfo(
    LogitechHIDPP20ProtocolInfo& info, std::string* err) {

    info = {};
    constexpr std::uint8_t kPing = 0xa5;
    const std::uint8_t params[3] = {0x00, 0x00, kPing};

    LongReport response{};
    if(!requestLong(0x00, 0x01, params, sizeof(params), response, err)) {
        return false;
    }
    if(response[6] != kPing) {
        if(err) {
            *err = "HID++ protocol-version ping mismatch";
        }
        return false;
    }

    info.protocolNumber = response[4];
    info.targetSoftware = response[5];
    info.pingData = response[6];
    return true;
}

bool LogitechHIDPP20Device::getProtocolVersion(
    std::uint8_t& protocolNumber, std::uint8_t& targetSoftware,
    std::string* err) {

    LogitechHIDPP20ProtocolInfo info;
    if(!getProtocolInfo(info, err)) {
        return false;
    }
    protocolNumber = info.protocolNumber;
    targetSoftware = info.targetSoftware;
    return true;
}

bool LogitechHIDPP20Device::getPerKey8080Info(
    LogitechHIDPP20PerKeyInfo& info, std::string* err) {

    info = {};

    LogitechHIDPP20FeatureInfo perKey;
    if(!getDiscoveredFeature(kFeaturePerKeyLighting, perKey, err)) {
        return false;
    }
    if(perKey.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8080 (Per Key Lighting) is not supported";
        }
        return false;
    }

    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(perKey.index, 0x00, nullptr, 0, raw, rawSize, err)) {
        return false;
    }
    if(rawSize < 11) {
        if(err) {
            *err = "HID++ 0x8080 GetInfo returned a short response";
        }
        return false;
    }

    info.typeFlags =
        (static_cast<std::uint16_t>(raw[4]) << 8) |
         static_cast<std::uint16_t>(raw[5]);
    info.keyTypeCount =
        (static_cast<std::uint16_t>(raw[7]) << 8) |
         static_cast<std::uint16_t>(raw[8]);
    info.maxKeyCount =
        (static_cast<std::uint16_t>(raw[9]) << 8) |
         static_cast<std::uint16_t>(raw[10]);

    // typeFlags is the device's authoritative key-type bitmap. Query every
    // set bit rather than limiting discovery to types already known to k-rgb.
    for(std::uint32_t bit = 1; bit <= 0x8000; bit <<= 1) {
        const std::uint16_t keyType = static_cast<std::uint16_t>(bit);
        if((info.typeFlags & keyType) == 0) {
            continue;
        }

        LogitechHIDPP20PerKeyTypeInfo typeInfo;
        typeInfo.keyType = keyType;

        const std::uint8_t typeQuery[2] = {
            static_cast<std::uint8_t>(keyType >> 8),
            static_cast<std::uint8_t>(keyType & 0xff),
        };
        RawReport typeRaw{};
        std::size_t typeRawSize = 0;
        if(!requestLongRaw(perKey.index, 0x01,
                           typeQuery, sizeof(typeQuery),
                           typeRaw, typeRawSize, err)) {
            return false;
        }
        if(typeRawSize < 6) {
            if(err) {
                *err = "HID++ 0x8080 GetKeyTypeInfo returned a short response";
            }
            return false;
        }

        typeInfo.keyCount =
            (static_cast<std::uint16_t>(typeRaw[4]) << 8) |
             static_cast<std::uint16_t>(typeRaw[5]);

        // Function 2 GetKeyColors pages up to 14 (keyId,R,G,B) entries.
        // Treat returned IDs as address candidates, not as a physical-key
        // count: hardware can expose a superset of layout-dependent IDs.
        // The response payload starts with a 4-byte function-specific header.
        std::uint16_t startIndex = 0;
        std::size_t guardPages = typeInfo.keyCount
            ? (static_cast<std::size_t>(typeInfo.keyCount) + 13) / 14
            : 16;
        if(guardPages == 0) {
            guardPages = 1;
        }

        for(std::size_t page = 0; page < guardPages; ++page) {
            const std::uint8_t colorQuery[5] = {
                static_cast<std::uint8_t>(keyType >> 8),
                static_cast<std::uint8_t>(keyType & 0xff),
                static_cast<std::uint8_t>(startIndex >> 8),
                static_cast<std::uint8_t>(startIndex & 0xff),
                0x00, // volatile/default persistence
            };

            RawReport colorRaw{};
            std::size_t colorRawSize = 0;
            if(!requestLongRaw(perKey.index, 0x02,
                               colorQuery, sizeof(colorQuery),
                               colorRaw, colorRawSize, err)) {
                return false;
            }
            if(colorRawSize <= 8) {
                break;
            }

            const std::size_t entries = (colorRawSize - 8) / 4;
            std::size_t found = 0;
            for(std::size_t e = 0; e < entries; ++e) {
                const std::size_t pos = 8 + e * 4;
                const std::uint8_t keyId = colorRaw[pos];
                if(keyId == 0) {
                    continue;
                }

                typeInfo.colors.push_back({
                    keyId,
                    colorRaw[pos + 1],
                    colorRaw[pos + 2],
                    colorRaw[pos + 3],
                });
                ++found;
            }

            if(found == 0 ||
               (typeInfo.keyCount &&
                typeInfo.colors.size() >= typeInfo.keyCount)) {
                break;
            }
            startIndex = static_cast<std::uint16_t>(startIndex + 14);
        }

        info.types.push_back(std::move(typeInfo));
    }

    return true;
}

bool LogitechHIDPP20Device::getColorLed8070ZoneCount(
    std::uint8_t& zoneCount, std::string* err) {

    zoneCount = 0;

    LogitechHIDPP20FeatureInfo fx;
    if(!getDiscoveredFeature(kFeatureColorLedEffects, fx, err)) {
        return false;
    }
    if(fx.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8070 (Color LED Effects) is not supported";
        }
        return false;
    }

    LongReport response{};
    if(!requestLong(fx.index, 0x00, nullptr, 0, response, err)) {
        return false;
    }

    zoneCount = response[4];
    if(zoneCount == 0) {
        if(err) {
            *err = "HID++ 0x8070 reported zero lighting zones";
        }
        return false;
    }
    return true;
}

bool LogitechHIDPP20Device::setColorLed8070Zones(
    const std::vector<LogitechHIDPP20ZoneColor>& colors,
    std::string* err) {

    if(colors.empty()) {
        if(err) {
            *err = "HID++ 0x8070 zone color list is empty";
        }
        return false;
    }

    LogitechHIDPP20FeatureInfo fx;
    if(!getDiscoveredFeature(kFeatureColorLedEffects, fx, err)) {
        return false;
    }
    if(fx.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8070 (Color LED Effects) is not supported";
        }
        return false;
    }

    std::uint8_t zoneCount = 0;
    if(!getColorLed8070ZoneCount(zoneCount, err)) {
        return false;
    }

    std::uint8_t addressBase = 0;
    if(const auto* known = logitechKnownDeviceForProductId(productId())) {
        addressBase = known->colorLed8070ZoneAddressBase;
    }

    for(const auto& item : colors) {
        if(item.zone >= zoneCount) {
            if(err) {
                *err = "HID++ 0x8070 logical zone " +
                       std::to_string(static_cast<unsigned>(item.zone)) +
                       " is outside the device-reported range";
            }
            return false;
        }

        std::uint8_t r = item.r;
        std::uint8_t g = item.g;
        std::uint8_t b = item.b;
        normalizeLightingColor(r, g, b);

        // 0x8070 SetZoneEffect (function 3):
        //   zone, mode=fixed/on (0x01), RGB, effect=0, remaining effect data.
        // G213 captures independently show exactly this fixed-color payload,
        // with physical region IDs 1..5 rather than the usual zero-based
        // zone indices. The runtime feature index is still discovered.
        std::uint8_t params[16]{};
        params[0] = static_cast<std::uint8_t>(item.zone + addressBase);
        params[1] = 0x01;
        params[2] = r;
        params[3] = g;
        params[4] = b;
        params[5] = 0x00;
        params[12] = 0x00; // volatile/RAM; do not write onboard flash

        LongReport response{};
        if(!requestLong(fx.index, 0x03, params, sizeof(params), response, err)) {
            return false;
        }
    }

    return true;
}

bool LogitechHIDPP20Device::setColorLed8070Effect(
    std::uint16_t effectId,
    std::uint8_t r, std::uint8_t g, std::uint8_t b,
    std::uint16_t periodMs,
    std::uint8_t intensity,
    std::uint8_t direction,
    std::string* err) {

    LogitechHIDPP20ColorLedInfo info;
    if(!getColorLed8070Info(info, err)) {
        return false;
    }

    LogitechHIDPP20FeatureInfo fx;
    if(!getDiscoveredFeature(kFeatureColorLedEffects, fx, err)) {
        return false;
    }
    if(fx.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8070 (Color LED Effects) is not supported";
        }
        return false;
    }

    normalizeLightingColor(r, g, b);
    intensity = static_cast<std::uint8_t>(std::min<unsigned>(intensity, 100u));

    bool wroteAny = false;
    for(const auto& zone : info.zones) {
        const LogitechHIDPP20ColorLedEffectInfo* chosen = nullptr;
        for(const auto& effect : zone.effects) {
            if(effect.effectId == effectId) {
                chosen = &effect;
                break;
            }
        }

        // Effects can legitimately be advertised on only part of a device.
        // G810 wave/starlight are available on the primary keyboard zone but
        // not on the separate logo zone. Apply to every zone that advertises
        // the effect instead of rejecting the whole operation.
        if(!chosen) {
            continue;
        }

        // x8070 SetZoneEffect:
        // zoneIndex, zoneEffectIndex, param1..param10, persistence.
        std::uint8_t params[13]{};
        params[0] = zone.zoneIndex;
        params[1] = chosen->effectIndex;

        switch(effectId) {
            case 0x000a: // Pulsing / Breathing (waveform)
                params[2] = r;
                params[3] = g;
                params[4] = b;
                params[5] = static_cast<std::uint8_t>(periodMs >> 8);
                params[6] = static_cast<std::uint8_t>(periodMs & 0xff);
                params[7] = 0x00; // device-default waveform
                params[8] = intensity;
                break;

            case 0x0003: // Color Cycling / Spectrum
                params[7] = static_cast<std::uint8_t>(periodMs >> 8);
                params[8] = static_cast<std::uint8_t>(periodMs & 0xff);
                params[9] = intensity;
                break;

            case 0x0004: // Color Wave
                // Leave start/stop RGB at zero. On the G810 this selects the
                // firmware's full-colour wave, matching the "all play"
                // default from the 0x8070 effect definition.
                params[8] = static_cast<std::uint8_t>(periodMs & 0xff);
                params[9] = direction;
                params[10] = intensity == 0 ? 1 : intensity;
                params[11] = static_cast<std::uint8_t>(periodMs >> 8);
                break;

            case 0x0005: // Starlight / LGS "Star Effect"
                // param1..3 = sky RGB, param4..6 = star RGB.
                // Match the G810 LGS PerKeyLightingDefaults resource:
                //   star/sky/color  = #000019
                //   star/star/color = selected GUI colour (default #ffff00)
                params[2] = 0x00;
                params[3] = 0x00;
                params[4] = 0x19;
                params[5] = r;
                params[6] = g;
                params[7] = b;
                break;

            default:
                if(err) {
                    char buffer[96]{};
                    std::snprintf(buffer, sizeof(buffer),
                                  "HID++ 0x8070 effect 0x%04x is not implemented",
                                  static_cast<unsigned>(effectId));
                    *err = buffer;
                }
                return false;
        }

        params[12] = 0x00; // volatile/RAM only
        LongReport response{};
        if(!requestLong(fx.index, 0x03, params, sizeof(params), response, err)) {
            return false;
        }
        wroteAny = true;
    }

    if(!wroteAny) {
        if(err) {
            char buffer[96]{};
            std::snprintf(buffer, sizeof(buffer),
                          "HID++ 0x8070 does not advertise effect 0x%04x on any zone",
                          static_cast<unsigned>(effectId));
            *err = buffer;
        }
        return false;
    }

    return true;
}

bool LogitechHIDPP20Device::setSolid(
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::string* err) {

    normalizeLightingColor(r, g, b);

    LogitechHIDPP20LightingFeatures features;
    if(!getLightingFeatures(features, err)) {
        return false;
    }

    bool wroteLighting = false;

    if(features.colorLedEffects8070.index != 0) {
        std::uint8_t zoneCount = 0;
        if(!getColorLed8070ZoneCount(zoneCount, err)) {
            return false;
        }

        std::vector<LogitechHIDPP20ZoneColor> zones;
        zones.reserve(zoneCount);
        for(std::uint8_t zone = 0; zone < zoneCount; ++zone) {
            zones.push_back({zone, r, g, b});
        }
        if(!setColorLed8070Zones(zones, err)) {
            return false;
        }
        wroteLighting = true;
    }

    if(features.perKey8080.index != 0) {
        LogitechHIDPP20PerKeyInfo topology;
        if(!getPerKey8080Info(topology, err)) {
            return false;
        }

        // 0x8080 GetKeyColors can expose a layout-independent candidate
        // address superset. For G610/G810 models with a known LGS geometry,
        // intersect candidates with the physical model/layout before normal
        // bulk writes. Low-level explicit per-address writes remain unfiltered
        // for diagnostics and reverse engineering.
        const logitech::g610_g810::KeyboardGeometry* physicalGeometry = nullptr;
        std::uint8_t modelMask = 0;
        if(const auto* known = logitechKnownDeviceForProductId(productId())) {
            switch(known->model) {
                case LogitechKnownModel::G610Orion:
                    modelMask = logitech::g610_g810::kModelG610;
                    break;
                case LogitechKnownModel::G810OrionSpectrum:
                    modelMask = logitech::g610_g810::kModelG810;
                    break;
                default:
                    break;
            }
        }

        if(modelMask != 0) {
            LogitechHIDPP20KeyboardLayoutInfo layoutInfo;
            std::string layoutError;
            if(getKeyboardLayout(layoutInfo, &layoutError)) {
                physicalGeometry =
                    logitech::g610_g810::inferGeometryForLayout(
                        modelMask, layoutInfo.countryCode);
            }
        }

        for(const auto& type : topology.types) {
            if(type.colors.empty()) {
                continue;
            }

            std::vector<LogitechHIDPP20KeyColor> colors;
            colors.reserve(type.colors.size());
            for(const auto& item : type.colors) {
                bool include = true;
                if(physicalGeometry) {
                    switch(type.keyType) {
                        case logitech::g610_g810::kKeyboardKeyType:
                        case logitech::g610_g810::kMediaKeyType:
                        case logitech::g610_g810::kLogoKeyType:
                        case logitech::g610_g810::kIndicatorKeyType:
                            include =
                                logitech::g610_g810::isPhysicalLightingAddress(
                                    *physicalGeometry,
                                    type.keyType, item.keyId);
                            break;
                        default:
                            // Unknown key types are not discarded merely
                            // because the G610/G810 resource map has no legend.
                            break;
                    }
                }

                if(include) {
                    colors.push_back({item.keyId, r, g, b});
                }
            }

            if(colors.empty()) {
                continue;
            }
            if(!setPerKey8080Colors(type.keyType, colors, err)) {
                return false;
            }
            wroteLighting = true;
        }
    }

    if(!wroteLighting) {
        if(err) {
            *err = "No implemented solid-color path for this device's "
                   "discovered HID++ lighting features";
        }
        return false;
    }

    return true;
}

bool LogitechHIDPP20Device::setPerKey8080Color(
    std::uint16_t keyType, std::uint8_t keyId,
    std::uint8_t r, std::uint8_t g, std::uint8_t b,
    std::string* err) {

    normalizeLightingColor(r, g, b);

    LogitechHIDPP20FeatureInfo perKey;
    if(!getDiscoveredFeature(kFeaturePerKeyLighting, perKey, err)) {
        return false;
    }
    if(perKey.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8080 (Per Key Lighting) is not supported";
        }
        return false;
    }

    // Function 3: SetKeyColors. Payload is keyType (BE16), count (BE16),
    // then (keyId, R, G, B) tuples in a 0x12 64-byte report.
    std::uint8_t payload[8] = {
        static_cast<std::uint8_t>(keyType >> 8),
        static_cast<std::uint8_t>(keyType & 0xff),
        0x00, 0x01,
        keyId, r, g, b,
    };
    if(!writeVeryLong(perKey.index, 0x03, payload, sizeof(payload), err)) {
        return false;
    }

    // Function 5: FlushLEDs. Empty long request commits the staged colors.
    LongReport response{};
    if(!requestLong(perKey.index, 0x05, nullptr, 0, response, err)) {
        return false;
    }

    return true;
}

bool LogitechHIDPP20Device::setPerKey8080Colors(
    std::uint16_t keyType,
    const std::vector<LogitechHIDPP20KeyColor>& colors,
    std::string* err) {

    if(colors.empty()) {
        if(err) {
            *err = "HID++ 0x8080 color group must contain at least one entry";
        }
        return false;
    }

    LogitechHIDPP20FeatureInfo perKey;
    if(!getDiscoveredFeature(kFeaturePerKeyLighting, perKey, err)) {
        return false;
    }
    if(perKey.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8080 (Per Key Lighting) is not supported";
        }
        return false;
    }

    // One HID++ very-long report can carry at most 14 (keyId,R,G,B) tuples.
    // Stage larger logical groups as consecutive SetKeyColors calls and flush
    // only once after the final chunk so they become one committed frame.
    constexpr std::size_t kMaxColorsPerReport = 14;
    for(std::size_t start = 0; start < colors.size();
        start += kMaxColorsPerReport) {

        const std::size_t count =
            std::min<std::size_t>(kMaxColorsPerReport, colors.size() - start);

        std::uint8_t payload[60]{};
        payload[0] = static_cast<std::uint8_t>(keyType >> 8);
        payload[1] = static_cast<std::uint8_t>(keyType & 0xff);
        payload[2] = 0x00;
        payload[3] = static_cast<std::uint8_t>(count);

        std::size_t pos = 4;
        for(std::size_t i = 0; i < count; ++i) {
            const auto& color = colors[start + i];
            std::uint8_t r = color.r;
            std::uint8_t g = color.g;
            std::uint8_t b = color.b;
            normalizeLightingColor(r, g, b);
            payload[pos++] = color.keyId;
            payload[pos++] = r;
            payload[pos++] = g;
            payload[pos++] = b;
        }

        if(!writeVeryLong(perKey.index, 0x03, payload, pos, err)) {
            return false;
        }
    }

    LongReport response{};
    if(!requestLong(perKey.index, 0x05, nullptr, 0, response, err)) {
        return false;
    }

    return true;
}

bool LogitechHIDPP20Device::getFirmwareInfo(
    std::vector<LogitechHIDPP20FirmwareInfo>& firmware,
    std::string* err) {

    firmware.clear();

    LogitechHIDPP20FeatureInfo devInfo;
    if(!getDiscoveredFeature(kFeatureDeviceInformation, devInfo, err)) {
        return false;
    }
    if(devInfo.index == 0) {
        if(err) {
            *err = "HID++ feature 0x0003 (Device Information) is not supported";
        }
        return false;
    }

    RawReport raw{};
    std::size_t rawSize = 0;
    if(!requestLongRaw(devInfo.index, 0x00, nullptr, 0, raw, rawSize, err)) {
        return false;
    }
    if(rawSize < 5) {
        if(err) {
            *err = "HID++ 0x0003 GetDeviceInfo returned a short response";
        }
        return false;
    }

    const std::uint8_t entityCount = raw[4];
    firmware.reserve(entityCount);

    for(std::uint8_t entity = 0; entity < entityCount; ++entity) {
        const std::uint8_t param[1] = { entity };
        RawReport ent{};
        std::size_t entSize = 0;
        if(!requestLongRaw(devInfo.index, 0x01, param, sizeof(param),
                           ent, entSize, err)) {
            return false;
        }
        if(entSize < 6) {
            if(err) {
                *err = "HID++ 0x0003 GetFirmwareInfo returned a short response";
            }
            return false;
        }

        LogitechHIDPP20FirmwareInfo item;
        item.entity = entity;
        item.kind = static_cast<std::uint8_t>(ent[4] & 0x0f);

        if(item.kind == 0x00 || item.kind == 0x01) {
            if(entSize < 12) {
                if(err) {
                    *err = "HID++ 0x0003 firmware entity payload is too short";
                }
                return false;
            }

            item.name.assign(reinterpret_cast<const char*>(&ent[5]), 3);
            while(!item.name.empty() &&
                  (item.name.back() == '\0' || item.name.back() == ' ')) {
                item.name.pop_back();
            }

            item.major = ent[8];
            item.minor = ent[9];
            item.build =
                (static_cast<std::uint16_t>(ent[10]) << 8) |
                 static_cast<std::uint16_t>(ent[11]);

            for(std::size_t i = 13; i < entSize; ++i) {
                if(ent[i] != 0) {
                    item.extra.assign(ent.begin() + 13, ent.begin() + entSize);
                    break;
                }
            }
        } else if(item.kind == 0x02) {
            if(entSize >= 6) {
                item.major = ent[5]; // Hardware revision in HID++ 0x0003.
            }
        }

        firmware.push_back(std::move(item));
    }

    return true;
}

bool LogitechHIDPP20Device::getFeature(
    std::uint16_t featureId, LogitechHIDPP20FeatureInfo& info,
    std::string* err) {

    const std::uint8_t params[2] = {
        static_cast<std::uint8_t>(featureId >> 8),
        static_cast<std::uint8_t>(featureId & 0xff),
    };

    LongReport response{};
    if(!requestLong(0x00, 0x00, params, sizeof(params), response, err)) {
        return false;
    }

    info.index = response[4];
    info.type = response[5];
    info.version = response[6];
    info.versionKnown = info.index != 0;
    return true;
}

} // namespace krgb
