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

namespace krgb {

struct LogitechHIDPP20FeatureInfo {
    std::uint8_t index = 0;
    std::uint8_t type = 0;
    std::uint8_t version = 0;
};

struct LogitechHIDPP20Feature {
    std::uint16_t featureId = 0;
    std::uint8_t index = 0;
    std::uint8_t type = 0;
    std::uint8_t version = 0;
};

struct LogitechHIDPP20Capabilities {
    std::uint8_t protocolMajor = 0;
    std::uint8_t protocolMinor = 0;
    std::vector<LogitechHIDPP20Feature> features;

    const LogitechHIDPP20Feature* findFeature(std::uint16_t featureId) const {
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
    static constexpr std::uint16_t kFeatureBrightnessControl = 0x8040;
    static constexpr std::uint16_t kFeatureColorLedEffects = 0x8070;
    static constexpr std::uint16_t kFeatureRgbEffects = 0x8071;
    static constexpr std::uint16_t kFeaturePerKeyLighting = 0x8080;
    static constexpr std::uint16_t kFeaturePerKeyLighting2 = 0x8081;

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

    // ROOT.getProtocolVersion(). HID++ 2.0 devices return their protocol
    // major/minor version and echo the ping byte.
    bool getProtocolVersion(std::uint8_t& major, std::uint8_t& minor,
                            std::string* err = nullptr);

    // ROOT.getFeature(featureId). A false return means transport/protocol
    // failure. A successful call with info.index == 0 means unsupported.
    bool getFeature(std::uint16_t featureId, LogitechHIDPP20FeatureInfo& info,
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
    // discovered at runtime; neither is assumed from the G810 captures.
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
    // very-long frame, then FlushLEDs once. This is important for groups such
    // as G810 status indicators: a partial frame can clear unstaged members.
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
