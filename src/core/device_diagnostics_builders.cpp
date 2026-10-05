#include "core/device_diagnostics_builders.h"

#include "core/hid_lamp_array_device.h"
#include "core/lightmount_device.h"
#include "core/lightmount_keymap.h"
#include "core/lightmount_map.h"
#include "core/logitech_hidpp20_device.h"
#include "core/logitech_hidpp20_feature_catalog.h"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace krgb {

namespace {

std::string hexValue(std::uint64_t value, int width) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << std::setfill('0')
        << std::setw(width) << value;
    return out.str();
}

std::string readTextFile(const fs::path& path) {
    std::ifstream in(path);
    if(!in) {
        return {};
    }
    std::string value;
    std::getline(in, value);
    while(!value.empty() &&
          (value.back() == '\r' || value.back() == '\n' ||
           value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

struct UsbTextIdentity {
    std::string manufacturer;
    std::string product;
    std::string serial;
    std::string bcdDevice;
};

UsbTextIdentity usbIdentityForHidraw(const std::string& devicePath) {
    UsbTextIdentity identity;
    const fs::path dev(devicePath);
    const std::string name = dev.filename().string();
    if(name.empty()) {
        return identity;
    }

    std::error_code ec;
    fs::path current = fs::canonical(fs::path("/sys/class/hidraw") / name / "device", ec);
    if(ec) {
        return identity;
    }

    for(int depth = 0; depth < 10 && !current.empty(); ++depth) {
        if(identity.manufacturer.empty()) {
            identity.manufacturer = readTextFile(current / "manufacturer");
        }
        if(identity.product.empty()) {
            identity.product = readTextFile(current / "product");
        }
        if(identity.serial.empty()) {
            identity.serial = readTextFile(current / "serial");
        }
        if(identity.bcdDevice.empty()) {
            identity.bcdDevice = readTextFile(current / "bcdDevice");
        }
        current = current.parent_path();
    }
    return identity;
}

std::string lightMountFirmwareFromBcdDevice(const std::string& bcd) {
    if(bcd.size() != 4) {
        return bcd;
    }

    // Light Mount bcdDevice 0x2300 corresponds to IO Center firmware 2.3.0.
    // Keep this conversion local to the known device rather than treating it
    // as a generic USB versioning rule.
    const char major = bcd[0];
    const char minor = bcd[1];
    const std::string patchText = bcd.substr(2);
    char* end = nullptr;
    errno = 0;
    const long patch = std::strtol(patchText.c_str(), &end, 16);
    if(end == patchText.c_str() || *end != '\0' || errno != 0) {
        return bcd;
    }

    std::ostringstream out;
    out << major << '.' << minor << '.' << patch;
    return out.str();
}

std::uint16_t crc16Modbus(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0xffff;
    for(std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for(int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u)
                ? static_cast<std::uint16_t>((crc >> 1) ^ 0xa001)
                : static_cast<std::uint16_t>(crc >> 1);
        }
    }
    return crc;
}

bool readLightMountIllumination(const std::string& devicePath,
                                bool& illumination,
                                std::string* err) {
    illumination = false;
    const int fd = ::open(devicePath.c_str(), O_RDWR | O_NONBLOCK);
    if(fd < 0) {
        if(err) {
            *err = "open " + devicePath + ": " + std::strerror(errno);
        }
        return false;
    }

    std::array<std::uint8_t, LightMountDevice::kReportLen> request{};
    request[0] = 0x06;
    request[2] = 0x01;
    request[4] = 0x71;
    request[5] = 0x10;
    request[6] = 0x01;
    const std::uint16_t crc = crc16Modbus(request.data(), 62);
    request[62] = static_cast<std::uint8_t>(crc & 0xff);
    request[63] = static_cast<std::uint8_t>((crc >> 8) & 0xff);

    if(::write(fd, request.data(), request.size()) !=
       static_cast<ssize_t>(request.size())) {
        if(err) {
            *err = "Light Mount illumination query write failed";
        }
        ::close(fd);
        return false;
    }

    pollfd pfd{};
    pfd.fd = fd;
    pfd.events = POLLIN;

    for(int attempt = 0; attempt < 6; ++attempt) {
        const int ready = ::poll(&pfd, 1, 200);
        if(ready <= 0) {
            continue;
        }

        std::array<std::uint8_t, LightMountDevice::kReportLen> response{};
        const ssize_t count = ::read(fd, response.data(), response.size());
        if(count < 8) {
            continue;
        }
        if(response[4] != request[4] || response[5] != 0x10 ||
           response[6] != 0x01) {
            continue;
        }

        illumination = response[7] != 0;
        ::close(fd);
        return true;
    }

    ::close(fd);
    if(err) {
        *err = "Light Mount illumination query timed out";
    }
    return false;
}

std::string featureFlags(const LogitechHIDPP20Feature& feature) {
    std::vector<std::string> flags;
    if(feature.isObsolete()) flags.emplace_back("obsolete");
    if(feature.isHidden()) flags.emplace_back("hidden");
    if(feature.isEngineering()) flags.emplace_back("engineering");
    if(feature.isManufacturingDeactivatable()) flags.emplace_back("manufacturing");
    if(feature.isComplianceDeactivatable()) flags.emplace_back("compliance");
    if(flags.empty()) return "normal";

    std::ostringstream out;
    for(std::size_t i = 0; i < flags.size(); ++i) {
        if(i != 0) out << ',';
        out << flags[i];
    }
    return out.str();
}

std::string reportRatesFromMask(std::uint16_t mask) {
    static constexpr unsigned rates[] = {125, 250, 500, 1000, 2000, 4000, 8000};
    std::ostringstream out;
    bool first = true;
    for(unsigned bit = 0; bit < 7; ++bit) {
        if((mask & (1u << bit)) == 0) continue;
        if(!first) out << ", ";
        out << rates[bit] << " Hz";
        first = false;
    }
    return first ? std::string("-") : out.str();
}

} // namespace

bool buildLightMountDiagnostics(DeviceDiagnostics& diagnostics,
                                std::string* err) {
    diagnostics = {};

    const std::string vendorPath = LightMountDevice::findDevicePath();
    if(vendorPath.empty()) {
        if(err) *err = "No be quiet! Light Mount vendor HID interface found";
        return false;
    }

    const UsbTextIdentity usb = usbIdentityForHidraw(vendorPath);

    DiagnosticSection device;
    device.title = "Device";
    device.fields = {
        {"Manufacturer", usb.manufacturer.empty() ? "be quiet!" : usb.manufacturer},
        {"Model", usb.product.empty() ? "Light Mount" : usb.product},
        {"VID:PID", "373f:0002"},
        {"Protocol", "be quiet! Mount vendor HID"},
        {"Device node", vendorPath},
    };
    if(!usb.bcdDevice.empty()) {
        device.fields.push_back({"Firmware", lightMountFirmwareFromBcdDevice(usb.bcdDevice)});
    }
    if(!usb.serial.empty()) {
        device.fields.push_back({"Serial", usb.serial});
    }

    bool illumination = false;
    std::string illuminationError;
    if(readLightMountIllumination(vendorPath, illumination, &illuminationError)) {
        device.fields.push_back({"Illumination", illumination ? "On" : "Off"});
    } else {
        device.fields.push_back({"Illumination", "Unavailable"});
        device.notes.push_back("Illumination read: " + illuminationError);
    }
    diagnostics.sections.push_back(std::move(device));

    DiagnosticSection lampArray;
    lampArray.title = "HID LampArray";
    lampArray.fields = {
        {"Interface", "3"},
        {"RGB elements", std::to_string(lightmount::kRgbElementCount)},
    };

    HIDLampArrayDevice lamp;
    std::string lampError;
    if(lamp.open(LightMountDevice::kVendorId,
                 LightMountDevice::kProductId,
                 3, &lampError)) {
        HIDLampArrayAttributes attrs;
        if(lamp.getAttributes(attrs)) {
            lampArray.fields.push_back({"Reported lamps", std::to_string(attrs.lampCount)});
            lampArray.fields.push_back({"LampArray node", lamp.path()});
        }
    } else {
        lampArray.notes.push_back("LampArray query: " + lampError);
    }

    DiagnosticTable zones;
    zones.headers = {"Zone", "LEDs"};
    zones.rows = {
        {"Top bar", std::to_string(lightmount::kTopBarCount)},
        {"3D Media Wheel", "1"},
        {"Keyboard", "109"},
        {"Left strip", std::to_string(lightmount::kLeftStripCount)},
        {"Right strip", std::to_string(lightmount::kRightStripCount)},
    };
    lampArray.tables.push_back(std::move(zones));
    diagnostics.sections.push_back(std::move(lampArray));

    DiagnosticSection controls;
    controls.title = "Programmable controls";
    DiagnosticTable mappings;
    mappings.headers = {"Control", "Factory mapping"};
    mappings.rows = {
        {"M1", "Media: Play/Pause"},
        {"M2", "Media: Next Track"},
        {"M3", "Media: Previous Track"},
        {"M4", "Media: Stop"},
        {"M5", "Media: Mic Mute"},
        {"Dial Rotate Right", "Media: Volume +"},
        {"Dial Rotate Left", "Media: Volume -"},
        {"Dial Press", "Media: Mute"},
    };
    controls.tables.push_back(std::move(mappings));
    controls.notes.push_back(
        "Current Light Mount mapping readback is not decoded yet; the table shows factory mappings.");
    diagnostics.sections.push_back(std::move(controls));

    return true;
}

bool buildLogitechHIDPP20Diagnostics(DeviceDiagnostics& diagnostics,
                                     std::string* err) {
    diagnostics = {};

    LogitechHIDPP20Device dev;
    if(!dev.openKeyboard(err)) {
        return false;
    }

    LogitechHIDPP20Capabilities capabilities;
    if(!dev.getCapabilities(capabilities, err)) {
        return false;
    }

    const auto& usb = dev.usbIdentity();
    DiagnosticSection device;
    device.title = "Device";
    device.fields = {
        {"Manufacturer", usb.manufacturer.empty() ? "Logitech" : usb.manufacturer},
        {"Model", dev.displayName()},
        {"VID:PID", hexValue(usb.vendorId, 4) + ":" + hexValue(usb.productId, 4)},
        {"Protocol", "HID++ " + std::to_string(capabilities.protocol.protocolNumber) + ".0"},
        {"Device node", dev.path()},
    };
    if(!usb.serial.empty()) {
        device.fields.push_back({"Serial", usb.serial});
    }

    std::vector<LogitechHIDPP20FirmwareInfo> firmware;
    std::string localError;
    if(dev.getFirmwareInfo(firmware, &localError)) {
        DiagnosticTable fwTable;
        fwTable.headers = {"Entity", "Type", "Version"};
        for(const auto& item : firmware) {
            std::string kind = "Other";
            if(item.kind == 0x00) kind = "Main application";
            else if(item.kind == 0x01) kind = "Bootloader";
            else if(item.kind == 0x02) kind = "Hardware";

            std::ostringstream version;
            if(item.kind == 0x02) {
                version << "revision " << static_cast<unsigned>(item.major);
            } else {
                if(!item.name.empty()) version << item.name << ' ';
                version << std::hex << std::uppercase << std::setfill('0')
                        << std::setw(2) << static_cast<unsigned>(item.major)
                        << '.' << std::setw(2) << static_cast<unsigned>(item.minor);
                if(item.build != 0) {
                    version << ".B" << std::setw(4)
                            << static_cast<unsigned>(item.build);
                }
            }
            fwTable.rows.push_back({
                std::to_string(item.entity), kind, version.str()
            });
        }
        device.tables.push_back(std::move(fwTable));
    } else if(!localError.empty()) {
        device.notes.push_back("Firmware query: " + localError);
    }
    diagnostics.sections.push_back(std::move(device));

    DiagnosticSection lighting;
    lighting.title = "Lighting";
    const auto color = dev.colorCapability();
    lighting.fields.push_back({
        "Colour capability",
        color == LogitechLightingColorCapability::Rgb ? "RGB" :
        color == LogitechLightingColorCapability::Monochrome ? "Monochrome" : "Unknown"
    });

    LogitechHIDPP20BrightnessInfo brightness;
    localError.clear();
    if(capabilities.findFeature(LogitechHIDPP20Device::kFeatureBrightnessControl) &&
       dev.getBrightnessInfo(brightness, &localError)) {
        lighting.fields.push_back({
            "Brightness",
            std::to_string(brightness.minimum) + ".." +
            std::to_string(brightness.maximum) +
            (brightness.hasCurrent
                ? " (current " + std::to_string(brightness.current) + ")"
                : std::string())
        });
    }

    LogitechHIDPP20PerKeyInfo perKey;
    localError.clear();
    if(capabilities.findFeature(LogitechHIDPP20Device::kFeaturePerKeyLighting) &&
       dev.getPerKey8080Info(perKey, &localError)) {
        std::size_t reported = 0;
        std::size_t discovered = 0;
        for(const auto& type : perKey.types) {
            reported += type.keyCount;
            discovered += type.colors.size();
        }
        lighting.fields.push_back({"0x8080 reported elements", std::to_string(reported)});
        lighting.fields.push_back({"0x8080 discovered addresses", std::to_string(discovered)});
    }

    LogitechHIDPP20PerKey8081Info perKey2;
    localError.clear();
    if(capabilities.findFeature(LogitechHIDPP20Device::kFeaturePerKeyLighting2) &&
       dev.getPerKey8081Info(perKey2, &localError)) {
        lighting.fields.push_back({"0x8081 RGB zones", std::to_string(perKey2.zoneIds.size())});
    }

    LogitechHIDPP20ColorLedInfo colorLed;
    localError.clear();
    if(capabilities.findFeature(LogitechHIDPP20Device::kFeatureColorLedEffects) &&
       dev.getColorLed8070Info(colorLed, &localError)) {
        lighting.fields.push_back({"0x8070 effect zones", std::to_string(colorLed.zoneCount)});
    }

    LogitechHIDPP20RgbEffectsInfo rgbEffects;
    localError.clear();
    if(capabilities.findFeature(LogitechHIDPP20Device::kFeatureRgbEffects) &&
       dev.getRgbEffects8071Info(rgbEffects, &localError)) {
        lighting.fields.push_back({"0x8071 RGB clusters", std::to_string(rgbEffects.clusterCount)});
    }
    diagnostics.sections.push_back(std::move(lighting));

    DiagnosticSection controls;
    controls.title = "Programmable controls";
    bool hasControls = false;
    for(std::uint16_t id = 0x1b00; id <= 0x1b04; ++id) {
        if(capabilities.findFeature(id)) {
            hasControls = true;
            break;
        }
    }
    if(hasControls) {
        LogitechHIDPP20ControlsInfo controlInfo;
        localError.clear();
        if(dev.getReprogrammableControls(controlInfo, &localError)) {
            DiagnosticTable table;
            table.headers = {"CID", "Task", "Flags", "Position", "Group"};
            for(const auto& control : controlInfo.controls) {
                table.rows.push_back({
                    "0x" + hexValue(control.controlId, 4),
                    "0x" + hexValue(control.taskId, 4),
                    "0x" + hexValue(control.flags, 4),
                    std::to_string(control.position),
                    std::to_string(control.group),
                });
            }
            controls.tables.push_back(std::move(table));
        } else {
            controls.notes.push_back("Control query: " + localError);
        }
    } else {
        controls.notes.push_back("No 0x1B00..0x1B04 reprogrammable-control feature reported.");
    }
    if(capabilities.findFeature(0x1c00)) {
        controls.notes.push_back(
            "0x1C00 PersistentRemappableAction is present; detailed persistent mapping readback is not implemented yet.");
    }
    diagnostics.sections.push_back(std::move(controls));

    DiagnosticSection features;
    features.title = "HID++ features";
    DiagnosticTable featureTable;
    featureTable.headers = {"ID", "Ver", "Domain", "Feature", "Flags"};
    for(const auto& feature : capabilities.features) {
        const auto* catalog = logitechHIDPP20FeatureCatalogEntry(feature.featureId);
        const std::string name = catalog
            ? std::string(catalog->name) : std::string("Unknown");
        const std::string domain = catalog
            ? std::string(logitechHIDPP20FeatureDomainName(catalog->domain))
            : std::string("unknown");
        featureTable.rows.push_back({
            "0x" + hexValue(feature.featureId, 4),
            feature.versionKnown ? std::to_string(feature.version) : "-",
            domain,
            name,
            featureFlags(feature),
        });
    }
    features.tables.push_back(std::move(featureTable));
    diagnostics.sections.push_back(std::move(features));

    DiagnosticSection connection;
    connection.title = "Connection / report rate";
    connection.fields.push_back({"USB interface", std::to_string(usb.interfaceNumber)});
    LogitechHIDPP20ReportRateInfo rate;
    localError.clear();
    if((capabilities.findFeature(LogitechHIDPP20Device::kFeatureAdjustableReportRate) ||
        capabilities.findFeature(LogitechHIDPP20Device::kFeatureExtendedAdjustableReportRate)) &&
       dev.getReportRateInfo(rate, &localError)) {
        connection.fields.push_back({"Supported", reportRatesFromMask(rate.supportedMask)});
        if(rate.hasCurrent) {
            connection.fields.push_back({"Current raw value", std::to_string(rate.current)});
        }
    } else if(!localError.empty()) {
        connection.notes.push_back("Report-rate query: " + localError);
    }
    diagnostics.sections.push_back(std::move(connection));

    return true;
}

} // namespace krgb
