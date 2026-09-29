// krgb-cli — exercises the AW410K core engine on real hardware.
// Mirrors tools/aw410k.py so the C++ port can be validated against it.
#include "core/aw410k_device.h"
#include "core/alienfx_device.h"
#include "core/keymap.h"
#include "core/hid_lamp_array_device.h"
#include "core/lightmount_device.h"
#include "core/lightmount_keymap.h"

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

void usage() {
    std::printf(
        "krgb-cli — Alienware AW410K RGB control\n"
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
        "                                colour is R,G,B or #RRGGBB\n"
        "  krgb-cli perkey-file FILE     read 'KEY R G B' lines (- = stdin)\n"
        "  --- case / chassis LEDs (Alienware AW-ELC controller) ---\n"
        "  krgb-cli case info\n"
        "  krgb-cli case solid R G B     all case zones\n"
        "  krgb-cli case zone N R G B    light zone N only (others off)\n"
        "  krgb-cli case rainbow         per-zone rainbow (reveals zone layout)\n"
        "  krgb-cli case off\n"
        "  krgb-cli case reset\n"
        "  --- be quiet! Light Mount ---\n"
        "  krgb-cli lightmount info\n"
        "  krgb-cli lightmount accent-test   top red, left green, right blue\n"
        "  krgb-cli lightmount keys-red      whole LampArray interface red\n"
        "  krgb-cli lightmount lamp ID R G B\n"
        "  krgb-cli lightmount lamp-range START END R G B\n"
        "  krgb-cli lightmount autonomous on|off\n"
        "  krgb-cli lightmount key LABEL R G B\n"
        "  krgb-cli lightmount vendor-led ID R G B\n"
        "  krgb-cli lightmount vendor-scan START END DELAY_MS\n");
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
        usage();
        return 2;
    }
    if(!ok) {
        std::fprintf(stderr, "error: case write failed (zones=%d)\n", dev.zoneCount());
        return 1;
    }
    return 0;
}


int runLightMount(const std::vector<std::string>& a) {
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

        std::vector<LightMountLedColor> leds;
        leds.reserve(LightMountDevice::kLedsPerPacket);
        leds.push_back({target, r, g, b});

        // The validated short vendor packet contains exactly five records.
        // Use known topbar LEDs as harmless black filler records.
        for(std::uint16_t filler = 40;
            filler <= 44 && leds.size() < LightMountDevice::kLedsPerPacket;
            ++filler) {
            leds.push_back({filler, 0, 0, 0});
        }

        if(!dev.setLeds(leds)) {
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

    if(sub == "keys-red" || sub == "lamp" || sub == "lamp-range") {
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

        if(sub == "lamp") {
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

        if(sub == "keys-red") {
            if(!lamp.setSolid(255, 0, 0, 255)) {
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
        usage();
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
