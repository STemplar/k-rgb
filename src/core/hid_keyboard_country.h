#pragma once

#include <cstddef>
#include <cstdint>

namespace krgb {

// USB HID 1.11, section 6.2.1 HID Descriptor: keyboard bCountryCode values.
// Logitech HID++ feature 0x4540 reports this value as its first GetLayout
// response byte.
inline constexpr const char* kHidKeyboardCountryNames[] = {
    "Not Supported",       // 0x00
    "Arabic",              // 0x01
    "Belgian",             // 0x02
    "Canadian-Bilingual",  // 0x03
    "Canadian-French",     // 0x04
    "Czech Republic",      // 0x05
    "Danish",              // 0x06
    "Finnish",             // 0x07
    "French",              // 0x08
    "German",              // 0x09
    "Greek",               // 0x0a
    "Hebrew",              // 0x0b
    "Hungary",             // 0x0c
    "International (ISO)", // 0x0d
    "Italian",             // 0x0e
    "Japan (Katakana)",    // 0x0f
    "Korean",              // 0x10
    "Latin American",      // 0x11
    "Netherlands/Dutch",   // 0x12
    "Norwegian",           // 0x13
    "Persian (Farsi)",     // 0x14
    "Poland",              // 0x15
    "Portuguese",          // 0x16
    "Russia",              // 0x17
    "Slovakia",            // 0x18
    "Spanish",             // 0x19
    "Swedish",             // 0x1a
    "Swiss/French",        // 0x1b
    "Swiss/German",        // 0x1c
    "Switzerland",         // 0x1d
    "Taiwan",              // 0x1e
    "Turkish-Q",           // 0x1f
    "UK",                  // 0x20
    "US",                  // 0x21
    "Yugoslavia",          // 0x22
    "Turkish-F",           // 0x23
};

inline constexpr std::size_t kHidKeyboardCountryNameCount =
    sizeof(kHidKeyboardCountryNames) / sizeof(kHidKeyboardCountryNames[0]);

static_assert(kHidKeyboardCountryNameCount == 0x24,
              "USB HID keyboard country table must cover 0x00..0x23");

inline constexpr bool hidKeyboardCountryCodeIsReserved(std::uint8_t code) {
    return code >= kHidKeyboardCountryNameCount;
}

inline constexpr const char* hidKeyboardCountryName(std::uint8_t code) {
    return hidKeyboardCountryCodeIsReserved(code)
        ? "Reserved"
        : kHidKeyboardCountryNames[code];
}

} // namespace krgb
