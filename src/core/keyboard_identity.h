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
    LogitechG810OrionSpectrum,
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

// The G810 was shipped with two USB product IDs. They are the same logical
// keyboard model and share the HID++ 2.0 lighting backend.
inline constexpr KeyboardIdentity kLogitechG810C331Identity{
    KeyboardModelId::LogitechG810OrionSpectrum,
    KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech",
    "G810 Orion Spectrum",
    "Logitech G810 Orion Spectrum",
    0x046d,
    0xc331,
    1,
    -1,
};

inline constexpr KeyboardIdentity kLogitechG810C337Identity{
    KeyboardModelId::LogitechG810OrionSpectrum,
    KeyboardBackendId::LogitechHIDPP20Family,
    "Logitech",
    "G810 Orion Spectrum",
    "Logitech G810 Orion Spectrum",
    0x046d,
    0xc337,
    1,
    -1,
};

inline constexpr std::array<const KeyboardIdentity*, 5> kKeyboardIdentities{{
    &kAlienwareAW410KIdentity,
    &kAlienwareAW510KIdentity,
    &kBeQuietLightMountIdentity,
    &kLogitechG810C331Identity,
    &kLogitechG810C337Identity,
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
