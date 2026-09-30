// LogitechHIDPP20Device — low-level HID++ 2.0 transport and feature discovery.
//
// The initial target is the Logitech G810 Orion Spectrum. HID++ 2.0 is
// feature-based: ROOT (0x0000) maps stable 16-bit feature IDs to runtime
// feature indices. Lighting support must therefore be discovered instead of
// hard-coding the index observed on one keyboard.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace krgb {

struct LogitechHIDPP20FeatureInfo {
    std::uint8_t index = 0;
    std::uint8_t type = 0;
    std::uint8_t version = 0;
};

class LogitechHIDPP20Device {
public:
    static constexpr std::uint16_t kVendorId = 0x046d;
    static constexpr std::uint16_t kG810ProductId1 = 0xc331;
    static constexpr std::uint16_t kG810ProductId2 = 0xc337;
    static constexpr int kG810Interface = 1;

    static constexpr std::uint8_t kShortReportId = 0x10;
    static constexpr std::uint8_t kLongReportId = 0x11;
    static constexpr std::uint8_t kVeryLongReportId = 0x12;
    static constexpr std::uint8_t kDefaultDeviceIndex = 0xff;
    static constexpr std::uint8_t kSoftwareId = 0x0a;

    static constexpr std::uint16_t kFeatureRoot = 0x0000;
    static constexpr std::uint16_t kFeatureSet = 0x0001;
    static constexpr std::uint16_t kFeatureColorLedEffects = 0x8070;
    static constexpr std::uint16_t kFeatureRgbEffects = 0x8071;
    static constexpr std::uint16_t kFeaturePerKeyLighting = 0x8080;
    static constexpr std::uint16_t kFeaturePerKeyLighting2 = 0x8081;

    LogitechHIDPP20Device() = default;
    ~LogitechHIDPP20Device();
    LogitechHIDPP20Device(const LogitechHIDPP20Device&) = delete;
    LogitechHIDPP20Device& operator=(const LogitechHIDPP20Device&) = delete;

    // Locate the G810 HID++ interface. Returns "" when no supported G810 is
    // present. productId receives the matched USB PID when non-null.
    static std::string findG810DevicePath(std::uint16_t* productId = nullptr);

    bool openG810(std::string* err = nullptr);
    bool openPath(const std::string& path, std::string* err = nullptr);
    void close();

    bool isOpen() const { return fd_ >= 0; }
    const std::string& path() const { return path_; }
    std::uint16_t productId() const { return productId_; }

    // ROOT.getProtocolVersion(). HID++ 2.0 devices return their protocol
    // major/minor version and echo the ping byte.
    bool getProtocolVersion(std::uint8_t& major, std::uint8_t& minor,
                            std::string* err = nullptr);

    // ROOT.getFeature(featureId). A false return means transport/protocol
    // failure. A successful call with info.index == 0 means unsupported.
    bool getFeature(std::uint16_t featureId, LogitechHIDPP20FeatureInfo& info,
                    std::string* err = nullptr);

    // Set every firmware lighting zone to one static colour through feature
    // 0x8070 (Color LED Effects). Zone count and the static-effect index are
    // discovered at runtime; neither is assumed from the G810 captures.
    bool setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b,
                  std::string* err = nullptr);

    // Set one addressable element through feature 0x8080 (Per Key Lighting)
    // and commit the frame. keyType/keyId are protocol-level addresses; model
    // code is responsible for mapping physical keys/indicators to them.
    bool setPerKey8080Color(std::uint16_t keyType, std::uint8_t keyId,
                            std::uint8_t r, std::uint8_t g, std::uint8_t b,
                            std::string* err = nullptr);

private:
    using LongReport = std::array<std::uint8_t, 20>;
    using VeryLongReport = std::array<std::uint8_t, 64>;

    bool requestLong(std::uint8_t featureIndex, std::uint8_t function,
                     const std::uint8_t* params, std::size_t paramCount,
                     LongReport& response, std::string* err);
    bool writeVeryLong(std::uint8_t featureIndex, std::uint8_t function,
                       const std::uint8_t* params, std::size_t paramCount,
                       std::string* err);
    static bool parseHex(const std::string& value, int& out);

    int fd_ = -1;
    std::string path_;
    std::uint16_t productId_ = 0;
};

} // namespace krgb
