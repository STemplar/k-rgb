// krgb-cli — command-line control and hardware diagnostics for supported RGB devices.
#include "core/aw410k_device.h"
#include "core/alienfx_device.h"
#include "core/keymap.h"
#include "core/hid_lamp_array_device.h"
#include "core/hid_keyboard_country.h"
#include "core/lightmount_device.h"
#include "core/lightmount_effects.h"
#include "core/lightmount_keymap.h"
#include "core/logitech_hidpp20_device.h"
#include "core/logitech_g610_g810_keymap.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace krgb;

namespace {

void hsv(double h, double s, double v, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    const double i = std::floor(h * 6.0);
    const double f = h * 6.0 - i;
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - f * s);
    const double t = v * (1.0 - (1.0 - f) * s);
    double rr = v, gg = t, bb = p;
    switch(static_cast<int>(i) % 6) {
        case 0: rr = v; gg = t; bb = p; break;
        case 1: rr = q; gg = v; bb = p; break;
        case 2: rr = p; gg = v; bb = t; break;
        case 3: rr = p; gg = q; bb = v; break;
        case 4: rr = t; gg = p; bb = v; break;
        default: rr = v; gg = p; bb = q; break;
    }
    r = static_cast<std::uint8_t>(rr * 255);
    g = static_cast<std::uint8_t>(gg * 255);
    b = static_cast<std::uint8_t>(bb * 255);
}

// Case-insensitive key-label -> hardware LED index lookup.
bool findKeyIdx(const std::string& name, std::uint8_t& idx) {
    for(std::size_t i = 0; i < kKeyCount; ++i) {
        if(strcasecmp(kKeyMap[i].name, name.c_str()) == 0) {
            idx = kKeyMap[i].idx;
            return true;
        }
    }
    return false;
}

bool findLightMountKey(const std::string& name, std::uint16_t& ledId) {
    for(const auto& key : lightmount::kKeys) {
        if(strcasecmp(key.name, name.c_str()) == 0) {
            ledId = key.ledId;
            return true;
        }
    }
    return false;
}

// Parse a colour spec: "#RRGGBB" or "R,G,B"/"R G B" (channels accept 0x.. too).
bool parseColorSpec(std::string spec, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    const std::size_t s = spec.find_first_not_of(" \t");
    if(s == std::string::npos) {
        return false;
    }
    spec = spec.substr(s);
    if(spec[0] == '#') {
        if(spec.size() < 7) {
            return false;
        }
        char* end = nullptr;
        const unsigned long v = std::strtoul(spec.substr(1, 6).c_str(), &end, 16);
        if(end && *end) {
            return false;
        }
        r = (v >> 16) & 0xFF;
        g = (v >> 8) & 0xFF;
        b = v & 0xFF;
        return true;
    }
    for(char& c : spec) {
        if(c == ',') {
            c = ' ';
        }
    }
    std::istringstream is(spec);
    long rr, gg, bb;
    if(!(is >> rr >> gg >> bb)) {
        return false;
    }
    if(rr < 0 || gg < 0 || bb < 0 || rr > 255 || gg > 255 || bb > 255) {
        return false;
    }
    r = static_cast<std::uint8_t>(rr);
    g = static_cast<std::uint8_t>(gg);
    b = static_cast<std::uint8_t>(bb);
    return true;
}


bool parseLightMountGradientStop(const std::string& spec, LightMountGradientStop& stop) {
    const std::size_t at = spec.rfind('@');
    if(at == std::string::npos || at == 0 || at + 1 >= spec.size()) {
        return false;
    }

    if(!parseColorSpec(spec.substr(0, at), stop.r, stop.g, stop.b)) {
        return false;
    }

    const std::string positionSpec = spec.substr(at + 1);
    char* end = nullptr;
    errno = 0;
    const long position = std::strtol(positionSpec.c_str(), &end, 0);
    if(end == positionSpec.c_str() || *end != '\0' || errno != 0 ||
       position < 0 || position > 100) {
        return false;
    }
    stop.position = static_cast<std::uint8_t>(position);
    return true;
}

// Load "KEY R G B" / "KEY=#RRGGBB" lines (# comments, blanks ok) from a file
// (or stdin when path is "-") into a key list. Returns false (after printing an
// error) on the first bad line.
bool loadPerKeyFile(const std::string& path, std::vector<KeyColor>& out) {
    std::ifstream file;
    std::istream* in = &std::cin;
    if(path != "-") {
        file.open(path);
        if(!file) {
            std::fprintf(stderr, "error: cannot open %s\n", path.c_str());
            return false;
        }
        in = &file;
    }
    std::string line;
    int lineno = 0;
    while(std::getline(*in, line)) {
        ++lineno;
        const std::size_t s = line.find_first_not_of(" \t\r\n");
        if(s == std::string::npos || line[s] == '#') {
            continue;  // blank or comment
        }
        std::string l = line.substr(s);
        for(char& c : l) {
            if(c == '=') {
                c = ' ';
            }
        }
        std::istringstream is(l);
        std::string name;
        is >> name;
        std::string rest;
        std::getline(is, rest);  // remainder is the colour spec

        std::uint8_t idx, r, g, b;
        if(!findKeyIdx(name, idx)) {
            std::fprintf(stderr, "%s:%d: unknown key label: %s\n", path.c_str(), lineno, name.c_str());
            return false;
        }
        if(!parseColorSpec(rest, r, g, b)) {
            std::fprintf(stderr, "%s:%d: bad colour: %s\n", path.c_str(), lineno, rest.c_str());
            return false;
        }
        out.push_back({idx, r, g, b});
    }
    if(out.empty()) {
        std::fprintf(stderr, "error: no key entries read from %s\n", path.c_str());
        return false;
    }
    return true;
}


void usageAw410k() {
    std::printf(
        "Alienware AW410K:\n"
        "  krgb-cli info\n"
        "  krgb-cli solid R G B          whole keyboard, direct\n"
        "  krgb-cli static R G B         whole keyboard, hardware static\n"
        "  krgb-cli off\n"
        "  krgb-cli spectrum             hardware rainbow spectrum\n"
        "  krgb-cli breathing R G B\n"
        "  krgb-cli wave R G B           single-colour wave\n"
        "  krgb-cli rainbow              per-key static rainbow\n"
        "  krgb-cli key LABEL R G B      light one key (others off)\n"
        "  krgb-cli perkey K=R,G,B ...   set listed keys (others off)\n"
        "  krgb-cli perkey-file FILE     read 'KEY R G B' lines (- = stdin)\n");
}

void usageCase() {
    std::printf(
        "Alienware AW-ELC case controller:\n"
        "  krgb-cli case info\n"
        "  krgb-cli case solid R G B\n"
        "  krgb-cli case zone N R G B\n"
        "  krgb-cli case rainbow\n"
        "  krgb-cli case off\n"
        "  krgb-cli case reset\n");
}

void usageLogitech() {
    std::printf(
        "Logitech HID++ 2.0:\n"
        "  krgb-cli logitech info        inspect HID++ features and device capabilities\n"
        "  krgb-cli logitech perkey-info dump read-only 0x8080 key types/IDs/colors\n"
        "  krgb-cli logitech layouts     list physical geometries for the connected model\n"
        "  krgb-cli logitech verify-keys GEOMETRY verify physical geometry IDs\n"
        "  krgb-cli logitech verify-controls verify media, controls, status LEDs and logo\n"
        "  krgb-cli logitech key GEOMETRY LABEL R G B set one physical keyboard LED\n"
        "  krgb-cli logitech perkey-solid GEOMETRY R G B set all physical lighting items\n"
        "  krgb-cli logitech solid R G B set all reported lighting elements/zones\n"
        "  krgb-cli logitech zone N R G B set one 0x8070 zone (N starts at 1)\n"
        "  krgb-cli logitech indicator NAME R G B\n"
        "    NAME: backlight|game|caps|scroll|num\n");
}

void usageLightMountGeneral() {
    std::printf(
        "Light Mount General firmware effects:\n"
        "  krgb-cli lightmount general static R G B BRIGHTNESS\n"
        "  krgb-cli lightmount general wave single DIR BRIGHTNESS SPEED R G B\n"
        "  krgb-cli lightmount general wave dual DIR BRIGHTNESS SPEED R1 G1 B1 R2 G2 B2\n"
        "  krgb-cli lightmount general wave gradient DIR BRIGHTNESS SPEED R,G,B@POS ...\n"
        "  krgb-cli lightmount general tornado clockwise|counter-clockwise BRIGHTNESS SPEED\n"
        "  krgb-cli lightmount general breathing BRIGHTNESS SPEED\n"
        "  krgb-cli lightmount general reactive BRIGHTNESS SPEED R1 G1 B1 R2 G2 B2\n"
        "  krgb-cli lightmount general matrix DIR BRIGHTNESS SPEED\n"
        "  brightness/speed: 10..100; DIR: up|down|left|right\n"
        "  gradient: 2..7 ordered stops, endpoints at 0 and 100\n");
}

void usageLightMount() {
    std::printf(
        "be quiet! Light Mount:\n"
        "  krgb-cli lightmount info\n"
        "  krgb-cli lightmount mode off|general|custom\n"
        "\n"
        "LampArray (standard HID):\n"
        "  krgb-cli lightmount lamparray autonomous on|off\n"
        "  krgb-cli lightmount lamparray solid R G B\n"
        "  krgb-cli lightmount lamparray lamp ID R G B\n"
        "  krgb-cli lightmount lamparray range START END R G B\n"
        "\n"
        "Custom vendor RGB:\n"
        "  krgb-cli lightmount custom solid R G B\n"
        "  krgb-cli lightmount custom key LABEL R G B\n"
        "  krgb-cli lightmount custom led ID R G B\n"
        "\n");
    usageLightMountGeneral();
    std::printf(
        "\nDiagnostics:\n"
        "  krgb-cli lightmount diagnostic accent-test\n"
        "  krgb-cli lightmount diagnostic padding-test LABEL\n"
        "  krgb-cli lightmount diagnostic keys-red\n"
        "  krgb-cli lightmount diagnostic vendor-scan START END DELAY_MS\n");
}

void usage() {
    std::printf(
        "krgb-cli — RGB device control\n"
        "\n");
    usageAw410k();
    std::printf("\n");
    usageCase();
    std::printf("\n");
    usageLightMount();
    std::printf("\n");
    usageLogitech();
}

