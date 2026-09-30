#pragma once

#include <array>
#include <cstdint>

namespace krgb {

enum class LogitechKnownModel : std::uint8_t {
    Unknown = 0,
    G213Prodigy,
    G410AtlasSpectrum,
    G413Carbon,
    G512Carbon,
    G513Carbon,
    G610Orion,
    G810OrionSpectrum,
    G815Lightsync,
    G910OrionSpark,
    G910OrionSpectrum,
    GPro,
};

enum class LogitechLightingColorCapability : std::uint8_t {
    Unknown = 0,
    Monochrome,
    Rgb,
};

// USB identity is not used to choose a HID++ lighting protocol.  HID++ ROOT
// feature discovery remains authoritative for protocol/capabilities.  This
// table only supplies human-readable fallback identity, udev-known PIDs and
// properties that the lighting feature set does not report (notably the
// physical LED colour capability on older devices).
struct LogitechKnownDevice {
    LogitechKnownModel model;
    std::uint16_t productId;
    const char* modelName;
    const char* displayName;
    LogitechLightingColorCapability colorCapability;
};

inline constexpr std::array<LogitechKnownDevice, 13> kLogitechKnownDevices{{
    { LogitechKnownModel::G213Prodigy,       0xc336, "G213 Prodigy",        "Logitech G213 Prodigy",        LogitechLightingColorCapability::Rgb },
    { LogitechKnownModel::G410AtlasSpectrum, 0xc330, "G410 Atlas Spectrum", "Logitech G410 Atlas Spectrum", LogitechLightingColorCapability::Rgb },
    { LogitechKnownModel::G413Carbon,        0xc33a, "G413 Carbon",         "Logitech G413 Carbon",         LogitechLightingColorCapability::Monochrome },
    { LogitechKnownModel::G512Carbon,        0xc342, "G512 Carbon",         "Logitech G512 Carbon",         LogitechLightingColorCapability::Rgb },
    { LogitechKnownModel::G513Carbon,        0xc33c, "G513 Carbon",         "Logitech G513 Carbon",         LogitechLightingColorCapability::Rgb },
    { LogitechKnownModel::G610Orion,         0xc333, "G610 Orion",          "Logitech G610 Orion",          LogitechLightingColorCapability::Monochrome },
    { LogitechKnownModel::G610Orion,         0xc338, "G610 Orion",          "Logitech G610 Orion",          LogitechLightingColorCapability::Monochrome },
    { LogitechKnownModel::G810OrionSpectrum, 0xc331, "G810 Orion Spectrum", "Logitech G810 Orion Spectrum", LogitechLightingColorCapability::Rgb },
    { LogitechKnownModel::G810OrionSpectrum, 0xc337, "G810 Orion Spectrum", "Logitech G810 Orion Spectrum", LogitechLightingColorCapability::Rgb },
    { LogitechKnownModel::G815Lightsync,      0xc33f, "G815 LIGHTSYNC",      "Logitech G815 LIGHTSYNC",      LogitechLightingColorCapability::Rgb },
    { LogitechKnownModel::G910OrionSpark,    0xc32b, "G910 Orion Spark",    "Logitech G910 Orion Spark",    LogitechLightingColorCapability::Rgb },
    { LogitechKnownModel::G910OrionSpectrum, 0xc335, "G910 Orion Spectrum", "Logitech G910 Orion Spectrum", LogitechLightingColorCapability::Rgb },
    { LogitechKnownModel::GPro,               0xc339, "G PRO",               "Logitech G PRO",               LogitechLightingColorCapability::Rgb },
}};

inline constexpr const LogitechKnownDevice* logitechKnownDeviceForProductId(
    std::uint16_t productId) {
    for(const auto& device : kLogitechKnownDevices) {
        if(device.productId == productId) {
            return &device;
        }
    }
    return nullptr;
}

} // namespace krgb
