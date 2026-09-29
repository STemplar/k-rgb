#include "core/lightmount_effects.h"

#include <utility>

namespace krgb::lightmount {

std::vector<LightMountGradientStop> defaultRainbowGradient() {
    return {
        {255,   0,   0,   0},
        {255, 255,   0,  17},
        {  0, 255,   0,  33},
        {  0, 255, 255,  50},
        {  0,   0, 255,  67},
        {255,   0, 255,  83},
        {255,   0,   0, 100},
    };
}

std::vector<LightMountGradientStop> defaultMatrixGradient() {
    return {
        {0x0d, 0x02, 0x08,   0},
        {0x00, 0x3b, 0x00,  33},
        {0x00, 0x8f, 0x11,  67},
        {0x00, 0xff, 0x41, 100},
    };
}

LightMountGeneralEffect makeStaticEffect(
    LightMountColor color, std::uint8_t brightness) {

    LightMountGeneralEffect effect;
    effect.effect = LightMountEffect::Static;
    effect.direction = LightMountDirection::Up; // ignored by Static
    effect.brightness = brightness;
    effect.speed = 50; // captured IO Center value; ignored by Static
    effect.colorMode = LightMountColorMode::Single;
    effect.color1 = color;
    return effect;
}

LightMountGeneralEffect makeColorWaveSingleEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed,
    LightMountColor color) {

    LightMountGeneralEffect effect;
    effect.effect = LightMountEffect::ColorWave;
    effect.direction = direction;
    effect.brightness = brightness;
    effect.speed = speed;
    effect.colorMode = LightMountColorMode::Single;
    effect.color1 = color;
    return effect;
}

LightMountGeneralEffect makeColorWaveDualEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed,
    LightMountColor color1, LightMountColor color2) {

    LightMountGeneralEffect effect;
    effect.effect = LightMountEffect::ColorWave;
    effect.direction = direction;
    effect.brightness = brightness;
    effect.speed = speed;
    effect.colorMode = LightMountColorMode::Dual;
    effect.color1 = color1;
    effect.color2 = color2;
    return effect;
}

LightMountGeneralEffect makeColorWaveGradientEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed,
    std::vector<LightMountGradientStop> gradient) {

    LightMountGeneralEffect effect;
    effect.effect = LightMountEffect::ColorWave;
    effect.direction = direction;
    effect.brightness = brightness;
    effect.speed = speed;
    effect.colorMode = LightMountColorMode::Gradient;
    effect.gradient = std::move(gradient);
    return effect;
}

LightMountGeneralEffect makeTornadoEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed) {

    LightMountGeneralEffect effect;
    effect.effect = LightMountEffect::Tornado;
    effect.direction = direction;
    effect.brightness = brightness;
    effect.speed = speed;
    effect.colorMode = LightMountColorMode::Gradient;
    effect.gradient = defaultRainbowGradient();
    return effect;
}

LightMountGeneralEffect makeBreathingEffect(
    std::uint8_t brightness, std::uint8_t speed) {

    LightMountGeneralEffect effect;
    effect.effect = LightMountEffect::Breathing;
    effect.direction = LightMountDirection::Up; // ignored by Breathing
    effect.brightness = brightness;
    effect.speed = speed;
    effect.colorMode = LightMountColorMode::Gradient;
    effect.gradient = defaultRainbowGradient();
    return effect;
}

LightMountGeneralEffect makeReactiveEffect(
    std::uint8_t brightness, std::uint8_t speed,
    LightMountColor color1, LightMountColor color2) {

    LightMountGeneralEffect effect;
    effect.effect = LightMountEffect::Reactive;
    effect.direction = LightMountDirection::Up; // ignored by Reactive
    effect.brightness = brightness;
    effect.speed = speed;
    effect.colorMode = LightMountColorMode::Dual;
    effect.color1 = color1;
    effect.color2 = color2;
    return effect;
}

LightMountGeneralEffect makeMatrixEffect(
    LightMountDirection direction, std::uint8_t brightness, std::uint8_t speed) {

    LightMountGeneralEffect effect;
    effect.effect = LightMountEffect::Matrix;
    effect.direction = direction;
    effect.brightness = brightness;
    effect.speed = speed;
    effect.colorMode = LightMountColorMode::Gradient;
    effect.gradient = defaultMatrixGradient();
    return effect;
}

} // namespace krgb::lightmount
