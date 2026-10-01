// LogitechHIDPP20Device — low-level HID++ 2.0 transport and feature discovery.
//
// Generic Logitech HID++ 2.0 keyboard transport.
// USB descriptors identify the attached product; HID++ feature discovery and
// the device-reported lighting topology determine what can actually be driven.
// Known product IDs are used only for quirks that HID++ does not describe.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "core/logitech_known_devices.h"
#include "core/logitech_hidpp20_feature_catalog.h"

namespace krgb {

inline constexpr std::uint8_t kHidppFeatureFlagObsolete = 0x80;
inline constexpr std::uint8_t kHidppFeatureFlagHidden = 0x40;
inline constexpr std::uint8_t kHidppFeatureFlagEngineering = 0x20;
inline constexpr std::uint8_t kHidppFeatureFlagManufacturingDeactivatable = 0x10;
inline constexpr std::uint8_t kHidppFeatureFlagComplianceDeactivatable = 0x08;

struct LogitechHIDPP20FeatureInfo {
    std::uint8_t index = 0;
    std::uint8_t type = 0;
    std::uint8_t version = 0;
    bool versionKnown = false;

    bool isObsolete() const { return (type & kHidppFeatureFlagObsolete) != 0; }
    bool isHidden() const { return (type & kHidppFeatureFlagHidden) != 0; }
    bool isEngineering() const { return (type & kHidppFeatureFlagEngineering) != 0; }
    bool isManufacturingDeactivatable() const {
        return (type & kHidppFeatureFlagManufacturingDeactivatable) != 0;
    }
    bool isComplianceDeactivatable() const {
        return (type & kHidppFeatureFlagComplianceDeactivatable) != 0;
    }
    bool isUsableByEndUserSoftware() const {
        return (type & (kHidppFeatureFlagHidden |
                        kHidppFeatureFlagEngineering |
                        kHidppFeatureFlagManufacturingDeactivatable |
                        kHidppFeatureFlagComplianceDeactivatable)) == 0;
    }
};

struct LogitechHIDPP20Feature {
    std::uint16_t featureId = 0;
    std::uint8_t index = 0;
    std::uint8_t type = 0;
    std::uint8_t version = 0;
    bool versionKnown = false;

    bool isObsolete() const { return (type & kHidppFeatureFlagObsolete) != 0; }
    bool isHidden() const { return (type & kHidppFeatureFlagHidden) != 0; }
    bool isEngineering() const { return (type & kHidppFeatureFlagEngineering) != 0; }
    bool isManufacturingDeactivatable() const {
        return (type & kHidppFeatureFlagManufacturingDeactivatable) != 0;
    }
    bool isComplianceDeactivatable() const {
        return (type & kHidppFeatureFlagComplianceDeactivatable) != 0;
    }
    bool isUsableByEndUserSoftware() const {
        return (type & (kHidppFeatureFlagHidden |
                        kHidppFeatureFlagEngineering |
                        kHidppFeatureFlagManufacturingDeactivatable |
                        kHidppFeatureFlagComplianceDeactivatable)) == 0;
    }
};

struct LogitechHIDPP20ProtocolInfo {
    std::uint8_t protocolNumber = 0;
    std::uint8_t targetSoftware = 0;
    std::uint8_t pingData = 0;

    bool hasTargetSoftwareHint() const { return protocolNumber >= 3; }
};

struct LogitechHIDPP20Capabilities {
    LogitechHIDPP20ProtocolInfo protocol;
    std::vector<LogitechHIDPP20Feature> features;

    // Normal capability lookup excludes features that Logitech marks hidden,
    // engineering or deactivatable for manufacturing/compliance use.
    const LogitechHIDPP20Feature* findFeature(std::uint16_t featureId) const {
        for(const auto& feature : features) {
            if(feature.featureId == featureId &&
               feature.isUsableByEndUserSoftware()) {
                return &feature;
            }
        }
        return nullptr;
    }

