#pragma once

#include <array>
#include <cstdint>

namespace krgb {

// Product identity and protocol/backend are deliberately separate. Multiple
// keyboard models may share one lighting backend, as AW410K/AW510K already do.
enum class KeyboardModelId : std::uint8_t {
    Unknown = 0,
    AlienwareAW410K,
    AlienwareAW510K,
    BeQuietLightMount,
    LogitechG213Prodigy,
    LogitechG410AtlasSpectrum,
    LogitechG413Carbon,
    LogitechG512Carbon,
    LogitechG513Carbon,
    LogitechG610Orion,
    LogitechG810OrionSpectrum,
    LogitechG815Lightsync,
    LogitechG910OrionSpark,
    LogitechG910OrionSpectrum,
    LogitechGPro,
};

enum class KeyboardBackendId : std::uint8_t {
    Unknown = 0,
    AlienwareAWx10KFamily,
    BeQuietMountFamily,
    LogitechHIDPP20Family,
};

struct KeyboardIdentity {
    KeyboardModelId model;
    KeyboardBackendId backend;
    const char* manufacturer;
    const char* modelName;
    const char* displayName;
    std::uint16_t vendorId;
    std::uint16_t productId;
    int vendorInterface;
    int lampArrayInterface; // -1 when the model has no HID LampArray interface.
};

inline constexpr KeyboardIdentity kAlienwareAW410KIdentity{
    KeyboardModelId::AlienwareAW410K,
    KeyboardBackendId::AlienwareAWx10KFamily,
    "Alienware",
    "AW410K",
    "Alienware AW410K",
    0x04f2,
    0x1968,
    2,
    -1,
};

inline constexpr KeyboardIdentity kAlienwareAW510KIdentity{
    KeyboardModelId::AlienwareAW510K,
    KeyboardBackendId::AlienwareAWx10KFamily,
    "Alienware",
    "AW510K",
    "Alienware AW510K",
    0x04f2,
    0x1830,
    2,
    -1,
};

inline constexpr KeyboardIdentity kBeQuietLightMountIdentity{
    KeyboardModelId::BeQuietLightMount,
    KeyboardBackendId::BeQuietMountFamily,
    "be quiet!",
    "Light Mount",
    "be quiet! Light Mount",
    0x373f,
    0x0002,
    2,
    3,
};

// Logitech USB identities are only product metadata. HID++ feature discovery
// determines the actual lighting protocol and capabilities at runtime.
inline constexpr KeyboardIdentity kLogitechG213Identity{
    KeyboardModelId::LogitechG213Prodigy, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G213 Prodigy", "Logitech G213 Prodigy", 0x046d, 0xc336, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG410Identity{
    KeyboardModelId::LogitechG410AtlasSpectrum, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G410 Atlas Spectrum", "Logitech G410 Atlas Spectrum", 0x046d, 0xc330, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG413Identity{
    KeyboardModelId::LogitechG413Carbon, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G413 Carbon", "Logitech G413 Carbon", 0x046d, 0xc33a, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG512Identity{
    KeyboardModelId::LogitechG512Carbon, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G512 Carbon", "Logitech G512 Carbon", 0x046d, 0xc342, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG513Identity{
    KeyboardModelId::LogitechG513Carbon, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G513 Carbon", "Logitech G513 Carbon", 0x046d, 0xc33c, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG610C333Identity{
    KeyboardModelId::LogitechG610Orion, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G610 Orion", "Logitech G610 Orion", 0x046d, 0xc333, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG610C338Identity{
    KeyboardModelId::LogitechG610Orion, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G610 Orion", "Logitech G610 Orion", 0x046d, 0xc338, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG810C331Identity{
    KeyboardModelId::LogitechG810OrionSpectrum, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G810 Orion Spectrum", "Logitech G810 Orion Spectrum", 0x046d, 0xc331, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG810C337Identity{
    KeyboardModelId::LogitechG810OrionSpectrum, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G810 Orion Spectrum", "Logitech G810 Orion Spectrum", 0x046d, 0xc337, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG815Identity{
    KeyboardModelId::LogitechG815Lightsync, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G815 LIGHTSYNC", "Logitech G815 LIGHTSYNC", 0x046d, 0xc33f, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG910SparkIdentity{
    KeyboardModelId::LogitechG910OrionSpark, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G910 Orion Spark", "Logitech G910 Orion Spark", 0x046d, 0xc32b, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechG910SpectrumIdentity{
    KeyboardModelId::LogitechG910OrionSpectrum, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G910 Orion Spectrum", "Logitech G910 Orion Spectrum", 0x046d, 0xc335, 1, -1,
};
inline constexpr KeyboardIdentity kLogitechGProIdentity{
    KeyboardModelId::LogitechGPro, KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech", "G PRO", "Logitech G PRO", 0x046d, 0xc339, 1, -1,
};

inline constexpr std::array<const KeyboardIdentity*, 16> kKeyboardIdentities{{
    &kAlienwareAW410KIdentity,
    &kAlienwareAW510KIdentity,
    &kBeQuietLightMountIdentity,
    &kLogitechG213Identity,
    &kLogitechG410Identity,
    &kLogitechG413Identity,
    &kLogitechG512Identity,
    &kLogitechG513Identity,
    &kLogitechG610C333Identity,
    &kLogitechG610C338Identity,
    &kLogitechG810C331Identity,
    &kLogitechG810C337Identity,
    &kLogitechG815Identity,
    &kLogitechG910SparkIdentity,
    &kLogitechG910SpectrumIdentity,
    &kLogitechGProIdentity,
}};

inline constexpr const KeyboardIdentity* keyboardIdentityForUsb(
    std::uint16_t vendorId, std::uint16_t productId) {

    for(const KeyboardIdentity* identity : kKeyboardIdentities) {
        if(identity->vendorId == vendorId && identity->productId == productId) {
            return identity;
        }
    }
    return nullptr;
}

} // namespace krgb
