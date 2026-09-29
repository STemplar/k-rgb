#pragma once

#include "core/lightmount_device.h"

namespace krgb::lightmount {

// Hardware-validated IO Center defaults/presets used by the General effects.
std::vector<LightMountGradientStop> defaultRainbowGradient();
std::vector<LightMountGradientStop> defaultMatrixGradient();

LightMountGeneralEffect makeStaticEffect(
    LightMountColor color, std::uint8_t brightness);

LightMountGeneralEffect makeColorWaveSingleEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed,
    LightMountColor color);

LightMountGeneralEffect makeColorWaveDualEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed,
    LightMountColor color1, LightMountColor color2);

LightMountGeneralEffect makeColorWaveGradientEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed,
    std::vector<LightMountGradientStop> gradient);

LightMountGeneralEffect makeTornadoEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed);

LightMountGeneralEffect makeBreathingEffect(
    std::uint8_t brightness, std::uint8_t speed);

LightMountGeneralEffect makeReactiveEffect(
    std::uint8_t brightness, std::uint8_t speed,
    LightMountColor color1, LightMountColor color2);

LightMountGeneralEffect makeMatrixEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed);

} // namespace krgb::lightmount
