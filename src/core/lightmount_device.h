// LightMountDevice — low-level vendor HID driver for the be quiet! Light Mount.
//
// The keyboard exposes several HID interfaces. This backend talks to the
// vendor-defined interface 2 (usage page 0xFF00) through /dev/hidrawN.
// It implements the Custom RGB path reverse-engineered from be quiet! IO Center.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/bequiet_mount_protocol.h"

namespace krgb {

struct LightMountLedColor {
    std::uint16_t id;
    std::uint8_t r, g, b;
};

// Backward-compatible Light Mount names for the shared Mount-family General
// lighting protocol. Existing Light Mount call sites keep their current API
// while new family-level code can use the BeQuietMount* names directly.
using LightMountColor = BeQuietMountColor;
using LightMountGradientStop = BeQuietMountGradientStop;
using LightMountLightingMode = BeQuietMountLightingMode;
using LightMountEffect = BeQuietMountEffect;
using LightMountDirection = BeQuietMountDirection;
using LightMountColorMode = BeQuietMountColorMode;
using LightMountGeneralEffect = BeQuietMountGeneralEffect;

class LightMountDevice {
public:
    static constexpr std::uint16_t kVendorId = 0x373f;
    static constexpr std::uint16_t kProductId = 0x0002;
    static constexpr int kVendorInterface = 2;
    static constexpr std::size_t kReportLen = 64;
    static constexpr std::size_t kLedsPerPacket = 5;

    LightMountDevice() = default;
    ~LightMountDevice();
    LightMountDevice(const LightMountDevice&) = delete;
    LightMountDevice& operator=(const LightMountDevice&) = delete;

    // Locate the vendor hidraw interface; "" when no Light Mount is present.
    static std::string findDevicePath();

    bool open(std::string* err = nullptr);
    bool openPath(const std::string& path, std::string* err = nullptr);
    void close();
    bool isOpen() const { return fd_ >= 0; }
    const std::string& path() const { return path_; }
    const std::string& lastError() const { return lastError_; }

    // Select the firmware lighting state used by IO Center.
    bool setLightingMode(LightMountLightingMode mode);

    // Switch the keyboard to the IO Center "Custom" lighting mode.
    bool setCustomMode();

    // Configure a firmware-driven IO Center "General" effect (command 0x10/0x06).
    // Gradient mode supports 2..7 ordered stops; endpoints must be 0 and 100.
    bool setGeneralEffect(const LightMountGeneralEffect& effect);

    // Write one or more Custom RGB records. The transport itself always
    // carries five records per packet; a short final group is padded by
    // repeating its final record with the same colour.
    bool setLeds(const std::vector<LightMountLedColor>& leds);

    // Set every known physical RGB element: topbar, 3D Media Wheel, keys and side strips.
    bool setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b);

    // Convenience operation for the 55 physically validated accent LEDs.
    bool setAccentSolid(std::uint8_t topR, std::uint8_t topG, std::uint8_t topB,
                        std::uint8_t leftR, std::uint8_t leftG, std::uint8_t leftB,
                        std::uint8_t rightR, std::uint8_t rightG, std::uint8_t rightB);

private:
    using Report = std::array<std::uint8_t, kReportLen>;

    bool writePacket(Report& packet, bool waitForAck = false);
    bool sendFiveLeds(const LightMountLedColor* leds);
    static std::uint16_t crc16Modbus(const std::uint8_t* data, std::size_t len);

    int fd_ = -1;
    std::string path_;
    std::uint8_t sequence_ = 1;
    std::string lastError_;
};

} // namespace krgb
