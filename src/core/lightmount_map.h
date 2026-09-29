// Physical vendor-LED mapping for the be quiet! Light Mount accent lighting.
//
// These ranges were validated on hardware using IO Center captures and direct
// Linux hidraw writes.
#pragma once

#include <cstddef>
#include <cstdint>

namespace krgb::lightmount {

inline constexpr std::uint16_t kTopBarFirst = 0;
inline constexpr std::uint16_t kTopBarLast = 44;
inline constexpr std::size_t kTopBarCount = 45;       // left -> right

// Illuminated 3D Media Wheel. Rotation controls volume and pressing it toggles
// mute; the media function itself is fixed.
inline constexpr std::uint16_t k3DMediaWheelLed = 45;

inline constexpr std::uint16_t kLeftStripFirst = 158;
inline constexpr std::uint16_t kLeftStripLast = 162;
inline constexpr std::size_t kLeftStripCount = 5;     // top -> bottom

inline constexpr std::uint16_t kRightStripFirst = 163;
inline constexpr std::uint16_t kRightStripLast = 167;
inline constexpr std::size_t kRightStripCount = 5;    // bottom -> top

inline constexpr std::size_t kAccentLedCount =
    kTopBarCount + kLeftStripCount + kRightStripCount;

} // namespace krgb::lightmount