// Handle the `case ...` subcommands against the AlienFX chassis controller.
// Returns the process exit code.
int runCase(const std::vector<std::string>& a) {
    const std::string sub = a.size() > 1 ? a[1] : std::string();

    if(sub == "info") {
        AlienFXDevice dev;
        std::string err;
        if(!dev.open(&err)) {
            std::printf("case controller : NOT FOUND\n");
            return 1;
        }
        std::printf("case controller : %s\n", dev.path().c_str());
        std::printf("firmware        : %s\n", dev.firmware().c_str());
        std::printf("zones           : %d\n", dev.zoneCount());
        return 0;
    }

    AlienFXDevice dev;
    std::string err;
    if(!dev.open(&err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    auto U = [&](std::size_t i) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::strtol(a[i].c_str(), nullptr, 0));
    };

    bool ok = true;
    if(sub == "solid" && a.size() >= 5) {
        ok = dev.setSolid(U(2), U(3), U(4));
    } else if(sub == "zone" && a.size() >= 6) {
        const int z = static_cast<int>(std::strtol(a[2].c_str(), nullptr, 0));
        std::vector<AlienFXDevice::ZoneColor> colors(dev.zoneCount(), AlienFXDevice::ZoneColor{0, 0, 0});
        if(z < 0 || z >= dev.zoneCount()) {
            std::fprintf(stderr, "zone out of range (0..%d)\n", dev.zoneCount() - 1);
            return 2;
        }
        colors[z] = {U(3), U(4), U(5)};
        ok = dev.setZoneColors(colors);
    } else if(sub == "rainbow") {
        const int n = dev.zoneCount();
        std::vector<AlienFXDevice::ZoneColor> colors(n);
        for(int z = 0; z < n; ++z) {
            std::uint8_t r, g, b;
            hsv(n ? static_cast<double>(z) / n : 0.0, 1.0, 1.0, r, g, b);
            colors[z] = {r, g, b};
        }
        ok = dev.setZoneColors(colors);
    } else if(sub == "off") {
        ok = dev.setOff();
    } else if(sub == "reset") {
        ok = dev.reset();
    } else {
        usageCase();
        return 2;
    }
    if(!ok) {
        std::fprintf(stderr, "error: case write failed (zones=%d)\n", dev.zoneCount());
        return 1;
    }
    return 0;
}


