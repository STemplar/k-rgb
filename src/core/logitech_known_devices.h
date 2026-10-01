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

enum class LogitechLightingTopologyHint : std::uint8_t {
    Unknown = 0,
    ZoneRgb,
    PerKey,
};

enum class LogitechProtocolEvidence : std::uint8_t {
    IdentityOnly = 0,
    ResourceDerived,
    CaptureDerived,
    HardwareVerified,
};

// USB identity is not used to choose a HID++ lighting protocol.  HID++ ROOT
// feature discovery remains authoritative for protocol/capabilities.  This
// table only supplies human-readable fallback identity, udev-known PIDs and
// properties that the lighting feature set does not report (notably physical
// LED colour capability and externally established topology hints). Runtime
// HID++ feature discovery remains authoritative.
struct LogitechKnownDevice {
    LogitechKnownModel model;
    std::uint16_t productId;
    const char* modelName;
    const char* displayName;
    LogitechLightingColorCapability colorCapability;
    LogitechLightingTopologyHint topologyHint;
    std::uint8_t expectedZoneCount;
    LogitechProtocolEvidence protocolEvidence;
    const char* evidenceNote;

    // Most documented 0x8070 implementations address zones from zero. The
    // G213 packet captures use protocol region IDs 1..5, so its dump-derived
    // mapping has a base of one. This is a model quirk, not feature discovery.
    std::uint8_t colorLed8070ZoneAddressBase;
};

inline constexpr std::array<LogitechKnownDevice, 13> kLogitechKnownDevices{{
    { LogitechKnownModel::G213Prodigy,       0xc336, "G213 Prodigy",        "Logitech G213 Prodigy",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::ZoneRgb, 5,
      LogitechProtocolEvidence::CaptureDerived,
      "G213 Region1..Region5/fixed-color packet captures; hardware-unverified in k-rgb", 1 },

    { LogitechKnownModel::G410AtlasSpectrum, 0xc330, "G410 Atlas Spectrum", "Logitech G410 Atlas Spectrum",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::ResourceDerived,
      "LGS per-key resources; hardware-unverified in k-rgb", 0 },

    { LogitechKnownModel::G413Carbon,        0xc33a, "G413 Carbon",         "Logitech G413 Carbon",
      LogitechLightingColorCapability::Monochrome, LogitechLightingTopologyHint::Unknown, 0,
      LogitechProtocolEvidence::CaptureDerived,
      "G413 brightness and breathing packet captures; hardware-unverified in k-rgb", 0 },

    { LogitechKnownModel::G512Carbon,        0xc342, "G512 Carbon",         "Logitech G512 Carbon",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::IdentityOnly,
      "known identity/third-party support reference; no packet dump in current evidence set", 0 },

    { LogitechKnownModel::G513Carbon,        0xc33c, "G513 Carbon",         "Logitech G513 Carbon",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::IdentityOnly,
      "known identity/third-party support reference; no packet dump in current evidence set", 0 },

    { LogitechKnownModel::G610Orion,         0xc333, "G610 Orion",          "Logitech G610 Orion",
      LogitechLightingColorCapability::Monochrome, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::ResourceDerived,
      "LGS geometry/defaults plus independent reverse engineering; hardware-unverified", 0 },

    { LogitechKnownModel::G610Orion,         0xc338, "G610 Orion",          "Logitech G610 Orion",
      LogitechLightingColorCapability::Monochrome, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::ResourceDerived,
      "LGS geometry/defaults plus independent reverse engineering; hardware-unverified", 0 },

    { LogitechKnownModel::G810OrionSpectrum, 0xc331, "G810 Orion Spectrum", "Logitech G810 Orion Spectrum",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::HardwareVerified,
      "physical c331 verified in k-rgb; independent G810 packet captures also available", 0 },

    { LogitechKnownModel::G810OrionSpectrum, 0xc337, "G810 Orion Spectrum", "Logitech G810 Orion Spectrum",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::ResourceDerived,
      "LGS geometry/defaults; c337 hardware-unverified in k-rgb", 0 },

    { LogitechKnownModel::G815Lightsync,      0xc33f, "G815 LIGHTSYNC",      "Logitech G815 LIGHTSYNC",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::CaptureDerived,
      "G815 effects/key/logo/media/G-key packet captures and raw data; hardware-unverified", 0 },

    { LogitechKnownModel::G910OrionSpark,    0xc32b, "G910 Orion Spark",    "Logitech G910 Orion Spark",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::CaptureDerived,
      "G910 Spark blue/logo/G-key/M-key packet captures; hardware-unverified", 0 },

    { LogitechKnownModel::G910OrionSpectrum, 0xc335, "G910 Orion Spectrum", "Logitech G910 Orion Spectrum",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::ResourceDerived,
      "LGS G910v2 per-key resources and independent implementation; no matching packet dump", 0 },

    { LogitechKnownModel::GPro,               0xc339, "G PRO",               "Logitech G PRO",
      LogitechLightingColorCapability::Rgb, LogitechLightingTopologyHint::PerKey, 0,
      LogitechProtocolEvidence::ResourceDerived,
      "independent implementation plus visually confirmed ANSI87/ISO88 TKL form; hardware-unverified", 0 },
}};

inline constexpr const char* logitechProtocolEvidenceName(
    LogitechProtocolEvidence evidence) {
    switch(evidence) {
        case LogitechProtocolEvidence::IdentityOnly: return "identity/discovery only";
        case LogitechProtocolEvidence::ResourceDerived: return "resource/reference-derived";
        case LogitechProtocolEvidence::CaptureDerived: return "packet-capture-derived";
        case LogitechProtocolEvidence::HardwareVerified: return "hardware-verified";
    }
    return "unknown";
}

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
