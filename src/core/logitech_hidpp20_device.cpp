#include "core/logitech_hidpp20_device.h"

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
                char* end = nullptr;
                errno = 0;
                const long parsedVid =
                    std::strtol(body.substr(p1 + 1, p2 - p1 - 1).c_str(),
                                &end, 16);
                if(end && *end == '\0' && errno == 0) {
                    vid = static_cast<int>(parsedVid);
                }

                end = nullptr;
                errno = 0;
                const long parsedPid =
                    std::strtol(body.substr(p2 + 1).c_str(), &end, 16);
                if(end && *end == '\0' && errno == 0) {
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
    switch(productId) {
        case kG610ProductId1:
        case kG610ProductId2:
            return LogitechLightingColorCapability::Monochrome;
        case kG810ProductId1:
        case kG810ProductId2:
            return LogitechLightingColorCapability::Rgb;
        default:
            return LogitechLightingColorCapability::Unknown;
    }
}

LogitechLightingColorCapability LogitechHIDPP20Device::colorCapability() const {
    return colorCapabilityForProductId(usbIdentity_.productId);
}

std::string LogitechHIDPP20Device::displayName() const {
    if(!usbIdentity_.product.empty()) {
        return usbIdentity_.product;
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

bool LogitechHIDPP20Device::probePerKeyKeyboard(std::string* err) {
    std::uint8_t major = 0;
    std::uint8_t minor = 0;
    if(!getProtocolVersion(major, minor, err)) {
        return false;
    }
    if(major < 2) {
        if(err) {
            *err = "device is not HID++ 2.0";
        }
        return false;
    }

    LogitechHIDPP20FeatureInfo perKey;
    if(!getFeature(kFeaturePerKeyLighting, perKey, err)) {
        return false;
    }
    if(perKey.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8080 is not supported";
        }
        return false;
    }

    LogitechHIDPP20PerKeyInfo info;
    if(!getPerKey8080Info(info, err)) {
        return false;
    }
    if((info.typeFlags & 0x0001) == 0) {
        if(err) {
            *err = "HID++ 0x8080 reports no keyboard key type";
        }
        return false;
    }
    return true;
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
        if(probePerKeyKeyboard(&probeError)) {
            return true;
        }

        lastProbeError = displayName() + ": " + probeError;
        close();
    }

    if(err) {
        *err = "No Logitech HID++ 2.0 per-key keyboard found";
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

bool LogitechHIDPP20Device::getProtocolVersion(
    std::uint8_t& major, std::uint8_t& minor, std::string* err) {

    // IRoot function 1. The two leading zero bytes plus a ping value follow
    // Logitech's HID++ 2.0 protocol-version request definition.
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

    major = response[4];
    minor = response[5];
    return true;
}

bool LogitechHIDPP20Device::getPerKey8080Info(
    LogitechHIDPP20PerKeyInfo& info, std::string* err) {

    info = {};

    LogitechHIDPP20FeatureInfo perKey;
    if(!getFeature(kFeaturePerKeyLighting, perKey, err)) {
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

    constexpr std::uint16_t kKnownTypes[] = {
        0x0001, // keyboard
        0x0002, // consumer/media
        0x0004, // G-keys
        0x0008, // buttons
        0x0010, // logo
        0x0040, // indicators
    };

    for(const std::uint16_t keyType : kKnownTypes) {
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

bool LogitechHIDPP20Device::setSolid(
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::string* err) {

    normalizeLightingColor(r, g, b);

    LogitechHIDPP20FeatureInfo fx;
    if(!getFeature(kFeatureColorLedEffects, fx, err)) {
        return false;
    }
    if(fx.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8070 (Color LED Effects) is not supported";
        }
        return false;
    }

    // 0x8070 function 0: GetInfo. On this feature generation byte 0 is the
    // number of firmware lighting zones.
    LongReport infoResponse{};
    if(!requestLong(fx.index, 0x00, nullptr, 0, infoResponse, err)) {
        return false;
    }
    const std::uint8_t zoneCount = infoResponse[4];
    if(zoneCount == 0) {
        if(err) {
            *err = "HID++ 0x8070 reported zero lighting zones";
        }
        return false;
    }

    // Find a static-color effect in every zone. 0x8070 function 1 returns
    // zone metadata; function 2 returns effect metadata. Effect type 0x0001
    // is the static-color effect.
    std::vector<std::uint8_t> staticEffectIndex(zoneCount, 0xff);
    for(std::uint8_t zone = 0; zone < zoneCount; ++zone) {
        const std::uint8_t zoneQuery[2] = {zone, 0x00};
        LongReport zoneResponse{};
        if(!requestLong(fx.index, 0x01, zoneQuery, sizeof(zoneQuery),
                        zoneResponse, err)) {
            return false;
        }

        const std::uint8_t effectCount = zoneResponse[7];
        for(std::uint8_t effect = 0; effect < effectCount; ++effect) {
            const std::uint8_t effectQuery[4] = {zone, effect, 0x00, 0x00};
            LongReport effectResponse{};
            if(!requestLong(fx.index, 0x02, effectQuery, sizeof(effectQuery),
                            effectResponse, err)) {
                return false;
            }

            const std::uint16_t effectType =
                (static_cast<std::uint16_t>(effectResponse[6]) << 8) |
                 static_cast<std::uint16_t>(effectResponse[7]);
            if(effectType == 0x0001) {
                staticEffectIndex[zone] = effect;
                break;
            }
        }

        if(staticEffectIndex[zone] == 0xff) {
            if(err) {
                *err = "HID++ 0x8070 zone " + std::to_string(zone) +
                       " has no static-color effect";
            }
            return false;
        }
    }

    // 0x8070 function 8: SetSWControl(enabled, persist). Claim live software
    // control before changing zone effects.
    const std::uint8_t claim[2] = {0x01, 0x01};
    LongReport claimResponse{};
    if(!requestLong(fx.index, 0x08, claim, sizeof(claim), claimResponse, err)) {
        return false;
    }

    // 0x8070 function 3: SetEffectByIndex.
    // Payload: zone, effect index, 10-byte parameter block, persistence/power.
    // Static effect parameters are RGB followed by marker 0x02.
    for(std::uint8_t zone = 0; zone < zoneCount; ++zone) {
        std::uint8_t params[16]{};
        params[0] = zone;
        params[1] = staticEffectIndex[zone];
        params[2] = r;
        params[3] = g;
        params[4] = b;
        params[5] = (r || g || b) ? 0x02 : 0x00;
        params[12] = 0x00; // live/volatile, do not persist to onboard storage

        LongReport response{};
        if(!requestLong(fx.index, 0x03, params, sizeof(params), response, err)) {
            return false;
        }
    }

    // G610/G810 keep the five status/backlight indicators in 0x8080 keyType
    // 0x0040. Program the complete group to the same RGB value so inactive
    // indicators retain their colour for the next time their status turns on.
    const std::vector<LogitechHIDPP20KeyColor> indicators = {
        {0x01, r, g, b}, // backlight
        {0x02, r, g, b}, // game mode
        {0x03, r, g, b}, // Caps Lock
        {0x04, r, g, b}, // Scroll Lock
        {0x05, r, g, b}, // Num Lock
    };
    if(!setPerKey8080Colors(0x0040, indicators, err)) {
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
    if(!getFeature(kFeaturePerKeyLighting, perKey, err)) {
        return false;
    }
    if(perKey.index == 0) {
        if(err) {
            *err = "HID++ feature 0x8080 (Per Key Lighting) is not supported";
        }
        return false;
    }

    // Function 3: SetKeyColors. Payload is keyType (BE16), count (BE16),
    // then (keyId, R, G, B) tuples. G610/G810 use a 0x12 64-byte report.
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
    if(!getFeature(kFeaturePerKeyLighting, perKey, err)) {
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
    if(!getFeature(kFeatureDeviceInformation, devInfo, err)) {
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
    return true;
}

} // namespace krgb
