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

namespace krgb {

struct LightMountLedColor {
    std::uint16_t id;
    std::uint8_t r, g, b;
};

struct LightMountColor {
    std::uint8_t r, g, b;
};

struct LightMountGradientStop {
    std::uint8_t r, g, b;
    std::uint8_t position; // 0..100
};

enum class LightMountLightingMode : std::uint8_t {
    Off     = 0x00,
    General = 0x01,
    Custom  = 0x03,
};

enum class LightMountEffect : std::uint8_t {
    Static    = 0x00,
    ColorWave = 0x01,
    Tornado   = 0x02,
    Breathing = 0x03,
    Reactive  = 0x04,
    Matrix    = 0x05,
};

enum class LightMountDirection : std::uint8_t {
    Up               = 0x00,
    Down             = 0x01,
    Left             = 0x02,
    Right            = 0x03,
    Clockwise        = 0x04,
    CounterClockwise = 0x05,
};

enum class LightMountColorMode : std::uint8_t {
    Single   = 0x00,
    Dual     = 0x01,
    Gradient = 0x02,
};

struct LightMountGeneralEffect {
    LightMountEffect effect = LightMountEffect::Static;
    LightMountDirection direction = LightMountDirection::Up;
    std::uint8_t brightness = 40; // IO Center range: 10..100
    std::uint8_t speed = 50;      // IO Center range: 10..100
    LightMountColorMode colorMode = LightMountColorMode::Single;
    LightMountColor color1{0xe6, 0x30, 0x00};
    LightMountColor color2{0xff, 0xff, 0xff};
    std::vector<LightMountGradientStop> gradient;
};

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

    // Set every known physical RGB element: topbar, knob, keys and side strips.
    bool setSolid(std::uint8_t r, std::uint8_t g, std::uint8_t b);

    // Convenience operation for the 55 physically validated accent LEDs.
    bool setAccentSolid(std::uint8_t topR, std::uint8_t topG, std::uint8_t topB,
                        std::uint8_t leftR, std::uint8_t leftG, std::uint8_t leftB,
                        std::uint8_t rightR, std::uint8_t rightG, std::uint8_t rightB);

private:
    using Report = std::array<std::uint8_t, kReportLen>;

    bool writePacket(Report& packet);
    bool sendFiveLeds(const LightMountLedColor* leds);
    static std::uint16_t crc16Modbus(const std::uint8_t* data, std::size_t len);

    int fd_ = -1;
    std::string path_;
    std::uint8_t sequence_ = 1;
};

} // namespace krgb
