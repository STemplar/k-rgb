#pragma once

#include <cstdint>
#include <string_view>

namespace krgb {

// Protocol metadata only. These are HID++ 2.0 feature identifiers, not
// model-specific capabilities. Presence is always decided from the device's
// runtime Feature Set (0x0001); this table only gives discovered IDs names and
// broad domains for diagnostics.
enum class LogitechHIDPP20FeatureDomain : std::uint8_t {
    Core,
    Power,
    Connection,
    Lighting,
    Input,
    Pointer,
    Keyboard,
    Touchpad,
    Gaming,
    Audio,
    Other,
};

struct LogitechHIDPP20FeatureCatalogEntry {
    std::uint16_t id;
    std::string_view name;
    LogitechHIDPP20FeatureDomain domain;
};

inline constexpr LogitechHIDPP20FeatureCatalogEntry kLogitechHIDPP20FeatureCatalog[] = {
    {0x0000, "Root", LogitechHIDPP20FeatureDomain::Core},
    {0x0001, "FeatureSet", LogitechHIDPP20FeatureDomain::Core},
    {0x0002, "FeatureInfo", LogitechHIDPP20FeatureDomain::Core},
    {0x0003, "DeviceInformation", LogitechHIDPP20FeatureDomain::Core},
    {0x0004, "UnitId", LogitechHIDPP20FeatureDomain::Core},
    {0x0005, "DeviceTypeAndName", LogitechHIDPP20FeatureDomain::Core},
    {0x0006, "DeviceGroups", LogitechHIDPP20FeatureDomain::Core},
    {0x0007, "DeviceFriendlyName", LogitechHIDPP20FeatureDomain::Core},
    {0x0008, "KeepAlive", LogitechHIDPP20FeatureDomain::Core},
    {0x0020, "ConfigChange", LogitechHIDPP20FeatureDomain::Core},
    {0x0021, "UniqueRandomId", LogitechHIDPP20FeatureDomain::Core},
    {0x0030, "TargetSoftware", LogitechHIDPP20FeatureDomain::Core},
    {0x0080, "WirelessSignalStrength", LogitechHIDPP20FeatureDomain::Connection},
    {0x00c0, "DfuControlLegacy", LogitechHIDPP20FeatureDomain::Core},
    {0x00c1, "DfuControlUnsigned", LogitechHIDPP20FeatureDomain::Core},
    {0x00c2, "DfuControlSigned", LogitechHIDPP20FeatureDomain::Core},
    {0x00c3, "DfuControlBolt", LogitechHIDPP20FeatureDomain::Core},
    {0x00d0, "Dfu", LogitechHIDPP20FeatureDomain::Core},
    {0x00d1, "DfuResumable", LogitechHIDPP20FeatureDomain::Core},

    {0x1000, "BatteryStatus", LogitechHIDPP20FeatureDomain::Power},
    {0x1001, "BatteryVoltage", LogitechHIDPP20FeatureDomain::Power},
    {0x1004, "UnifiedBattery", LogitechHIDPP20FeatureDomain::Power},
    {0x1010, "ChargingControl", LogitechHIDPP20FeatureDomain::Power},
    {0x1300, "LedControl", LogitechHIDPP20FeatureDomain::Lighting},

    {0x1800, "GenericTest", LogitechHIDPP20FeatureDomain::Other},
    {0x1802, "DeviceReset", LogitechHIDPP20FeatureDomain::Core},
    {0x1805, "OobState", LogitechHIDPP20FeatureDomain::Core},
    {0x1806, "ConfigDeviceProps", LogitechHIDPP20FeatureDomain::Core},
    {0x1814, "ChangeHost", LogitechHIDPP20FeatureDomain::Connection},
    {0x1815, "HostsInfo", LogitechHIDPP20FeatureDomain::Connection},

    {0x1981, "Backlight1", LogitechHIDPP20FeatureDomain::Lighting},
    {0x1982, "Backlight2", LogitechHIDPP20FeatureDomain::Lighting},
    {0x1983, "Backlight3", LogitechHIDPP20FeatureDomain::Lighting},
    {0x1990, "Illumination", LogitechHIDPP20FeatureDomain::Lighting},
    {0x19b0, "HapticFeedback", LogitechHIDPP20FeatureDomain::Other},
    {0x19c0, "ForceSensingButton", LogitechHIDPP20FeatureDomain::Input},
    {0x1a00, "PresenterControl", LogitechHIDPP20FeatureDomain::Input},
    {0x1a01, "Sensor3D", LogitechHIDPP20FeatureDomain::Input},

    {0x1b00, "ReprogControls", LogitechHIDPP20FeatureDomain::Input},
    {0x1b01, "ReprogControls2", LogitechHIDPP20FeatureDomain::Input},
    {0x1b02, "ReprogControls3", LogitechHIDPP20FeatureDomain::Input},
    {0x1b03, "ReprogControls4", LogitechHIDPP20FeatureDomain::Input},
    {0x1b04, "ReprogControls5", LogitechHIDPP20FeatureDomain::Input},
    {0x1bc0, "ReportHidUsages", LogitechHIDPP20FeatureDomain::Input},
    {0x1c00, "PersistentRemappableAction", LogitechHIDPP20FeatureDomain::Input},
    {0x1d4b, "WirelessDeviceStatus", LogitechHIDPP20FeatureDomain::Connection},
    {0x1df0, "RemainingPairings", LogitechHIDPP20FeatureDomain::Connection},
    {0x1f1f, "FirmwareProperties", LogitechHIDPP20FeatureDomain::Core},
    {0x1f20, "AdcMeasurement", LogitechHIDPP20FeatureDomain::Power},

    {0x2001, "SwapLeftRightButton", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2005, "ButtonSwapCancel", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2006, "PointerAxesOrientation", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2100, "VerticalScrolling", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2110, "SmartShiftWheel", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2111, "SmartShiftWheelEnhanced", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2120, "HighResolutionScrolling", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2121, "HiResWheel", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2130, "RatchetWheel", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2150, "Thumbwheel", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2200, "MousePointer", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2201, "AdjustableDpi", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2202, "ExtendedAdjustableDpi", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2205, "PointerMotionScaling", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2230, "SensorAngleSnapping", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2240, "SurfaceTuning", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2250, "XyStats", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2251, "WheelStats", LogitechHIDPP20FeatureDomain::Pointer},
    {0x2400, "HybridTrackingEngine", LogitechHIDPP20FeatureDomain::Pointer},

    {0x40a0, "FnInversion", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x40a2, "FnInversionWithDefaultState", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x40a3, "FnInversionForMultiHostDevices", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4100, "Encryption", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4220, "LockKeyState", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4301, "SolarKeyboardDashboard", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4520, "KeyboardLayout", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4521, "DisableKeys", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4522, "DisableKeysByUsage", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4530, "DualPlatform", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4531, "MultiPlatform", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4540, "KeyboardInternationalLayouts", LogitechHIDPP20FeatureDomain::Keyboard},
    {0x4600, "Crown", LogitechHIDPP20FeatureDomain::Keyboard},

    {0x6010, "TouchpadFwItems", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6011, "TouchpadSwItems", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6012, "TouchpadWin8FwItems", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6020, "TapEnable", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6021, "TapEnableExtended", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6030, "CursorBallistic", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6040, "TouchpadResolutionDivider", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6100, "TouchpadRawXy", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6110, "TouchMouseRawTouchPoints", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6120, "BtTouchMouseSettings", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6500, "Gestures1", LogitechHIDPP20FeatureDomain::Touchpad},
    {0x6501, "Gestures2", LogitechHIDPP20FeatureDomain::Touchpad},

    {0x8010, "GamingGKeys", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8020, "GamingMKeys", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8030, "MacroRecord", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8040, "BrightnessControl", LogitechHIDPP20FeatureDomain::Lighting},
    {0x8060, "AdjustableReportRate", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8061, "ExtendedAdjustableReportRate", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8070, "ColorLedEffects", LogitechHIDPP20FeatureDomain::Lighting},
    {0x8071, "RgbEffects", LogitechHIDPP20FeatureDomain::Lighting},
    {0x8080, "PerKeyLighting", LogitechHIDPP20FeatureDomain::Lighting},
    {0x8081, "PerKeyLighting2", LogitechHIDPP20FeatureDomain::Lighting},
    {0x8090, "ModeStatus", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8100, "OnboardProfiles", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8110, "MouseButtonFilter", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8111, "LatencyMonitoring", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8120, "GamingAttachments", LogitechHIDPP20FeatureDomain::Gaming},
    {0x8123, "ForceFeedback", LogitechHIDPP20FeatureDomain::Gaming},

    {0x8300, "Sidetone", LogitechHIDPP20FeatureDomain::Audio},
    {0x8310, "Equalizer", LogitechHIDPP20FeatureDomain::Audio},
    {0x8320, "HeadsetOut", LogitechHIDPP20FeatureDomain::Audio},
};

inline constexpr const LogitechHIDPP20FeatureCatalogEntry*
logitechHIDPP20FeatureCatalogEntry(std::uint16_t id) {
    for(const auto& entry : kLogitechHIDPP20FeatureCatalog) {
        if(entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

inline constexpr std::string_view logitechHIDPP20FeatureName(std::uint16_t id) {
    const auto* entry = logitechHIDPP20FeatureCatalogEntry(id);
    return entry ? entry->name : std::string_view{"Unknown"};
}

inline constexpr std::string_view logitechHIDPP20FeatureDomainName(
    LogitechHIDPP20FeatureDomain domain) {
    switch(domain) {
        case LogitechHIDPP20FeatureDomain::Core: return "core";
        case LogitechHIDPP20FeatureDomain::Power: return "power";
        case LogitechHIDPP20FeatureDomain::Connection: return "connection";
        case LogitechHIDPP20FeatureDomain::Lighting: return "lighting";
        case LogitechHIDPP20FeatureDomain::Input: return "input";
        case LogitechHIDPP20FeatureDomain::Pointer: return "pointer";
        case LogitechHIDPP20FeatureDomain::Keyboard: return "keyboard";
        case LogitechHIDPP20FeatureDomain::Touchpad: return "touchpad";
        case LogitechHIDPP20FeatureDomain::Gaming: return "gaming";
        case LogitechHIDPP20FeatureDomain::Audio: return "audio";
        case LogitechHIDPP20FeatureDomain::Other: return "other";
    }
    return "other";
}

} // namespace krgb