    // Diagnostics may still need to display every feature the device reports.
    const LogitechHIDPP20Feature* findReportedFeature(
        std::uint16_t featureId) const {
        for(const auto& feature : features) {
            if(feature.featureId == featureId) {
                return &feature;
            }
        }
        return nullptr;
    }
};

struct LogitechHIDPP20FirmwareInfo {
    std::uint8_t entity = 0;
    std::uint8_t kind = 0xff; // 0=firmware, 1=bootloader, 2=hardware
    std::string name;
    std::uint8_t major = 0;
    std::uint8_t minor = 0;
    std::uint16_t build = 0;
    std::vector<std::uint8_t> extra;
};

struct LogitechHIDPP20KeyColor {
    std::uint8_t keyId = 0;
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct LogitechHIDPP20ZoneColor {
    std::uint8_t zone = 0; // zero-based logical zone index
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct LogitechHIDPP20PerKeyTypeInfo {
    std::uint16_t keyType = 0;
    std::uint16_t keyCount = 0;
    std::vector<LogitechHIDPP20KeyColor> colors;
};

struct LogitechHIDPP20PerKeyInfo {
    std::uint16_t typeFlags = 0;
    // Raw fn0 payload words at offsets 3..4 and 5..6. Some references name
    // these keyTypeCount/maxKeyCount, but hardware can disagree with the
    // populated typeFlags/fn1 topology (for example the G810 reports 2/0
    // while four key types are populated). Keep them for diagnostics only.
    std::uint16_t keyTypeCount = 0;
    std::uint16_t maxKeyCount = 0;
    std::vector<LogitechHIDPP20PerKeyTypeInfo> types;
};

struct LogitechUsbIdentity {
    std::uint16_t vendorId = 0;
    std::uint16_t productId = 0;
    int interfaceNumber = -1;
    std::string manufacturer;
    std::string product;
    std::string serial;
    std::string hidName;
};

struct LogitechHIDPP20KeyboardLayoutInfo {
    std::uint16_t featureId = 0;
    std::uint8_t countryCode = 0;
};

struct LogitechHIDPP20DeviceTypeInfo {
    std::uint8_t deviceType = 0xff;
    std::string name;
};

// HID++ 0x0005 (Device Type and Name), versions 0..2.
// Returns nullptr for values not defined by the feature specification.
inline constexpr const char* logitechHIDPP20DeviceTypeName(std::uint8_t type) {
    switch(type) {
        case 0: return "Keyboard";
        case 1: return "Remote Control";
        case 2: return "Numpad";
        case 3: return "Mouse";
        case 4: return "Trackpad";
        case 5: return "Trackball";
        case 6: return "Presenter";
        case 7: return "Receiver";
        case 8: return "Headset";
        case 9: return "Webcam";
        case 10: return "Steering Wheel";
        case 11: return "Joystick";
        case 12: return "Gamepad";
        case 13: return "Dock";
        case 14: return "Speaker";
        case 15: return "Microphone";
        case 16: return "Illumination Light";
        case 17: return "Programmable Controller";
        case 18: return "Car Sim Pedals";
        case 19: return "Adapter";
        default: return nullptr;
    }
}

struct LogitechHIDPP20BrightnessInfo {
    std::uint16_t minimum = 0;
    std::uint16_t maximum = 0;
    std::uint16_t steps = 0;
    std::uint8_t capabilities = 0;
    std::uint16_t current = 0;
    bool hasCurrent = false;
};

struct LogitechHIDPP20DisableKeysInfo {
    std::uint8_t disableableMask = 0;
    std::uint8_t disabledMask = 0;
    bool hasDisabledMask = false;
    std::uint8_t maxDisabledUsages = 0;
};

struct LogitechHIDPP20ControlInfo {
    std::uint16_t controlId = 0;
    std::uint16_t taskId = 0;
    std::uint16_t flags = 0;
    std::uint8_t position = 0;
    std::uint8_t group = 0;
    std::uint8_t groupMask = 0;
};

struct LogitechHIDPP20ControlsInfo {
    std::uint16_t featureId = 0;
    std::vector<LogitechHIDPP20ControlInfo> controls;
};

struct LogitechHIDPP20ColorLedEffectInfo {
    std::uint8_t zoneIndex = 0;
    std::uint8_t effectIndex = 0;
    std::uint16_t effectId = 0;
    std::uint16_t capabilities = 0;
    std::uint16_t periodMs = 0;
};

struct LogitechHIDPP20ColorLedZoneInfo {
    std::uint8_t zoneIndex = 0;
    std::uint16_t location = 0;
    std::uint8_t effectCount = 0;
    std::uint8_t persistencyCapabilities = 0;
    std::vector<LogitechHIDPP20ColorLedEffectInfo> effects;
};

struct LogitechHIDPP20ColorLedInfo {
    std::uint8_t zoneCount = 0;
    std::uint16_t nvCapabilities = 0;
    std::uint16_t extCapabilities = 0;
    std::vector<LogitechHIDPP20ColorLedZoneInfo> zones;
};

struct LogitechHIDPP20PerKey8081Info {
    std::vector<std::uint8_t> zoneIds;
};

struct LogitechHIDPP20RgbEffectInfo {
    std::uint8_t clusterIndex = 0;
    std::uint8_t effectIndex = 0;
    std::uint16_t effectId = 0;
    std::uint16_t capabilities = 0;
    std::uint16_t periodMs = 0;
};

struct LogitechHIDPP20RgbClusterInfo {
    std::uint8_t clusterIndex = 0;
    std::uint16_t location = 0;
    std::uint8_t effectCount = 0;
    std::uint8_t displayPersistencyCapabilities = 0;
    bool effectPersistency = false;
    bool multiLedPattern = false;
    std::vector<LogitechHIDPP20RgbEffectInfo> effects;
};

struct LogitechHIDPP20RgbEffectsInfo {
    std::uint8_t clusterCount = 0;
    std::uint16_t nvCapabilities = 0;
    std::uint16_t extCapabilities = 0;
    std::uint8_t multiClusterEffectCount = 0;
    std::vector<LogitechHIDPP20RgbClusterInfo> clusters;
};

struct LogitechHIDPP20ReportRateInfo {
    std::uint16_t supportedMask = 0;
    std::uint8_t current = 0;
    bool hasCurrent = false;
};

struct LogitechHIDPP20ModeStatusInfo {
    std::uint8_t status0 = 0;
    std::uint8_t status1 = 0;
    std::uint16_t capabilities = 0;
};

struct LogitechHIDPP20LightingFeatures {
    LogitechHIDPP20FeatureInfo brightness8040;
    LogitechHIDPP20FeatureInfo colorLedEffects8070;
    LogitechHIDPP20FeatureInfo rgbEffects8071;
    LogitechHIDPP20FeatureInfo perKey8080;
    LogitechHIDPP20FeatureInfo perKey8081;

    bool hasKnownLightingFeature() const {
        return brightness8040.index != 0 ||
               colorLedEffects8070.index != 0 ||
               rgbEffects8071.index != 0 ||
               perKey8080.index != 0 ||
               perKey8081.index != 0;
    }
};

class LogitechHIDPP20Device {
public:
    static constexpr std::uint16_t kVendorId = 0x046d;

    static constexpr std::uint8_t kShortReportId = 0x10;
    static constexpr std::uint8_t kLongReportId = 0x11;
    static constexpr std::uint8_t kVeryLongReportId = 0x12;
    static constexpr std::uint8_t kDefaultDeviceIndex = 0xff;
    static constexpr std::uint8_t kSoftwareId = 0x0a;

    static constexpr std::uint16_t kFeatureRoot = 0x0000;
    static constexpr std::uint16_t kFeatureSet = 0x0001;
    static constexpr std::uint16_t kFeatureDeviceInformation = 0x0003;
    static constexpr std::uint16_t kFeatureDeviceTypeAndName = 0x0005;
    static constexpr std::uint16_t kFeatureReprogControls = 0x1b00;
    static constexpr std::uint16_t kFeatureReprogControls5 = 0x1b04;
    static constexpr std::uint16_t kFeatureKeyboardLayout = 0x4520;
    static constexpr std::uint16_t kFeatureDisableKeys = 0x4521;
    static constexpr std::uint16_t kFeatureDisableKeysByUsage = 0x4522;
    static constexpr std::uint16_t kFeatureKeyboardInternationalLayouts = 0x4540;
    static constexpr std::uint16_t kFeatureGamingGKeys = 0x8010;
    static constexpr std::uint16_t kFeatureGamingMKeys = 0x8020;
    static constexpr std::uint16_t kFeatureMacroRecord = 0x8030;
    static constexpr std::uint16_t kFeatureBrightnessControl = 0x8040;
    static constexpr std::uint16_t kFeatureColorLedEffects = 0x8070;
    static constexpr std::uint16_t kFeatureRgbEffects = 0x8071;
    static constexpr std::uint16_t kFeaturePerKeyLighting = 0x8080;
    static constexpr std::uint16_t kFeaturePerKeyLighting2 = 0x8081;
    static constexpr std::uint16_t kFeatureAdjustableReportRate = 0x8060;
    static constexpr std::uint16_t kFeatureExtendedAdjustableReportRate = 0x8061;
    static constexpr std::uint16_t kFeatureModeStatus = 0x8090;
    static constexpr std::uint16_t kFeatureOnboardProfiles = 0x8100;

    LogitechHIDPP20Device() = default;
    ~LogitechHIDPP20Device();
    LogitechHIDPP20Device(const LogitechHIDPP20Device&) = delete;
    LogitechHIDPP20Device& operator=(const LogitechHIDPP20Device&) = delete;

    // Scan Logitech hidraw interfaces and select an endpoint that identifies
    // itself as HID++ 2.0 and exposes a supported lighting feature. 0x8080
    // endpoints are proven as keyboards from their keyboard key type; other
    // lighting families currently require a known keyboard USB identity.
    bool openKeyboard(std::string* err = nullptr);
    bool openPath(const std::string& path, std::string* err = nullptr);
    void close();

    bool isOpen() const { return fd_ >= 0; }
    const std::string& path() const { return path_; }
    const LogitechUsbIdentity& usbIdentity() const { return usbIdentity_; }
    std::uint16_t productId() const { return usbIdentity_.productId; }
    std::string displayName() const;

    LogitechLightingColorCapability colorCapability() const;
    bool isMonochrome() const {
        return colorCapability() == LogitechLightingColorCapability::Monochrome;
    }

    static LogitechLightingColorCapability colorCapabilityForProductId(
        std::uint16_t productId);

    // Enumerate the complete HID++ 2.0 Feature Set (0x0001). The returned list
    // preserves every device-reported feature ID, runtime index, type/flags and
    // version, including features unknown to k-rgb.
    bool enumerateFeatures(std::vector<LogitechHIDPP20Feature>& features,
                           std::string* err = nullptr);

    // Build the runtime capability snapshot from HID++ protocol version plus
    // full Feature Set enumeration.
    bool getCapabilities(LogitechHIDPP20Capabilities& capabilities,
                         std::string* err = nullptr);

    // Extract lighting features currently understood by k-rgb from the fully
    // enumerated Feature Set. Runtime indices are never selected by USB PID.
    bool getLightingFeatures(LogitechHIDPP20LightingFeatures& features,
                             std::string* err = nullptr);

    // ROOT.getProtocolVersion(). Modern ROOT v2 reports a protocol number,
    // a target-software hint and the echoed ping byte. In particular the
    // second byte is not a protocol minor version when protocolNumber >= 3.
    bool getProtocolInfo(LogitechHIDPP20ProtocolInfo& info,
                         std::string* err = nullptr);

    // Compatibility wrapper for callers that still expect two output bytes.
    // The outputs are protocolNumber and targetSoftware, not major/minor.
    bool getProtocolVersion(std::uint8_t& protocolNumber,
                            std::uint8_t& targetSoftware,
                            std::string* err = nullptr);

    // ROOT.getFeature(featureId). A false return means transport/protocol
    // failure. A successful call with info.index == 0 means unsupported.
    bool getFeature(std::uint16_t featureId, LogitechHIDPP20FeatureInfo& info,
                    std::string* err = nullptr);

    // Read-only capability queries. These only call documented getter
    // functions for features that were reported by the runtime Feature Set.
    bool getDeviceTypeAndName(LogitechHIDPP20DeviceTypeInfo& info,
                              std::string* err = nullptr);
    bool getKeyboardLayout(LogitechHIDPP20KeyboardLayoutInfo& info,
                           std::string* err = nullptr);
    bool getBrightnessInfo(LogitechHIDPP20BrightnessInfo& info,
                           std::string* err = nullptr);
    bool getDisableKeysInfo(LogitechHIDPP20DisableKeysInfo& info,
                            std::string* err = nullptr);
    bool getReprogrammableControls(LogitechHIDPP20ControlsInfo& info,
                                   std::string* err = nullptr);
    bool getColorLed8070Info(LogitechHIDPP20ColorLedInfo& info,
                             std::string* err = nullptr);
    bool getPerKey8081Info(LogitechHIDPP20PerKey8081Info& info,
                           std::string* err = nullptr);
    bool getRgbEffects8071Info(LogitechHIDPP20RgbEffectsInfo& info,
                               std::string* err = nullptr);
    bool getReportRateInfo(LogitechHIDPP20ReportRateInfo& info,
                           std::string* err = nullptr);
    bool getModeStatusInfo(LogitechHIDPP20ModeStatusInfo& info,
                           std::string* err = nullptr);

    // HID++ 2.0 feature 0x0003 (Device Information). Reads all reported
    // firmware/hardware entities such as main firmware and bootloader.
    bool getFirmwareInfo(std::vector<LogitechHIDPP20FirmwareInfo>& firmware,
                         std::string* err = nullptr);

    // Read the 0x8080 Per Key Lighting topology and current volatile RGB
    // buffer. This is read-only and lets model code build a key map from the
    // device-reported key types/IDs instead of assuming a keyboard variant.
    bool getPerKey8080Info(LogitechHIDPP20PerKeyInfo& info,
                           std::string* err = nullptr);

    // Set every firmware lighting zone to one static colour through feature
    // 0x8070 (Color LED Effects). Zone count and the static-effect index are
    // discovered at runtime rather than assumed from model-specific captures.
    bool setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                  std::string* err = nullptr);

    // Read the number of 0x8070 lighting zones reported by the device.
    bool getColorLed8070ZoneCount(std::uint8_t& zoneCount,
                                  std::string* err = nullptr);

    // Set individual 0x8070 zones. Input zones are zero-based logical indices;
    // model quirks map them to protocol region IDs where required.
    bool setColorLed8070Zones(const std::vector<LogitechHIDPP20ZoneColor>& colors,
                              std::string* err = nullptr);

    // Set one addressable element through feature 0x8080 (Per Key Lighting)
    // and commit the frame. keyType/keyId are protocol-level addresses; model
    // code is responsible for mapping physical keys/indicators to them.
    bool setPerKey8080Color(std::uint16_t keyType, std::uint8_t keyId,
                            std::uint8_t r, std::uint8_t g, std::uint8_t b,
                            std::string* err = nullptr);

    // Stage a complete set of colours for one 0x8080 keyType in a single
    // very-long frame, then FlushLEDs once. This is important for multi-element
    // groups because a partial frame can clear unstaged members.
    bool setPerKey8080Colors(std::uint16_t keyType,
                             const std::vector<LogitechHIDPP20KeyColor>& colors,
                             std::string* err = nullptr);

private:
    using LongReport = std::array<std::uint8_t, 20>;
    using VeryLongReport = std::array<std::uint8_t, 64>;
    using RawReport = std::array<std::uint8_t, 64>;

    bool requestLongRaw(std::uint8_t featureIndex, std::uint8_t function,
                        const std::uint8_t* params, std::size_t paramCount,
                        RawReport& response, std::size_t& responseSize,
                        std::string* err);
    bool requestLong(std::uint8_t featureIndex, std::uint8_t function,
                     const std::uint8_t* params, std::size_t paramCount,
                     LongReport& response, std::string* err);
    bool writeVeryLong(std::uint8_t featureIndex, std::uint8_t function,
                       const std::uint8_t* params, std::size_t paramCount,
                       std::string* err);
    static bool parseHex(const std::string& value, int& out);
    bool probeKeyboard(std::string* err = nullptr);
    bool ensureFeatureSet(std::string* err = nullptr);
    bool getDiscoveredFeature(std::uint16_t featureId,
                              LogitechHIDPP20FeatureInfo& info,
                              std::string* err = nullptr);
    void normalizeLightingColor(std::uint8_t& r, std::uint8_t& g,
                                std::uint8_t& b) const;

    int fd_ = -1;
    std::string path_;
    LogitechUsbIdentity usbIdentity_;
    bool featureSetEnumerated_ = false;
    std::vector<LogitechHIDPP20Feature> discoveredFeatures_;
};

} // namespace krgb
