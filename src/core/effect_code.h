#pragma once

#include <cstdint>

namespace krgb {

// GUI/profile effect identifiers are protocol-scoped. This prevents one
// manufacturer's mode enumeration from being reused as another protocol's
// semantic vocabulary. Legacy Alienware profiles keep their historical raw
// krgb::Mode values; new protocol-specific effects use the tagged form below.
enum class EffectProtocol : std::uint8_t {
    BeQuietMount   = 0x01,
    LogitechHIDPP2 = 0x02,
};

inline constexpr int kProtocolEffectMagic = 0x4b000000;
inline constexpr int kProtocolEffectMagicMask = 0xff000000;
inline constexpr int kProtocolEffectProtocolMask = 0x00ff0000;
inline constexpr int kProtocolEffectValueMask = 0x0000ffff;

inline constexpr int makeProtocolEffectCode(EffectProtocol protocol,
                                             std::uint16_t nativeValue) {
    return kProtocolEffectMagic |
           (static_cast<int>(protocol) << 16) |
           static_cast<int>(nativeValue);
}

inline constexpr bool isProtocolEffectCode(int code) {
    return (code & kProtocolEffectMagicMask) == kProtocolEffectMagic;
}

inline constexpr EffectProtocol protocolFromEffectCode(int code) {
    return static_cast<EffectProtocol>((code & kProtocolEffectProtocolMask) >> 16);
}

inline constexpr std::uint16_t nativeEffectValue(int code) {
    return static_cast<std::uint16_t>(code & kProtocolEffectValueMask);
}

// Logitech effect selectors used by the GUI. Values 0x0003/0x0004/0x0005/
// 0x000a are the native 0x8070 effect IDs. KeyPress is an LGS-style software
// effect rendered through evdev + HID++ 0x8080 and therefore uses a private
// value outside the firmware effect-ID range.
enum class LogitechEffect : std::uint16_t {
    ColorCycle = 0x0003,
    ColorWave  = 0x0004,
    Starlight  = 0x0005,
    Breathing  = 0x000a,
    KeyPress   = 0x8001,
};

// Cross-protocol UI parameter semantics. Each backend maps these presets to
// its own wire representation; they are not HID/Alienware protocol values.
enum class EffectSpeedPreset : int {
    Slow = 0,
    Normal = 1,
    Fast = 2,
};

enum class EffectDirection : int {
    Right = 0,
    Left = 1,
    Down = 2,
    Up = 3,
    Clockwise = 4,
    CounterClockwise = 5,
};

} // namespace krgb
