#pragma once

#include <cstdint>
#include <vector>

namespace krgb {

// Shared lighting vocabulary for the be quiet! Mount keyboard family.
//
// The Light Mount protocol has been hardware-validated. Dark Mount captures
// show the same General-effect command family (0x10/0x06) and effect IDs.
// Model-specific transport/session handling and physical LED maps remain in
// their respective device backends until they are validated independently.
struct BeQuietMountColor {
    std::uint8_t r, g, b;
};

struct BeQuietMountGradientStop {
    std::uint8_t r, g, b;
    std::uint8_t position; // 0..100
};

enum class BeQuietMountLightingMode : std::uint8_t {
    Off     = 0x00,
    General = 0x01,
    Custom  = 0x03,
};

enum class BeQuietMountEffect : std::uint8_t {
    Static    = 0x00,
    ColorWave = 0x01,
    Tornado   = 0x02,
    Breathing = 0x03,
    Reactive  = 0x04,
    Matrix    = 0x05,
};

enum class BeQuietMountDirection : std::uint8_t {
    Up               = 0x00,
    Down             = 0x01,
    Left             = 0x02,
    Right            = 0x03,
    Clockwise        = 0x04,
    CounterClockwise = 0x05,
};

enum class BeQuietMountColorMode : std::uint8_t {
    Single   = 0x00,
    Dual     = 0x01,
    Gradient = 0x02,
};

struct BeQuietMountGeneralEffect {
    BeQuietMountEffect effect = BeQuietMountEffect::Static;
    BeQuietMountDirection direction = BeQuietMountDirection::Up;
    std::uint8_t brightness = 40;
    std::uint8_t speed = 50;
    BeQuietMountColorMode colorMode = BeQuietMountColorMode::Single;
    BeQuietMountColor color1{0xe6, 0x30, 0x00};
    BeQuietMountColor color2{0xff, 0xff, 0xff};
    std::vector<BeQuietMountGradientStop> gradient;
};

} // namespace krgb