int runLogitech(const std::vector<std::string>& a) {
    const std::string sub = a.size() > 1 ? a[1] : std::string();
    if(sub != "info" && sub != "perkey-info" && sub != "layouts" &&
       sub != "verify-keys" && sub != "verify-controls" &&
       sub != "key" && sub != "perkey-solid" &&
       sub != "solid" && sub != "zone" && sub != "indicator") {
        usageLogitech();
        return 2;
    }

    LogitechHIDPP20Device dev;
    std::string err;
    if(!dev.openKeyboard(&err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    const std::string keyboardName = dev.displayName();
    std::uint8_t geometryModelMask = 0;
    if(const auto* known = logitechKnownDeviceForProductId(dev.productId())) {
        switch(known->model) {
            case LogitechKnownModel::G610Orion:
                geometryModelMask = logitech::g610_g810::kModelG610;
                break;
            case LogitechKnownModel::G810OrionSpectrum:
                geometryModelMask = logitech::g610_g810::kModelG810;
                break;
            default:
                break;
        }
    }

    auto findDeviceGeometry = [&](const std::string& name) {
        if(geometryModelMask == 0) {
            std::fprintf(stderr,
                         "error: %s has no model-specific LGS geometry in k-rgb; "
                         "use 'logitech perkey-info' for its device-reported topology\n",
                         keyboardName.c_str());
            return static_cast<const logitech::g610_g810::KeyboardGeometry*>(nullptr);
        }
        const auto* geometry = logitech::g610_g810::findGeometry(name);
        if(!geometry) {
            std::fprintf(stderr,
                         "error: unknown %s geometry '%s'; use 'logitech layouts'\n",
                         keyboardName.c_str(), name.c_str());
            return static_cast<const logitech::g610_g810::KeyboardGeometry*>(nullptr);
        }
        if(!logitech::g610_g810::geometrySupportsModel(*geometry,
                                                       geometryModelMask)) {
            std::fprintf(stderr,
                         "error: geometry %s is not present in the LGS resources for %s\n",
                         geometry->name, keyboardName.c_str());
            return static_cast<const logitech::g610_g810::KeyboardGeometry*>(nullptr);
        }
        return geometry;
    };

    if(sub == "layouts") {
        if(a.size() != 2) {
            usageLogitech();
            return 2;
        }
        if(geometryModelMask == 0) {
            std::printf("%s: no model-specific LGS geometry; topology is device-reported.\n",
                        keyboardName.c_str());
            return 0;
        }
        std::printf("%s physical geometries from LGS resources:\n",
                    keyboardName.c_str());
        for(const auto& geometry : logitech::g610_g810::kGeometries) {
            if(!logitech::g610_g810::geometrySupportsModel(
                   geometry, geometryModelMask)) {
                continue;
            }
            std::printf("  %-8s %3zu keys  %s\n",
                        geometry.name, geometry.keyCount, geometry.sources);
        }
        return 0;
    }

    if(sub == "perkey-info") {
        if(a.size() != 2) {
            usageLogitech();
            return 2;
        }

        LogitechHIDPP20PerKeyInfo info;
        if(!dev.getPerKey8080Info(info, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        std::printf("0x8080 typeFlags   : 0x%04x\n",
                    static_cast<unsigned>(info.typeFlags));
        std::printf("0x8080 keyTypes    : %u\n",
                    static_cast<unsigned>(info.keyTypeCount));
        std::printf("0x8080 maxKeyCount : %u\n",
                    static_cast<unsigned>(info.maxKeyCount));

        for(const auto& type : info.types) {
            const char* name = "unknown";
            switch(type.keyType) {
                case 0x0001: name = "keyboard"; break;
                case 0x0002: name = "consumer/media"; break;
                case 0x0004: name = "G-keys"; break;
                case 0x0008: name = "buttons"; break;
                case 0x0010: name = "logo"; break;
                case 0x0040: name = "indicators"; break;
            }

            std::printf("keyType 0x%04x %-14s reported=%u discovered=%zu\n",
                        static_cast<unsigned>(type.keyType), name,
                        static_cast<unsigned>(type.keyCount),
                        type.colors.size());
            for(const auto& color : type.colors) {
                std::printf("  id 0x%02x  RGB %3u %3u %3u\n",
                            static_cast<unsigned>(color.keyId),
                            static_cast<unsigned>(color.r),
                            static_cast<unsigned>(color.g),
                            static_cast<unsigned>(color.b));
            }
        }
        return 0;
    }

    if(sub == "verify-keys") {
        if(a.size() != 3) {
            usageLogitech();
            return 2;
        }

        const auto* selectedGeometry = findDeviceGeometry(a[2]);
        if(!selectedGeometry) {
            return 2;
        }

        LogitechHIDPP20PerKeyInfo info;
        if(!dev.getPerKey8080Info(info, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        constexpr std::uint16_t kKeyboardKeyType = 0x0001;
        const LogitechHIDPP20PerKeyTypeInfo* keyboard = nullptr;
        for(const auto& type : info.types) {
            if(type.keyType == kKeyboardKeyType) {
                keyboard = &type;
                break;
            }
        }
        if(!keyboard || keyboard->colors.empty()) {
            std::fprintf(stderr,
                         "error: %s reported no 0x8080 keyboard key IDs\n", keyboardName.c_str());
            return 1;
        }

        // Start from a known visual state.  Use batches because one 0x12
        // SetKeyColors report can carry at most 14 key tuples.
        for(std::size_t start = 0; start < keyboard->colors.size(); start += 14) {
            std::vector<LogitechHIDPP20KeyColor> off;
            const std::size_t end =
                std::min<std::size_t>(start + 14, keyboard->colors.size());
            off.reserve(end - start);
            for(std::size_t i = start; i < end; ++i) {
                off.push_back({keyboard->colors[i].keyId, 0, 0, 0});
            }
            if(!dev.setPerKey8080Colors(kKeyboardKeyType, off, &err)) {
                std::fprintf(stderr, "error: cannot clear keyboard LEDs: %s\n",
                             err.c_str());
                return 1;
            }
        }

        std::vector<std::uint8_t> verifyIds;
        verifyIds.reserve(selectedGeometry->keyCount);
        for(std::size_t i = 0; i < selectedGeometry->keyCount; ++i) {
            const std::uint8_t expectedId = selectedGeometry->keyIds[i];
            bool reported = false;
            for(const auto& color : keyboard->colors) {
                if(color.keyId == expectedId) {
                    reported = true;
                    break;
                }
            }
            if(!reported) {
                const auto* mapped =
                    logitech::g610_g810::keyboardDefinitionById(expectedId);
                std::fprintf(stderr,
                             "error: layout %s key 0x%02x (%s) not reported by device\n",
                             selectedGeometry->name,
                             static_cast<unsigned>(expectedId),
                             mapped ? mapped->label : "unknown");
                return 1;
            }
            verifyIds.push_back(expectedId);
        }

        std::printf(
            "%s %s key verifier: %zu firmware addresses, %zu physical keys "
            "(device reports %u).\n"
            "Physical key set comes from Logitech LGS resources %s.\n"
            "Each key is lit at full intensity. Enter=next, r=repeat, q=quit.\n\n",
            keyboardName.c_str(), selectedGeometry->name,
            keyboard->colors.size(), verifyIds.size(),
            static_cast<unsigned>(keyboard->keyCount),
            selectedGeometry->sources);

        std::uint8_t activeId = 0;
        bool haveActive = false;
        std::string input;

        for(std::size_t i = 0; i < verifyIds.size(); ++i) {
            const std::uint8_t keyId = verifyIds[i];

            if(haveActive) {
                if(!dev.setPerKey8080Color(kKeyboardKeyType, activeId,
                                           0, 0, 0, &err)) {
                    std::fprintf(stderr, "error: cannot clear id 0x%02x: %s\n",
                                 static_cast<unsigned>(activeId), err.c_str());
                    return 1;
                }
            }

            if(!dev.setPerKey8080Color(kKeyboardKeyType, keyId,
                                       255, 0, 0, &err)) {
                std::fprintf(stderr, "error: cannot light id 0x%02x: %s\n",
                             static_cast<unsigned>(keyId), err.c_str());
                return 1;
            }
            activeId = keyId;
            haveActive = true;

            for(;;) {
                const auto* mapped =
                    logitech::g610_g810::findKeyboardById(*selectedGeometry, keyId);
                const char* name = mapped ? mapped->label : "UNKNOWN";
                std::printf("[%3zu/%3zu] id 0x%02x -> %-30s  > ",
                            i + 1, verifyIds.size(),
                            static_cast<unsigned>(keyId), name);
                std::fflush(stdout);

                if(!std::getline(std::cin, input)) {
                    input = "q";
                }
                if(input.empty()) {
                    break;
                }
                if(input == "q" || input == "Q") {
                    if(haveActive) {
                        dev.setPerKey8080Color(kKeyboardKeyType, activeId,
                                               0, 0, 0, nullptr);
                    }
                    std::printf("Stopped at id 0x%02x.\n",
                                static_cast<unsigned>(keyId));
                    return 0;
                }
                if(input == "r" || input == "R") {
                    if(!dev.setPerKey8080Color(kKeyboardKeyType, keyId,
                                               255, 0, 0, &err)) {
                        std::fprintf(stderr,
                                     "error: cannot relight id 0x%02x: %s\n",
                                     static_cast<unsigned>(keyId), err.c_str());
                        return 1;
                    }
                    continue;
                }
                std::printf("Use Enter, r, or q.\n");
            }
        }

        if(haveActive) {
            if(!dev.setPerKey8080Color(kKeyboardKeyType, activeId,
                                       0, 0, 0, &err)) {
                std::fprintf(stderr, "warning: cannot clear final key: %s\n",
                             err.c_str());
            }
        }

        std::printf("Verification sequence complete.\n");
        return 0;
    }

    if(sub == "verify-controls") {
        if(a.size() != 2) {
            usageLogitech();
            return 2;
        }

        LogitechHIDPP20PerKeyInfo info;
        if(!dev.getPerKey8080Info(info, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        const LogitechHIDPP20PerKeyTypeInfo* media = nullptr;
        const LogitechHIDPP20PerKeyTypeInfo* indicators = nullptr;
        const LogitechHIDPP20PerKeyTypeInfo* logo = nullptr;
        for(const auto& type : info.types) {
            if(type.keyType == 0x0002) {
                media = &type;
            } else if(type.keyType == 0x0040) {
                indicators = &type;
            } else if(type.keyType == 0x0010) {
                logo = &type;
            }
        }

        if(!media || media->colors.empty()) {
            std::fprintf(stderr, "error: %s reported no media/control IDs\n", keyboardName.c_str());
            return 1;
        }
        if(!indicators || indicators->colors.empty()) {
            std::fprintf(stderr, "error: %s reported no indicator IDs\n", keyboardName.c_str());
            return 1;
        }
        if(!logo || logo->colors.empty()) {
            std::fprintf(stderr, "error: %s reported no logo IDs\n", keyboardName.c_str());
            return 1;
        }

        std::printf(
            "%s control verifier\n"
            "  media/control keyType 0x0002: %zu discovered (reported %u)\n"
            "  indicator keyType     0x0040: %zu discovered (reported %u)\n"
            "  logo keyType          0x0010: %zu discovered (reported %u)\n"
            "Tests media, lighting/game controls, lock-status LEDs and logo.\n"
            "All tested groups are cleared first; exactly one item is then lit at full intensity.\n"
            "Enter=next, r=repeat, q=quit.\n\n",
            keyboardName.c_str(),
            media->colors.size(), static_cast<unsigned>(media->keyCount),
            indicators->colors.size(), static_cast<unsigned>(indicators->keyCount),
            logo->colors.size(), static_cast<unsigned>(logo->keyCount));

        std::string input;

        // Put both groups into a known all-black state before verification.
        // This avoids pre-existing firmware colours (for example orange)
        // obscuring which control is currently under test.
        std::vector<LogitechHIDPP20KeyColor> mediaOff;
        mediaOff.reserve(media->colors.size());
        for(const auto& color : media->colors) {
            mediaOff.push_back({color.keyId, 0, 0, 0});
        }
        if(!dev.setPerKey8080Colors(0x0002, mediaOff, &err)) {
            std::fprintf(stderr, "error: cannot clear media controls: %s\n",
                         err.c_str());
            return 1;
        }

        std::vector<LogitechHIDPP20KeyColor> indicatorsOff;
        indicatorsOff.reserve(indicators->colors.size());
        for(const auto& color : indicators->colors) {
            indicatorsOff.push_back({color.keyId, 0, 0, 0});
        }
        if(!dev.setPerKey8080Colors(0x0040, indicatorsOff, &err)) {
            std::fprintf(stderr, "error: cannot clear indicator controls: %s\n",
                         err.c_str());
            return 1;
        }

        std::vector<LogitechHIDPP20KeyColor> logoOff;
        logoOff.reserve(logo->colors.size());
        for(const auto& color : logo->colors) {
            logoOff.push_back({color.keyId, 0, 0, 0});
        }
        if(!dev.setPerKey8080Colors(0x0010, logoOff, &err)) {
            std::fprintf(stderr, "error: cannot clear logo: %s\n",
                         err.c_str());
            return 1;
        }

        // The five illuminated media buttons are protocol keyType 0x0002.
        // Send the complete group for every step so all non-selected buttons
        // remain black.
        for(std::size_t i = 0; i < media->colors.size(); ++i) {
            const std::uint8_t keyId = media->colors[i].keyId;
            std::vector<LogitechHIDPP20KeyColor> frame;
            frame.reserve(media->colors.size());
            for(const auto& color : media->colors) {
                frame.push_back({color.keyId,
                                 static_cast<std::uint8_t>(color.keyId == keyId ? 255 : 0),
                                 0, 0});
            }
            if(!dev.setPerKey8080Colors(0x0002, frame, &err)) {
                std::fprintf(stderr, "error: cannot light media id 0x%02x: %s\n",
                             static_cast<unsigned>(keyId), err.c_str());
                return 1;
            }

            for(;;) {
                const auto* mapped = logitech::g610_g810::findById(logitech::g610_g810::kMedia, keyId);
                const char* name = mapped ? mapped->label : "UNKNOWN media ID";
                std::printf("[media %zu/%zu] id 0x%02x -> %-28s > ",
                            i + 1, media->colors.size(),
                            static_cast<unsigned>(keyId), name);
                std::fflush(stdout);
                if(!std::getline(std::cin, input)) {
                    input = "q";
                }
                if(input.empty()) {
                    break;
                }
                if(input == "q" || input == "Q") {
                    dev.setPerKey8080Colors(0x0002, mediaOff, nullptr);
                    dev.setPerKey8080Colors(0x0040, indicatorsOff, nullptr);
                    dev.setPerKey8080Colors(0x0010, logoOff, nullptr);
                    return 0;
                }
                if(input == "r" || input == "R") {
                    if(!dev.setPerKey8080Colors(0x0002, frame, &err)) {
                        std::fprintf(stderr, "error: cannot relight media id 0x%02x: %s\n",
                                     static_cast<unsigned>(keyId), err.c_str());
                        return 1;
                    }
                    continue;
                }
                std::printf("Use Enter, r, or q.\n");
            }

            if(!dev.setPerKey8080Colors(0x0002, mediaOff, &err)) {
                std::fprintf(stderr, "error: cannot clear media controls: %s\n",
                             err.c_str());
                return 1;
            }
        }

        // Verify every 0x0040 item. The G610/G810 resources define:
        // 0x01 Lighting, 0x02 Game, 0x03 Caps, 0x04 Scroll, 0x05 Num.
        for(std::size_t i = 0; i < indicators->colors.size(); ++i) {
            const std::uint8_t keyId = indicators->colors[i].keyId;

            std::vector<LogitechHIDPP20KeyColor> frame;
            frame.reserve(indicators->colors.size());
            for(const auto& color : indicators->colors) {
                frame.push_back({color.keyId,
                                 static_cast<std::uint8_t>(color.keyId == keyId ? 255 : 0),
                                 0, 0});
            }
            if(!dev.setPerKey8080Colors(0x0040, frame, &err)) {
                std::fprintf(stderr, "error: cannot set indicator id 0x%02x: %s\n",
                             static_cast<unsigned>(keyId), err.c_str());
                return 1;
            }

            for(;;) {
                const auto* mapped = logitech::g610_g810::findById(logitech::g610_g810::kIndicators, keyId);
                const char* name = mapped ? mapped->label : "UNKNOWN indicator ID";
                std::printf("[indicator %zu/%zu] id 0x%02x -> %-28s > ",
                            i + 1, indicators->colors.size(),
                            static_cast<unsigned>(keyId), name);
                std::fflush(stdout);
                if(!std::getline(std::cin, input)) {
                    input = "q";
                }
                if(input.empty()) {
                    break;
                }
                if(input == "q" || input == "Q") {
                    dev.setPerKey8080Colors(0x0002, mediaOff, nullptr);
                    dev.setPerKey8080Colors(0x0040, indicatorsOff, nullptr);
                    dev.setPerKey8080Colors(0x0010, logoOff, nullptr);
                    return 0;
                }
                if(input == "r" || input == "R") {
                    if(!dev.setPerKey8080Colors(0x0040, frame, &err)) {
                        std::fprintf(stderr, "error: cannot reset indicator id 0x%02x: %s\n",
                                     static_cast<unsigned>(keyId), err.c_str());
                        return 1;
                    }
                    continue;
                }
                std::printf("Use Enter, r, or q.\n");
            }

            if(!dev.setPerKey8080Colors(0x0040, indicatorsOff, &err)) {
                std::fprintf(stderr, "error: cannot clear indicator controls: %s\n",
                             err.c_str());
                return 1;
            }
        }

        // The G610/G810 LGS resources define exactly one 0x0010 item: logo id 0x01.
        // GetKeyColors can expose additional address slots (for example 0x02)
        // that are part of a broader Logitech model superset but are not
        // physically present on these models.
        constexpr std::uint8_t kLogoId = 0x01;
        bool logoPresent = false;
        for(const auto& color : logo->colors) {
            if(color.keyId == kLogoId) {
                logoPresent = true;
                break;
            }
        }
        if(!logoPresent) {
            std::fprintf(stderr, "error: %s logo id 0x01 was not reported\n", keyboardName.c_str());
            return 1;
        }

        std::vector<LogitechHIDPP20KeyColor> logoFrame;
        logoFrame.reserve(logo->colors.size());
        for(const auto& color : logo->colors) {
            logoFrame.push_back({
                color.keyId,
                static_cast<std::uint8_t>(color.keyId == kLogoId ? 255 : 0),
                0, 0
            });
        }
        if(!dev.setPerKey8080Colors(0x0010, logoFrame, &err)) {
            std::fprintf(stderr, "error: cannot set logo id 0x01: %s\n",
                         err.c_str());
            return 1;
        }

        for(;;) {
            std::printf("[logo 1/1] id 0x01 -> Logo                         > ");
            std::fflush(stdout);
            if(!std::getline(std::cin, input)) {
                input = "q";
            }
            if(input.empty()) {
                break;
            }
            if(input == "q" || input == "Q") {
                dev.setPerKey8080Colors(0x0002, mediaOff, nullptr);
                dev.setPerKey8080Colors(0x0040, indicatorsOff, nullptr);
                dev.setPerKey8080Colors(0x0010, logoOff, nullptr);
                return 0;
            }
            if(input == "r" || input == "R") {
                if(!dev.setPerKey8080Colors(0x0010, logoFrame, &err)) {
                    std::fprintf(stderr, "error: cannot reset logo id 0x01: %s\n",
                                 err.c_str());
                    return 1;
                }
                continue;
            }
            std::printf("Use Enter, r, or q.\n");
        }

        if(!dev.setPerKey8080Colors(0x0040, indicatorsOff, &err)) {
            std::fprintf(stderr, "warning: cannot clear indicator controls: %s\n",
                         err.c_str());
        }
        if(!dev.setPerKey8080Colors(0x0010, logoOff, &err)) {
            std::fprintf(stderr, "warning: cannot clear logo: %s\n",
                         err.c_str());
        }
        std::printf("Control verification sequence complete.\n");
        return 0;
    }

    if(sub == "key") {
        if(a.size() != 7) {
            usageLogitech();
            return 2;
        }

        const auto* geometry = findDeviceGeometry(a[2]);
        if(!geometry) {
            return 2;
        }

        const std::size_t labelArg = 3;
        const std::size_t rgbArg = 4;
        const auto* key =
            logitech::g610_g810::findKeyboardByName(*geometry, a[labelArg]);
        if(!key) {
            std::fprintf(stderr,
                         "error: key %s is not present in %s geometry %s\n",
                         a[labelArg].c_str(), keyboardName.c_str(), geometry->name);
            return 2;
        }

        auto parseChannel = [](const std::string& value, std::uint8_t& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < 0 || parsed > 255) {
                return false;
            }
            out = static_cast<std::uint8_t>(parsed);
            return true;
        };

        std::uint8_t r = 0, g = 0, b = 0;
        if(!parseChannel(a[rgbArg], r) ||
           !parseChannel(a[rgbArg + 1], g) ||
           !parseChannel(a[rgbArg + 2], b)) {
            std::fprintf(stderr, "error: RGB values must be within 0..255\n");
            return 2;
        }

        if(!dev.setPerKey8080Color(key->keyType, key->keyId, r, g, b, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        std::printf("Logitech %s %s key %s (0x%02x) -> %u,%u,%u\n",
                    keyboardName.c_str(), geometry->name, key->name,
                    static_cast<unsigned>(key->keyId),
                    static_cast<unsigned>(r),
                    static_cast<unsigned>(g),
                    static_cast<unsigned>(b));
        return 0;
    }

    if(sub == "perkey-solid") {
        if(a.size() != 6) {
            usageLogitech();
            return 2;
        }

        const auto* geometry = findDeviceGeometry(a[2]);
        if(!geometry) {
            return 2;
        }

        const std::size_t rgbArg = 3;
        auto parseChannel = [](const std::string& value, std::uint8_t& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < 0 || parsed > 255) {
                return false;
            }
            out = static_cast<std::uint8_t>(parsed);
            return true;
        };

        std::uint8_t r = 0, g = 0, b = 0;
        if(!parseChannel(a[rgbArg], r) ||
           !parseChannel(a[rgbArg + 1], g) ||
           !parseChannel(a[rgbArg + 2], b)) {
            std::fprintf(stderr, "error: RGB values must be within 0..255\n");
            return 2;
        }

        std::vector<LogitechHIDPP20KeyColor> keyboardColors;
        keyboardColors.reserve(geometry->keyCount);
        for(std::size_t i = 0; i < geometry->keyCount; ++i) {
            keyboardColors.push_back({geometry->keyIds[i], r, g, b});
        }
        if(!dev.setPerKey8080Colors(logitech::g610_g810::kKeyboardKeyType,
                                    keyboardColors, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        auto setGroup = [&](std::uint16_t type, const auto& elements) {
            std::vector<LogitechHIDPP20KeyColor> colors;
            colors.reserve(elements.size());
            for(const auto& element : elements) {
                colors.push_back({element.keyId, r, g, b});
            }
            return dev.setPerKey8080Colors(type, colors, &err);
        };

        if(!setGroup(logitech::g610_g810::kMediaKeyType, logitech::g610_g810::kMedia) ||
           !setGroup(logitech::g610_g810::kIndicatorKeyType, logitech::g610_g810::kIndicators) ||
           !setGroup(logitech::g610_g810::kLogoKeyType, logitech::g610_g810::kLogo)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        std::printf("Logitech %s %s physical lighting (%zu items) -> %u,%u,%u\n",
                    keyboardName.c_str(), geometry->name,
                    logitech::g610_g810::physicalLightingCount(*geometry),
                    static_cast<unsigned>(r),
                    static_cast<unsigned>(g),
                    static_cast<unsigned>(b));
        return 0;
    }

    if(sub == "indicator") {
        if(a.size() != 6) {
            usageLogitech();
            return 2;
        }

        std::uint8_t keyId = 0;
        if(a[2] == "backlight") {
            keyId = 0x01;
        } else if(a[2] == "game") {
            keyId = 0x02;
        } else if(a[2] == "caps") {
            keyId = 0x03;
        } else if(a[2] == "scroll") {
            keyId = 0x04;
        } else if(a[2] == "num") {
            keyId = 0x05;
        } else {
            std::fprintf(stderr, "error: unknown indicator: %s\n", a[2].c_str());
            return 2;
        }

        auto parseChannel = [](const std::string& value, std::uint8_t& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < 0 || parsed > 255) {
                return false;
            }
            out = static_cast<std::uint8_t>(parsed);
            return true;
        };

        std::uint8_t r = 0, g = 0, b = 0;
        if(!parseChannel(a[3], r) ||
           !parseChannel(a[4], g) ||
           !parseChannel(a[5], b)) {
            std::fprintf(stderr, "error: RGB values must be within 0..255\n");
            return 2;
        }

        constexpr std::uint16_t kIndicatorKeyType = 0x0040;
        if(!dev.setPerKey8080Color(kIndicatorKeyType, keyId, r, g, b, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        std::printf("Logitech %s indicator %s -> %u,%u,%u\n",
                    keyboardName.c_str(), a[2].c_str(),
                    static_cast<unsigned>(r),
                    static_cast<unsigned>(g),
                    static_cast<unsigned>(b));
        return 0;
    }

    if(sub == "zone") {
        if(a.size() != 6) {
            usageLogitech();
            return 2;
        }

        char* end = nullptr;
        errno = 0;
        const long zoneNumber = std::strtol(a[2].c_str(), &end, 0);
        if(end == a[2].c_str() || *end != '\0' || errno != 0 ||
           zoneNumber < 1 || zoneNumber > 255) {
            std::fprintf(stderr, "error: zone must be within 1..255\n");
            return 2;
        }

        auto parseChannel = [](const std::string& value, std::uint8_t& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < 0 || parsed > 255) {
                return false;
            }
            out = static_cast<std::uint8_t>(parsed);
            return true;
        };

        std::uint8_t r = 0, g = 0, b = 0;
        if(!parseChannel(a[3], r) ||
           !parseChannel(a[4], g) ||
           !parseChannel(a[5], b)) {
            std::fprintf(stderr, "error: RGB values must be within 0..255\n");
            return 2;
        }

        std::uint8_t zoneCount = 0;
        if(!dev.getColorLed8070ZoneCount(zoneCount, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(zoneNumber > zoneCount) {
            std::fprintf(stderr, "error: device reports %u zones\n",
                         static_cast<unsigned>(zoneCount));
            return 2;
        }

        const std::vector<LogitechHIDPP20ZoneColor> colors{{
            static_cast<std::uint8_t>(zoneNumber - 1), r, g, b
        }};
        if(!dev.setColorLed8070Zones(colors, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        std::printf("Logitech %s zone %ld/%u -> %u,%u,%u\n",
                    keyboardName.c_str(), zoneNumber,
                    static_cast<unsigned>(zoneCount),
                    static_cast<unsigned>(r),
                    static_cast<unsigned>(g),
                    static_cast<unsigned>(b));
        return 0;
    }

    if(sub == "solid") {
        if(a.size() != 5) {
            usageLogitech();
            return 2;
        }

        auto parseChannel = [](const std::string& value, std::uint8_t& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < 0 || parsed > 255) {
                return false;
            }
            out = static_cast<std::uint8_t>(parsed);
            return true;
        };

        std::uint8_t r = 0, g = 0, b = 0;
        if(!parseChannel(a[2], r) ||
           !parseChannel(a[3], g) ||
           !parseChannel(a[4], b)) {
            std::fprintf(stderr, "error: RGB values must be within 0..255\n");
            return 2;
        }

        if(!dev.setSolid(r, g, b, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        std::printf("Logitech %s solid -> %u,%u,%u\n",
                    keyboardName.c_str(),
                    static_cast<unsigned>(r),
                    static_cast<unsigned>(g),
                    static_cast<unsigned>(b));
        return 0;
    }

    const auto& usb = dev.usbIdentity();
    std::printf("model    : %s\n", keyboardName.c_str());
    std::printf("device   : %s\n", dev.path().c_str());
    std::printf("usb      : %04x:%04x  interface %d\n",
                static_cast<unsigned>(usb.vendorId),
                static_cast<unsigned>(usb.productId),
                usb.interfaceNumber);
    if(!usb.manufacturer.empty()) {
        std::printf("vendor   : %s\n", usb.manufacturer.c_str());
    }
    if(!usb.serial.empty()) {
        std::printf("serial   : %s\n", usb.serial.c_str());
    }
    const char* colorCapability = "unknown (not reported by HID++)";
    switch(dev.colorCapability()) {
        case LogitechLightingColorCapability::Monochrome:
            colorCapability = "monochrome (known hardware property)";
            break;
        case LogitechLightingColorCapability::Rgb:
            colorCapability = "RGB (known hardware property)";
            break;
        case LogitechLightingColorCapability::Unknown:
            break;
    }
    std::printf("lighting : %s\n", colorCapability);
    if(const auto* known = logitechKnownDeviceForProductId(dev.productId())) {
        std::printf("evidence : %s\n",
                    logitechProtocolEvidenceName(known->protocolEvidence));
        if(known->evidenceNote && known->evidenceNote[0] != '\0') {
            std::printf("source   : %s\n", known->evidenceNote);
        }
    }

    LogitechHIDPP20Capabilities capabilities;
    if(!dev.getCapabilities(capabilities, &err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    std::printf("HID++ protocol : %u\n",
                static_cast<unsigned>(capabilities.protocol.protocolNumber));
    if(capabilities.protocol.hasTargetSoftwareHint()) {
        std::printf("target software: 0x%02x\n",
                    static_cast<unsigned>(capabilities.protocol.targetSoftware));
    }
    std::printf("features       : %zu (ROOT + complete Feature Set enumeration)\n",
                capabilities.features.size());

    for(const auto& feature : capabilities.features) {
        const auto* catalog =
            logitechHIDPP20FeatureCatalogEntry(feature.featureId);
        const std::string_view name = catalog
            ? catalog->name : std::string_view{"Unknown"};
        const std::string_view domain = catalog
            ? logitechHIDPP20FeatureDomainName(catalog->domain)
            : std::string_view{"unknown"};
        std::string flagText;
        if(feature.isObsolete()) flagText += " obsolete";
        if(feature.isHidden()) flagText += " hidden";
        if(feature.isEngineering()) flagText += " engineering";
        if(feature.isManufacturingDeactivatable()) flagText += " manufacturing";
        if(feature.isComplianceDeactivatable()) flagText += " compliance";
        if(flagText.empty()) flagText = " normal";

        if(feature.versionKnown) {
            std::printf("  index 0x%02x  id 0x%04x  type 0x%02x  version %-3u"
                        "  %-10.*s %.*s [%s]\n",
                        static_cast<unsigned>(feature.index),
                        static_cast<unsigned>(feature.featureId),
                        static_cast<unsigned>(feature.type),
                        static_cast<unsigned>(feature.version),
                        static_cast<int>(domain.size()), domain.data(),
                        static_cast<int>(name.size()), name.data(),
                        flagText.c_str() + 1);
        } else {
            std::printf("  index 0x%02x  id 0x%04x  type 0x%02x  version   -"
                        "  %-10.*s %.*s [%s]\n",
                        static_cast<unsigned>(feature.index),
                        static_cast<unsigned>(feature.featureId),
                        static_cast<unsigned>(feature.type),
                        static_cast<int>(domain.size()), domain.data(),
                        static_cast<int>(name.size()), name.data(),
                        flagText.c_str() + 1);
        }
    }

    std::vector<LogitechHIDPP20FirmwareInfo> fw;
    if(dev.getFirmwareInfo(fw, &err)) {
        for(const auto& item : fw) {
            const char* kind = "other";
            if(item.kind == 0x00) kind = "firmware";
            else if(item.kind == 0x01) kind = "bootloader";
            else if(item.kind == 0x02) kind = "hardware";

            if(item.kind == 0x00 || item.kind == 0x01) {
                std::printf("%-9s: entity %u  %s %02X.%02X",
                            kind,
                            static_cast<unsigned>(item.entity),
                            item.name.empty() ? "" : item.name.c_str(),
                            static_cast<unsigned>(item.major),
                            static_cast<unsigned>(item.minor));
                if(item.build) {
                    std::printf(".B%04X", static_cast<unsigned>(item.build));
                }
                std::printf("\n");
            } else if(item.kind == 0x02) {
                std::printf("%-9s: entity %u  revision %u\n",
                            kind,
                            static_cast<unsigned>(item.entity),
                            static_cast<unsigned>(item.major));
            } else {
                std::printf("%-9s: entity %u  type 0x%02x\n",
                            kind,
                            static_cast<unsigned>(item.entity),
                            static_cast<unsigned>(item.kind));
            }
        }
    } else {
        std::printf("firmware : unavailable (%s)\n", err.c_str());
        err.clear();
    }

    std::printf("capabilities (read-only queries):\n");

    if(capabilities.findFeature(
           LogitechHIDPP20Device::kFeatureDeviceTypeAndName) != nullptr) {
        LogitechHIDPP20DeviceTypeInfo info;
        if(dev.getDeviceTypeAndName(info, &err)) {
            const char* type = info.deviceType == 0 ? "keyboard" : "other";
            std::printf("  0x0005 device type/name   : type=%u (%s), name=%s\n",
                        static_cast<unsigned>(info.deviceType), type,
                        info.name.empty() ? "-" : info.name.c_str());
        } else {
            std::printf("  0x0005 device type/name   : query failed (%s)\n", err.c_str());
            err.clear();
        }
    }

    if(capabilities.findFeature(
           LogitechHIDPP20Device::kFeatureKeyboardInternationalLayouts) != nullptr) {
        LogitechHIDPP20KeyboardLayoutInfo info;
        if(dev.getKeyboardLayout(info, &err)) {
            const char* country = hidKeyboardCountryName(info.countryCode);
            if(hidKeyboardCountryCodeIsReserved(info.countryCode)) {
                std::printf("  0x4540 keyboard layout    : country=%s (0x%02x)\n",
                            country, static_cast<unsigned>(info.countryCode));
            } else {
                std::printf("  0x4540 keyboard layout    : country=%s\n", country);
            }
        } else {
            std::printf("  0x4540 keyboard layout    : query failed (%s)\n", err.c_str());
            err.clear();
        }
    } else if(capabilities.findFeature(
                  LogitechHIDPP20Device::kFeatureKeyboardLayout) != nullptr) {
        std::printf("  0x4520 keyboard layout    : present; legacy wire format not guessed\n");
    }

    if(capabilities.findFeature(
           LogitechHIDPP20Device::kFeatureBrightnessControl) != nullptr) {
        LogitechHIDPP20BrightnessInfo info;
        if(dev.getBrightnessInfo(info, &err)) {
            std::printf("  0x8040 brightness         : min=%u max=%u steps=%u caps=0x%02x",
                        static_cast<unsigned>(info.minimum),
                        static_cast<unsigned>(info.maximum),
                        static_cast<unsigned>(info.steps),
                        static_cast<unsigned>(info.capabilities));
            if(info.hasCurrent) {
                std::printf(" current=%u", static_cast<unsigned>(info.current));
            }
            std::printf("\n");
        } else {
            std::printf("  0x8040 brightness         : present; %s\n", err.c_str());
            err.clear();
        }
    }

    if(capabilities.findFeature(LogitechHIDPP20Device::kFeatureDisableKeys) != nullptr ||
       capabilities.findFeature(LogitechHIDPP20Device::kFeatureDisableKeysByUsage) != nullptr) {
        LogitechHIDPP20DisableKeysInfo info;
        if(dev.getDisableKeysInfo(info, &err)) {
            std::printf("  keyboard disable          : fixed-cap=0x%02x",
                        static_cast<unsigned>(info.disableableMask));
            if(info.hasDisabledMask) {
                std::printf(" active=0x%02x",
                            static_cast<unsigned>(info.disabledMask));
            }
            if(info.maxDisabledUsages != 0) {
                std::printf(" usage-capacity=%u",
                            static_cast<unsigned>(info.maxDisabledUsages));
            }
            std::printf("\n");
        } else {
            std::printf("  keyboard disable          : query failed (%s)\n", err.c_str());
            err.clear();
        }
    }

    bool hasReprog = false;
    for(std::uint16_t id = 0x1b00; id <= 0x1b04; ++id) {
        if(capabilities.findFeature(id) != nullptr) {
            hasReprog = true;
            break;
        }
    }
    if(hasReprog) {
        LogitechHIDPP20ControlsInfo info;
        if(dev.getReprogrammableControls(info, &err)) {
            std::printf("  0x%04x controls           : %zu controls\n",
                        static_cast<unsigned>(info.featureId), info.controls.size());
            for(std::size_t i = 0; i < info.controls.size(); ++i) {
                const auto& control = info.controls[i];
                std::printf("    [%zu] cid=0x%04x task=0x%04x flags=0x%04x"
                            " pos=%u group=%u mask=0x%02x\n",
                            i,
                            static_cast<unsigned>(control.controlId),
                            static_cast<unsigned>(control.taskId),
                            static_cast<unsigned>(control.flags),
                            static_cast<unsigned>(control.position),
                            static_cast<unsigned>(control.group),
                            static_cast<unsigned>(control.groupMask));
            }
        } else {
            std::printf("  reprogrammable controls   : query failed (%s)\n", err.c_str());
            err.clear();
        }
    }

    if(capabilities.findFeature(
           LogitechHIDPP20Device::kFeatureAdjustableReportRate) != nullptr ||
       capabilities.findFeature(
           LogitechHIDPP20Device::kFeatureExtendedAdjustableReportRate) != nullptr) {
        LogitechHIDPP20ReportRateInfo info;
        if(dev.getReportRateInfo(info, &err)) {
            std::printf("  report rate               : supported-mask=0x%04x",
                        static_cast<unsigned>(info.supportedMask));
            if(info.hasCurrent) {
                std::printf(" current=%u ms",
                            static_cast<unsigned>(info.current));
            }
            std::printf("\n");
        } else {
            std::printf("  report rate               : query failed (%s)\n", err.c_str());
            err.clear();
        }
    }

    if(capabilities.findFeature(
           LogitechHIDPP20Device::kFeatureColorLedEffects) != nullptr) {
        LogitechHIDPP20ColorLedInfo info;
        if(dev.getColorLed8070Info(info, &err)) {
            std::printf("  0x8070 ColorLedEffects    : zones=%u nv=0x%04x ext=0x%04x\n",
                        static_cast<unsigned>(info.zoneCount),
                        static_cast<unsigned>(info.nvCapabilities),
                        static_cast<unsigned>(info.extCapabilities));
            for(const auto& zone : info.zones) {
                std::printf("    zone %u location=0x%04x effects=%u persist=0x%02x\n",
                            static_cast<unsigned>(zone.zoneIndex),
                            static_cast<unsigned>(zone.location),
                            static_cast<unsigned>(zone.effectCount),
                            static_cast<unsigned>(zone.persistencyCapabilities));
                for(const auto& effect : zone.effects) {
                    std::printf("      effect %u id=0x%04x caps=0x%04x period=%u ms\n",
                                static_cast<unsigned>(effect.effectIndex),
                                static_cast<unsigned>(effect.effectId),
                                static_cast<unsigned>(effect.capabilities),
                                static_cast<unsigned>(effect.periodMs));
                }
            }
        } else {
            std::printf("  0x8070 ColorLedEffects    : query failed (%s)\n", err.c_str());
            err.clear();
        }
    }

    if(capabilities.findFeature(
           LogitechHIDPP20Device::kFeatureRgbEffects) != nullptr) {
        LogitechHIDPP20RgbEffectsInfo info;
        if(dev.getRgbEffects8071Info(info, &err)) {
            std::printf("  0x8071 RgbEffects         : clusters=%u nv=0x%04x ext=0x%04x multi=%u\n",
                        static_cast<unsigned>(info.clusterCount),
                        static_cast<unsigned>(info.nvCapabilities),
                        static_cast<unsigned>(info.extCapabilities),
                        static_cast<unsigned>(info.multiClusterEffectCount));
            for(const auto& cluster : info.clusters) {
                std::printf("    cluster %u location=0x%04x effects=%u"
                            " display-persist=0x%02x effect-persist=%s multi-led=%s\n",
                            static_cast<unsigned>(cluster.clusterIndex),
                            static_cast<unsigned>(cluster.location),
                            static_cast<unsigned>(cluster.effectCount),
                            static_cast<unsigned>(cluster.displayPersistencyCapabilities),
                            cluster.effectPersistency ? "yes" : "no",
                            cluster.multiLedPattern ? "yes" : "no");
                for(const auto& effect : cluster.effects) {
                    std::printf("      effect %u id=0x%04x caps=0x%04x period=%u ms\n",
                                static_cast<unsigned>(effect.effectIndex),
                                static_cast<unsigned>(effect.effectId),
                                static_cast<unsigned>(effect.capabilities),
                                static_cast<unsigned>(effect.periodMs));
                }
            }
        } else {
            std::printf("  0x8071 RgbEffects         : query failed (%s)\n", err.c_str());
            err.clear();
        }
    }

    if(capabilities.findFeature(
           LogitechHIDPP20Device::kFeaturePerKeyLighting) != nullptr) {
        LogitechHIDPP20PerKeyInfo info;
        if(dev.getPerKey8080Info(info, &err)) {
            std::size_t addresses = 0;
            for(const auto& type : info.types) addresses += type.colors.size();
            std::printf("  0x8080 PerKeyLighting     : types=0x%04x keyTypes=%u"
                        " max=%u addresses=%zu\n",
                        static_cast<unsigned>(info.typeFlags),
                        static_cast<unsigned>(info.keyTypeCount),
                        static_cast<unsigned>(info.maxKeyCount),
                        addresses);
        } else {
            std::printf("  0x8080 PerKeyLighting     : query failed (%s)\n", err.c_str());
            err.clear();
        }
    }

    if(capabilities.findFeature(
           LogitechHIDPP20Device::kFeaturePerKeyLighting2) != nullptr) {
        LogitechHIDPP20PerKey8081Info info;
        if(dev.getPerKey8081Info(info, &err)) {
            std::printf("  0x8081 PerKeyLighting2    : %zu populated RGB zones\n",
                        info.zoneIds.size());
            std::printf("    zones:");
            for(const auto id : info.zoneIds) {
                std::printf(" %u", static_cast<unsigned>(id));
            }
            std::printf("\n");
        } else {
            std::printf("  0x8081 PerKeyLighting2    : query failed (%s)\n", err.c_str());
            err.clear();
        }
    }

    if(capabilities.findFeature(
           LogitechHIDPP20Device::kFeatureModeStatus) != nullptr) {
        LogitechHIDPP20ModeStatusInfo info;
        if(dev.getModeStatusInfo(info, &err)) {
            std::printf("  0x8090 ModeStatus         : status=%02x/%02x caps=0x%04x\n",
                        static_cast<unsigned>(info.status0),
                        static_cast<unsigned>(info.status1),
                        static_cast<unsigned>(info.capabilities));
        } else {
            std::printf("  0x8090 ModeStatus         : present; %s\n", err.c_str());
            err.clear();
        }
    }

    for(const auto featureId : {
            LogitechHIDPP20Device::kFeatureGamingGKeys,
            LogitechHIDPP20Device::kFeatureGamingMKeys,
            LogitechHIDPP20Device::kFeatureMacroRecord,
            LogitechHIDPP20Device::kFeatureOnboardProfiles}) {
        if(capabilities.findFeature(featureId) != nullptr) {
            const auto name = logitechHIDPP20FeatureName(featureId);
            std::printf("  0x%04x %-21.*s: present (capability from Feature Set;"
                        " detailed read parser pending)\n",
                        static_cast<unsigned>(featureId),
                        static_cast<int>(name.size()), name.data());
        }
    }

    return 0;
}


int runLightMount(const std::vector<std::string>& a) {
    // Preferred hierarchical command syntax. Translate to the existing
    // validated command handlers so legacy development aliases keep working.
    if(a.size() > 1 && a[1] == "general") {
        if(a.size() < 3) {
            usageLightMountGeneral();
            return 2;
        }

        std::vector<std::string> legacy{"lightmount"};
        if(a[2] == "static") {
            legacy.push_back("general-static");
            legacy.insert(legacy.end(), a.begin() + 3, a.end());
        } else if(a[2] == "wave") {
            if(a.size() < 4) {
                usageLightMountGeneral();
                return 2;
            }
            if(a[3] == "single") {
                legacy.push_back("general-wave");
            } else if(a[3] == "dual") {
                legacy.push_back("general-wave-dual");
            } else if(a[3] == "gradient") {
                legacy.push_back("general-wave-gradient");
            } else {
                usageLightMountGeneral();
                return 2;
            }
            legacy.insert(legacy.end(), a.begin() + 4, a.end());
        } else if(a[2] == "tornado") {
            legacy.push_back("general-tornado");
            legacy.insert(legacy.end(), a.begin() + 3, a.end());
        } else if(a[2] == "breathing") {
            legacy.push_back("general-breathing");
            legacy.insert(legacy.end(), a.begin() + 3, a.end());
        } else if(a[2] == "reactive") {
            legacy.push_back("general-reactive");
            legacy.insert(legacy.end(), a.begin() + 3, a.end());
        } else if(a[2] == "matrix") {
            legacy.push_back("general-matrix");
            legacy.insert(legacy.end(), a.begin() + 3, a.end());
        } else {
            usageLightMountGeneral();
            return 2;
        }
        return runLightMount(legacy);
    }

    if(a.size() > 1 && a[1] == "lamparray") {
        if(a.size() < 3) {
            usageLightMount();
            return 2;
        }
        std::vector<std::string> legacy{"lightmount"};
        if(a[2] == "autonomous") {
            legacy.push_back("autonomous");
        } else if(a[2] == "solid") {
            legacy.push_back("lamp-solid");
        } else if(a[2] == "lamp") {
            legacy.push_back("lamp");
        } else if(a[2] == "range") {
            legacy.push_back("lamp-range");
        } else {
            usageLightMount();
            return 2;
        }
        legacy.insert(legacy.end(), a.begin() + 3, a.end());
        return runLightMount(legacy);
    }

    if(a.size() > 1 && a[1] == "custom") {
        if(a.size() < 3) {
            usageLightMount();
            return 2;
        }
        std::vector<std::string> legacy{"lightmount"};
        if(a[2] == "solid") {
            legacy.push_back("solid");
        } else if(a[2] == "key") {
            legacy.push_back("key");
        } else if(a[2] == "led") {
            legacy.push_back("vendor-led");
        } else {
            usageLightMount();
            return 2;
        }
        legacy.insert(legacy.end(), a.begin() + 3, a.end());
        return runLightMount(legacy);
    }

    if(a.size() > 1 && a[1] == "diagnostic") {
        if(a.size() < 3) {
            usageLightMount();
            return 2;
        }
        std::vector<std::string> legacy{"lightmount"};
        if(a[2] == "accent-test") {
            legacy.push_back("accent-test");
        } else if(a[2] == "padding-test") {
            legacy.push_back("padding-test");
        } else if(a[2] == "keys-red") {
            legacy.push_back("keys-red");
        } else if(a[2] == "vendor-scan") {
            legacy.push_back("vendor-scan");
        } else {
            usageLightMount();
            return 2;
        }
        legacy.insert(legacy.end(), a.begin() + 3, a.end());
        return runLightMount(legacy);
    }

    if(a.size() > 1 && a[1] == "mode") {
        if(a.size() != 3) {
            usageLightMount();
            return 2;
        }

        LightMountLightingMode mode;
        if(a[2] == "off") {
            mode = LightMountLightingMode::Off;
        } else if(a[2] == "general") {
            mode = LightMountLightingMode::General;
        } else if(a[2] == "custom") {
            mode = LightMountLightingMode::Custom;
        } else {
            usageLightMount();
            return 2;
        }

        HIDLampArrayDevice lamp;
        std::string err;
        if(!lamp.open(LightMountDevice::kVendorId,
                      LightMountDevice::kProductId,
                      3, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!lamp.setAutonomousMode(true)) {
            std::fprintf(stderr, "error: failed to enable LampArray autonomous mode\n");
            return 1;
        }

        LightMountDevice dev;
        if(!dev.open(&err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!dev.setLightingMode(mode)) {
            std::fprintf(stderr, "error: failed to set Light Mount lighting mode\n");
            return 1;
        }
        return 0;
    }

    const std::string sub = a.size() > 1 ? a[1] : std::string();

    if(sub == "info") {
        const std::string p = LightMountDevice::findDevicePath();
        std::printf("Light Mount vendor HID : %s\n", p.empty() ? "NOT FOUND" : p.c_str());
        return p.empty() ? 1 : 0;
    }

    if(sub == "autonomous") {
        if(a.size() != 3 || (a[2] != "on" && a[2] != "off")) {
            usage();
            return 2;
        }

        HIDLampArrayDevice lamp;
        std::string err;
        if(!lamp.open(LightMountDevice::kVendorId,
                      LightMountDevice::kProductId,
                      3, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        const bool enabled = a[2] == "on";
        if(!lamp.setAutonomousMode(enabled)) {
            std::fprintf(stderr, "error: failed to set LampArray autonomous mode\n");
            return 1;
        }

        std::printf("LampArray autonomous mode: %s\n", enabled ? "on" : "off");
        return 0;
    }

    if(sub == "general-static" ||
       sub == "general-wave" ||
       sub == "general-wave-dual" ||
       sub == "general-wave-gradient" ||
       sub == "general-tornado" ||
       sub == "general-breathing" ||
       sub == "general-reactive" ||
       sub == "general-matrix") {
        auto parseNumber = [](const std::string& value, long min, long max, long& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < min || parsed > max) {
                return false;
            }
            out = parsed;
            return true;
        };

        auto parseFourWayDirection = [](const std::string& value,
                                        LightMountDirection& direction) {
            if(strcasecmp(value.c_str(), "up") == 0) {
                direction = LightMountDirection::Up;
            } else if(strcasecmp(value.c_str(), "down") == 0) {
                direction = LightMountDirection::Down;
            } else if(strcasecmp(value.c_str(), "left") == 0) {
                direction = LightMountDirection::Left;
            } else if(strcasecmp(value.c_str(), "right") == 0) {
                direction = LightMountDirection::Right;
            } else {
                return false;
            }
            return true;
        };

        LightMountGeneralEffect effect;
        long rr = 0, gg = 0, bb = 0, r2 = 0, g2 = 0, b2 = 0;
        long brightness = 100, speed = 50;

        if(sub == "general-static") {
            if(a.size() != 6 ||
               !parseNumber(a[2], 0, 255, rr) ||
               !parseNumber(a[3], 0, 255, gg) ||
               !parseNumber(a[4], 0, 255, bb) ||
               !parseNumber(a[5], 10, 100, brightness)) {
                std::fprintf(stderr,
                             "error: usage: lightmount general-static R G B BRIGHTNESS "
                             "(brightness 10..100)\n");
                return 2;
            }
            effect = lightmount::makeStaticEffect(
                {static_cast<std::uint8_t>(rr),
                 static_cast<std::uint8_t>(gg),
                 static_cast<std::uint8_t>(bb)},
                static_cast<std::uint8_t>(brightness));
        } else if(sub == "general-wave" || sub == "general-wave-dual") {
            const bool dual = sub == "general-wave-dual";
            const std::size_t expected = dual ? 11 : 8;
            if(a.size() != expected ||
               !parseFourWayDirection(a[2], effect.direction) ||
               !parseNumber(a[3], 10, 100, brightness) ||
               !parseNumber(a[4], 10, 100, speed) ||
               !parseNumber(a[5], 0, 255, rr) ||
               !parseNumber(a[6], 0, 255, gg) ||
               !parseNumber(a[7], 0, 255, bb) ||
               (dual && (!parseNumber(a[8], 0, 255, r2) ||
                         !parseNumber(a[9], 0, 255, g2) ||
                         !parseNumber(a[10], 0, 255, b2)))) {
                std::fprintf(stderr, "error: invalid Color Wave arguments\n");
                return 2;
            }
            const LightMountColor color1{
                static_cast<std::uint8_t>(rr),
                static_cast<std::uint8_t>(gg),
                static_cast<std::uint8_t>(bb)};
            if(dual) {
                effect = lightmount::makeColorWaveDualEffect(
                    effect.direction,
                    static_cast<std::uint8_t>(brightness),
                    static_cast<std::uint8_t>(speed),
                    color1,
                    {static_cast<std::uint8_t>(r2),
                     static_cast<std::uint8_t>(g2),
                     static_cast<std::uint8_t>(b2)});
            } else {
                effect = lightmount::makeColorWaveSingleEffect(
                    effect.direction,
                    static_cast<std::uint8_t>(brightness),
                    static_cast<std::uint8_t>(speed),
                    color1);
            }
        } else if(sub == "general-wave-gradient") {
            if(a.size() < 7 || a.size() > 12 ||
               !parseFourWayDirection(a[2], effect.direction) ||
               !parseNumber(a[3], 10, 100, brightness) ||
               !parseNumber(a[4], 10, 100, speed)) {
                std::fprintf(stderr,
                             "error: usage: lightmount general-wave-gradient DIR BRIGHTNESS SPEED "
                             "R,G,B@POS ... (2..7 stops)\n");
                return 2;
            }
            std::vector<LightMountGradientStop> gradient;
            for(std::size_t i = 5; i < a.size(); ++i) {
                LightMountGradientStop stop{};
                if(!parseLightMountGradientStop(a[i], stop)) {
                    std::fprintf(stderr, "error: invalid gradient stop: %s\n", a[i].c_str());
                    return 2;
                }
                gradient.push_back(stop);
            }
            effect = lightmount::makeColorWaveGradientEffect(
                effect.direction,
                static_cast<std::uint8_t>(brightness),
                static_cast<std::uint8_t>(speed),
                std::move(gradient));
        } else if(sub == "general-tornado") {
            if(a.size() != 5 ||
               !parseNumber(a[3], 10, 100, brightness) ||
               !parseNumber(a[4], 10, 100, speed)) {
                std::fprintf(stderr,
                             "error: usage: lightmount general-tornado "
                             "clockwise|counter-clockwise BRIGHTNESS SPEED\n");
                return 2;
            }
            if(strcasecmp(a[2].c_str(), "clockwise") == 0) {
                effect.direction = LightMountDirection::Clockwise;
            } else if(strcasecmp(a[2].c_str(), "counter-clockwise") == 0) {
                effect.direction = LightMountDirection::CounterClockwise;
            } else {
                std::fprintf(stderr, "error: Tornado direction must be clockwise or counter-clockwise\n");
                return 2;
            }
            effect = lightmount::makeTornadoEffect(
                effect.direction,
                static_cast<std::uint8_t>(brightness),
                static_cast<std::uint8_t>(speed));
        } else if(sub == "general-breathing") {
            if(a.size() != 4 ||
               !parseNumber(a[2], 10, 100, brightness) ||
               !parseNumber(a[3], 10, 100, speed)) {
                std::fprintf(stderr,
                             "error: usage: lightmount general-breathing BRIGHTNESS SPEED\n");
                return 2;
            }
            effect = lightmount::makeBreathingEffect(
                static_cast<std::uint8_t>(brightness),
                static_cast<std::uint8_t>(speed));
        } else if(sub == "general-reactive") {
            if(a.size() != 10 ||
               !parseNumber(a[2], 10, 100, brightness) ||
               !parseNumber(a[3], 10, 100, speed) ||
               !parseNumber(a[4], 0, 255, rr) ||
               !parseNumber(a[5], 0, 255, gg) ||
               !parseNumber(a[6], 0, 255, bb) ||
               !parseNumber(a[7], 0, 255, r2) ||
               !parseNumber(a[8], 0, 255, g2) ||
               !parseNumber(a[9], 0, 255, b2)) {
                std::fprintf(stderr,
                             "error: usage: lightmount general-reactive "
                             "BRIGHTNESS SPEED R1 G1 B1 R2 G2 B2\n");
                return 2;
            }
            effect = lightmount::makeReactiveEffect(
                static_cast<std::uint8_t>(brightness),
                static_cast<std::uint8_t>(speed),
                {static_cast<std::uint8_t>(rr),
                 static_cast<std::uint8_t>(gg),
                 static_cast<std::uint8_t>(bb)},
                {static_cast<std::uint8_t>(r2),
                 static_cast<std::uint8_t>(g2),
                 static_cast<std::uint8_t>(b2)});
        } else {
            if(a.size() != 5 ||
               !parseFourWayDirection(a[2], effect.direction) ||
               !parseNumber(a[3], 10, 100, brightness) ||
               !parseNumber(a[4], 10, 100, speed)) {
                std::fprintf(stderr,
                             "error: usage: lightmount general-matrix DIR BRIGHTNESS SPEED\n");
                return 2;
            }
            effect = lightmount::makeMatrixEffect(
                effect.direction,
                static_cast<std::uint8_t>(brightness),
                static_cast<std::uint8_t>(speed));
        }

        HIDLampArrayDevice lamp;
        std::string err;
        if(!lamp.open(LightMountDevice::kVendorId,
                      LightMountDevice::kProductId,
                      3, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!lamp.setAutonomousMode(true)) {
            std::fprintf(stderr, "error: failed to enable LampArray autonomous mode\n");
            return 1;
        }

        LightMountDevice dev;
        if(!dev.open(&err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!dev.setLightingMode(LightMountLightingMode::General)) {
            std::fprintf(stderr, "error: failed to select Light Mount General mode\n");
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if(!dev.setGeneralEffect(effect)) {
            std::fprintf(stderr, "error: invalid or failed Light Mount General effect write\n");
            return 1;
        }

        std::printf("Light Mount General effect configured\n");
        return 0;
    }

    if(sub == "vendor-scan") {
        if(a.size() != 5) {
            usage();
            return 2;
        }

        auto parseNumber = [](const std::string& value, long min, long max, long& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < min || parsed > max) {
                return false;
            }
            out = parsed;
            return true;
        };

        long startValue, endValue, delayMs;
        if(!parseNumber(a[2], 0, 65535, startValue) ||
           !parseNumber(a[3], 0, 65535, endValue) ||
           !parseNumber(a[4], 50, 10000, delayMs) ||
           startValue > endValue) {
            std::fprintf(stderr, "error: invalid vendor scan range or delay (50..10000 ms)\n");
            return 2;
        }

        HIDLampArrayDevice lamp;
        std::string err;
        if(!lamp.open(LightMountDevice::kVendorId,
                      LightMountDevice::kProductId,
                      3, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!lamp.setAutonomousMode(true)) {
            std::fprintf(stderr, "error: failed to enable LampArray autonomous mode\n");
            return 1;
        }

        LightMountDevice dev;
        if(!dev.open(&err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!dev.setCustomMode()) {
            std::fprintf(stderr, "error: failed to select Light Mount Custom mode\n");
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        bool havePrevious = false;
        std::uint16_t previous = 0;

        for(long idValue = startValue; idValue <= endValue; ++idValue) {
            const std::uint16_t current = static_cast<std::uint16_t>(idValue);
            std::vector<LightMountLedColor> leds;
            leds.reserve(LightMountDevice::kLedsPerPacket);

            // White makes the active LED easy to identify. Turn the previous
            // scan LED off in the same packet so only one tested ID remains on.
            leds.push_back({current, 255, 255, 255});
            if(havePrevious && previous != current) {
                leds.push_back({previous, 0, 0, 0});
            }

            // Fill the validated five-record packet with known topbar IDs off.
            for(std::uint16_t filler = 0;
                filler <= 44 && leds.size() < LightMountDevice::kLedsPerPacket;
                ++filler) {
                if(filler != current && (!havePrevious || filler != previous)) {
                    leds.push_back({filler, 0, 0, 0});
                }
            }

            if(leds.size() != LightMountDevice::kLedsPerPacket ||
               !dev.setLeds(leds)) {
                std::fprintf(stderr, "error: vendor scan write failed at ID %u\n",
                             static_cast<unsigned>(current));
                return 1;
            }

            std::printf("vendor LED %u\n", static_cast<unsigned>(current));
            std::fflush(stdout);
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

            previous = current;
            havePrevious = true;
        }

        // Leave the final scanned LED off when the scan completes.
        if(havePrevious) {
            std::vector<LightMountLedColor> leds;
            leds.reserve(LightMountDevice::kLedsPerPacket);
            leds.push_back({previous, 0, 0, 0});
            for(std::uint16_t filler = 0;
                filler <= 44 && leds.size() < LightMountDevice::kLedsPerPacket;
                ++filler) {
                if(filler != previous) {
                    leds.push_back({filler, 0, 0, 0});
                }
            }
            if(leds.size() == LightMountDevice::kLedsPerPacket) {
                dev.setLeds(leds);
            }
        }

        return 0;
    }

    if(sub == "solid") {
        if(a.size() != 5) {
            usage();
            return 2;
        }

        auto parseChannel = [](const std::string& value, std::uint8_t& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < 0 || parsed > 255) {
                return false;
            }
            out = static_cast<std::uint8_t>(parsed);
            return true;
        };

        std::uint8_t r, g, b;
        if(!parseChannel(a[2], r) ||
           !parseChannel(a[3], g) ||
           !parseChannel(a[4], b)) {
            std::fprintf(stderr, "error: RGB values must be within 0..255\n");
            return 2;
        }

        HIDLampArrayDevice lamp;
        std::string err;
        if(!lamp.open(LightMountDevice::kVendorId,
                      LightMountDevice::kProductId,
                      3, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!lamp.setAutonomousMode(true)) {
            std::fprintf(stderr, "error: failed to enable LampArray autonomous mode\n");
            return 1;
        }

        LightMountDevice dev;
        if(!dev.open(&err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!dev.setSolid(r, g, b)) {
            std::fprintf(stderr, "error: Light Mount solid vendor write failed\n");
            return 1;
        }

        std::printf("Light Mount solid -> %u,%u,%u\n",
                    static_cast<unsigned>(r),
                    static_cast<unsigned>(g),
                    static_cast<unsigned>(b));
        return 0;
    }

    if(sub == "padding-test") {
        if(a.size() != 3) {
            usage();
            return 2;
        }

        std::uint16_t target = 0;
        if(!findLightMountKey(a[2], target)) {
            std::fprintf(stderr, "error: unknown Light Mount key label: %s\n", a[2].c_str());
            return 2;
        }

        HIDLampArrayDevice lamp;
        std::string err;
        if(!lamp.open(LightMountDevice::kVendorId,
                      LightMountDevice::kProductId,
                      3, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!lamp.setAutonomousMode(true)) {
            std::fprintf(stderr, "error: failed to enable LampArray autonomous mode\n");
            return 1;
        }

        LightMountDevice dev;
        if(!dev.open(&err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        // Establish one Custom-mode session and one full red frame.
        if(!dev.setSolid(255, 0, 0)) {
            std::fprintf(stderr, "error: failed to set red Light Mount frame\n");
            return 1;
        }

        // Do not call setCustomMode() again. This isolates setLeds() padding:
        // the requested key should become blue while every other RGB element
        // remains red.
        if(!dev.setLeds({{target, 0, 0, 255}})) {
            std::fprintf(stderr, "error: Light Mount padding test write failed\n");
            return 1;
        }

        std::printf("Light Mount padding test: %s (vendor LED %u) blue, all others red\n",
                    a[2].c_str(), static_cast<unsigned>(target));
        return 0;
    }

    if(sub == "key") {
        if(a.size() != 6) {
            usage();
            return 2;
        }

        std::uint16_t target = 0;
        if(!findLightMountKey(a[2], target)) {
            std::fprintf(stderr, "error: unknown Light Mount key label: %s\n", a[2].c_str());
            return 2;
        }

        auto parseChannel = [](const std::string& value, std::uint8_t& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < 0 || parsed > 255) {
                return false;
            }
            out = static_cast<std::uint8_t>(parsed);
            return true;
        };

        std::uint8_t r, g, b;
        if(!parseChannel(a[3], r) ||
           !parseChannel(a[4], g) ||
           !parseChannel(a[5], b)) {
            std::fprintf(stderr, "error: RGB values must be within 0..255\n");
            return 2;
        }

        // Vendor Custom and LampArray host-control are mutually exclusive.
        HIDLampArrayDevice lamp;
        std::string err;
        if(!lamp.open(LightMountDevice::kVendorId,
                      LightMountDevice::kProductId,
                      3, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!lamp.setAutonomousMode(true)) {
            std::fprintf(stderr, "error: failed to enable LampArray autonomous mode\n");
            return 1;
        }

        LightMountDevice dev;
        if(!dev.open(&err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!dev.setCustomMode()) {
            std::fprintf(stderr, "error: failed to select Light Mount Custom mode\n");
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Pass a single logical update. setLeds() pads the validated
        // five-record vendor packet by repeating this record, so this also
        // exercises the short-final-packet path on real hardware.
        if(!dev.setLeds({{target, r, g, b}})) {
            std::fprintf(stderr, "error: Light Mount key RGB write failed\n");
            return 1;
        }

        std::printf("Light Mount key %s (vendor LED %u) -> %u,%u,%u\n",
                    a[2].c_str(),
                    static_cast<unsigned>(target),
                    static_cast<unsigned>(r),
                    static_cast<unsigned>(g),
                    static_cast<unsigned>(b));
        return 0;
    }

    if(sub == "vendor-led") {
        if(a.size() != 6) {
            usage();
            return 2;
        }

        auto parseNumber = [](const std::string& value, long min, long max, long& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < min || parsed > max) {
                return false;
            }
            out = parsed;
            return true;
        };

        long idValue, rr, gg, bb;
        if(!parseNumber(a[2], 0, 65535, idValue) ||
           !parseNumber(a[3], 0, 255, rr) ||
           !parseNumber(a[4], 0, 255, gg) ||
           !parseNumber(a[5], 0, 255, bb)) {
            std::fprintf(stderr, "error: invalid vendor LED ID or RGB value\n");
            return 2;
        }

        // Vendor Custom and LampArray host-control are mutually exclusive on
        // the Light Mount. Return LampArray to autonomous mode before sending
        // vendor Custom packets.
        HIDLampArrayDevice lamp;
        std::string err;
        if(!lamp.open(LightMountDevice::kVendorId,
                      LightMountDevice::kProductId,
                      3, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!lamp.setAutonomousMode(true)) {
            std::fprintf(stderr, "error: failed to enable LampArray autonomous mode\n");
            return 1;
        }

        LightMountDevice dev;
        if(!dev.open(&err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        if(!dev.setCustomMode()) {
            std::fprintf(stderr, "error: failed to select Light Mount Custom mode\n");
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        const std::uint16_t target = static_cast<std::uint16_t>(idValue);
        std::vector<LightMountLedColor> leds;
        leds.reserve(LightMountDevice::kLedsPerPacket);
        leds.push_back({target,
                        static_cast<std::uint8_t>(rr),
                        static_cast<std::uint8_t>(gg),
                        static_cast<std::uint8_t>(bb)});

        // The validated short vendor packet contains exactly five records.
        // Fill the unused records with distinct, known topbar IDs set to black.
        for(std::uint16_t filler = 40;
            filler <= 44 && leds.size() < LightMountDevice::kLedsPerPacket;
            ++filler) {
            if(filler != target) {
                leds.push_back({filler, 0, 0, 0});
            }
        }

        if(leds.size() != LightMountDevice::kLedsPerPacket ||
           !dev.setLeds(leds)) {
            std::fprintf(stderr, "error: Light Mount vendor LED write failed\n");
            return 1;
        }

        std::printf("Light Mount vendor LED %u -> %ld,%ld,%ld\n",
                    static_cast<unsigned>(target), rr, gg, bb);
        return 0;
    }

    if(sub == "keys-red" || sub == "lamp-solid" || sub == "lamp" || sub == "lamp-range") {
        auto parseNumber = [](const std::string& value, long min, long max, long& out) {
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 0);
            if(end == value.c_str() || *end != '\0' || errno != 0 ||
               parsed < min || parsed > max) {
                return false;
            }
            out = parsed;
            return true;
        };

        std::uint16_t first = 0;
        std::uint16_t last = 0;
        std::uint8_t r = 255, g = 0, b = 0;

        if(sub == "lamp-solid") {
            if(a.size() != 5) {
                usageLightMount();
                return 2;
            }
            long rr, gg, bb;
            if(!parseNumber(a[2], 0, 255, rr) ||
               !parseNumber(a[3], 0, 255, gg) ||
               !parseNumber(a[4], 0, 255, bb)) {
                std::fprintf(stderr, "error: invalid RGB value\n");
                return 2;
            }
            r = static_cast<std::uint8_t>(rr);
            g = static_cast<std::uint8_t>(gg);
            b = static_cast<std::uint8_t>(bb);
        } else if(sub == "lamp") {
            if(a.size() != 6) {
                usage();
                return 2;
            }
            long id, rr, gg, bb;
            if(!parseNumber(a[2], 0, 65535, id) ||
               !parseNumber(a[3], 0, 255, rr) ||
               !parseNumber(a[4], 0, 255, gg) ||
               !parseNumber(a[5], 0, 255, bb)) {
                std::fprintf(stderr, "error: invalid lamp ID or RGB value\n");
                return 2;
            }
            first = last = static_cast<std::uint16_t>(id);
            r = static_cast<std::uint8_t>(rr);
            g = static_cast<std::uint8_t>(gg);
            b = static_cast<std::uint8_t>(bb);
        } else if(sub == "lamp-range") {
            if(a.size() != 7) {
                usage();
                return 2;
            }
            long start, end, rr, gg, bb;
            if(!parseNumber(a[2], 0, 65535, start) ||
               !parseNumber(a[3], 0, 65535, end) ||
               !parseNumber(a[4], 0, 255, rr) ||
               !parseNumber(a[5], 0, 255, gg) ||
               !parseNumber(a[6], 0, 255, bb)) {
                std::fprintf(stderr, "error: invalid lamp range or RGB value\n");
                return 2;
            }
            first = static_cast<std::uint16_t>(start);
            last = static_cast<std::uint16_t>(end);
            r = static_cast<std::uint8_t>(rr);
            g = static_cast<std::uint8_t>(gg);
            b = static_cast<std::uint8_t>(bb);
        }

        HIDLampArrayDevice lamp;
        std::string err;
        if(!lamp.open(LightMountDevice::kVendorId,
                      LightMountDevice::kProductId,
                      3, &err)) {
            std::fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }

        HIDLampArrayAttributes attrs;
        if(!lamp.getAttributes(attrs)) {
            std::fprintf(stderr, "error: failed to read LampArray attributes\n");
            return 1;
        }

        std::printf("Light Mount LampArray : %s\n", lamp.path().c_str());
        std::printf("lamps                 : %u\n", attrs.lampCount);

        if(sub == "keys-red" || sub == "lamp-solid") {
            const std::uint8_t solidR = sub == "keys-red" ? 255 : r;
            const std::uint8_t solidG = sub == "keys-red" ? 0 : g;
            const std::uint8_t solidB = sub == "keys-red" ? 0 : b;
            if(!lamp.setSolid(solidR, solidG, solidB, 255)) {
                std::fprintf(stderr, "error: LampArray write failed\n");
                return 1;
            }
            return 0;
        }

        if(first > last || last >= attrs.lampCount) {
            std::fprintf(stderr, "error: lamp range must be within 0..%u\n",
                         attrs.lampCount ? attrs.lampCount - 1 : 0);
            return 2;
        }

        if(!lamp.setAutonomousMode(false)) {
            std::fprintf(stderr, "error: failed to disable LampArray autonomous mode\n");
            return 1;
        }

        const bool ok = (sub == "lamp")
            ? lamp.setLamp(first, r, g, b, 255)
            : lamp.setRange(first, last, r, g, b, 255);
        if(!ok) {
            std::fprintf(stderr, "error: LampArray write failed\n");
            return 1;
        }
        return 0;
    }

    if(sub != "accent-test") {
        usageLightMount();
        return 2;
    }

    LightMountDevice dev;
    std::string err;
    if(!dev.open(&err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    // Physically validated mapping:
    // top bar = red, left strip = green, right strip = blue.
    if(!dev.setAccentSolid(255, 0, 0,
                           0, 255, 0,
                           0, 0, 255)) {
        std::fprintf(stderr, "error: Light Mount vendor write failed\n");
        return 1;
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> a(argv + 1, argv + argc);
    if(a.empty()) {
        usage();
        return 2;
    }
    const std::string cmd = a[0];

    if(cmd == "case") {
        return runCase(a);
    }

    if(cmd == "lightmount") {
        return runLightMount(a);
    }

    if(cmd == "logitech") {
        return runLogitech(a);
    }

    if(cmd == "info") {
        const KeyboardModel* model = nullptr;
        const std::string p = AW410KDevice::findDevice(&model);
        std::printf("device : %s\n", p.empty() ? "NOT FOUND" : p.c_str());
        std::printf("model  : %s\n", model ? model->name : "-");
        std::printf("keys   : %zu\n", model ? modelKeyCount(model->bit) : kKeyCount);
        return 0;
    }

    AW410KDevice dev;
    std::string err;
    if(!dev.open(&err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    auto U = [&](std::size_t i) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::strtol(a[i].c_str(), nullptr, 0));
    };

    bool ok = true;
    if(cmd == "solid" && a.size() >= 4) {
        ok = dev.setSolid(U(1), U(2), U(3));
    } else if(cmd == "static" && a.size() >= 4) {
        ok = dev.setStatic(U(1), U(2), U(3));
    } else if(cmd == "off") {
        ok = dev.setOff();
    } else if(cmd == "spectrum") {
        ok = dev.setEffect(Mode::Spectrum, Speed::Normal, Direction::Left, ColorMode::Rainbow, 0, 0, 0);
    } else if(cmd == "breathing" && a.size() >= 4) {
        ok = dev.setEffect(Mode::Breathing, Speed::Normal, Direction::Right, ColorMode::Single, U(1), U(2), U(3));
    } else if(cmd == "wave" && a.size() >= 4) {
        ok = dev.setEffect(Mode::SingleWave, Speed::Normal, Direction::Left, ColorMode::Single, U(1), U(2), U(3));
    } else if(cmd == "rainbow") {
        const std::uint8_t bit = dev.model() ? dev.model()->bit : kAllModels;
        const std::size_t total = modelKeyCount(bit);
        std::vector<KeyColor> keys;
        keys.reserve(total);
        std::size_t n = 0;
        for(std::size_t i = 0; i < kKeyCount; ++i) {
            if(!(kKeyMap[i].models & bit)) {
                continue;
            }
            std::uint8_t r, g, b;
            hsv(total ? static_cast<double>(n++) / total : 0.0, 1.0, 1.0, r, g, b);
            keys.push_back({kKeyMap[i].idx, r, g, b});
        }
        ok = dev.setPerKey(keys);
    } else if(cmd == "key" && a.size() >= 5) {
        std::uint8_t idx = 0;
        if(!findKeyIdx(a[1], idx)) {
            std::fprintf(stderr, "unknown key label: %s\n", a[1].c_str());
            return 2;
        }
        ok = dev.setPerKey({{idx, U(2), U(3), U(4)}});
    } else if(cmd == "perkey" && a.size() >= 2) {
        std::vector<KeyColor> keys;
        for(std::size_t i = 1; i < a.size(); ++i) {
            const std::size_t eq = a[i].find('=');
            if(eq == std::string::npos) {
                std::fprintf(stderr, "bad argument (expected KEY=R,G,B): %s\n", a[i].c_str());
                return 2;
            }
            const std::string name = a[i].substr(0, eq);
            std::uint8_t idx, r, g, b;
            if(!findKeyIdx(name, idx)) {
                std::fprintf(stderr, "unknown key label: %s\n", name.c_str());
                return 2;
            }
            if(!parseColorSpec(a[i].substr(eq + 1), r, g, b)) {
                std::fprintf(stderr, "bad colour for %s: %s\n", name.c_str(), a[i].substr(eq + 1).c_str());
                return 2;
            }
            keys.push_back({idx, r, g, b});
        }
        ok = dev.setPerKey(keys);
    } else if(cmd == "perkey-file" && a.size() >= 2) {
        std::vector<KeyColor> keys;
        if(!loadPerKeyFile(a[1], keys)) {
            return 2;  // loadPerKeyFile already printed the error
        }
        ok = dev.setPerKey(keys);
    } else {
        usage();
        return 2;
    }

    if(!ok) {
        std::fprintf(stderr, "error: device write failed\n");
        return 1;
    }
    return 0;
}
