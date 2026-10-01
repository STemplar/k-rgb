#pragma once

#include <cstdint>

namespace krgb {

// Logitech HID++ 0x4540 (Keyboard International Layouts) country/layout codes.
// These values are Logitech-private, from the official application's layout
// enumeration. They are NOT USB HID 1.11 bCountryCode values, even though some
// numeric values overlap.
//
// Returns nullptr for values not present in the known Logitech enumeration.
inline constexpr const char* logitechKeyboardLayoutName(std::uint8_t code) {
    switch(code) {
        case 0x01: return "US";
        case 0x02: return "International";
        case 0x03: return "UK";
        case 0x04: return "German";
        case 0x05: return "French";
        case 0x07: return "Russian";
        case 0x08: return "Nordic";
        case 0x09: return "Korean";
        case 0x0A: return "Japanese";
        case 0x0B: return "Chinese";
        case 0x0D: return "Swiss";
        case 0x0E: return "Turkish";
        case 0x0F: return "Spanish";
        case 0x10: return "Arabic";
        case 0x11: return "Belgian";
        case 0x14: return "Czech";
        case 0x16: return "Nordic";
        case 0x18: return "Hebrew";
        case 0x19: return "Hungarian";
        case 0x1A: return "Italian";
        case 0x1D: return "Nordic";
        case 0x1F: return "Portuguese";
        case 0x21: return "Nordic";
        case 0x24: return "Turkish";
        case 0x28: return "Bulgarian";
        case 0x33: return "Thai";
        case 0x37: return "International 2";
        case 0x38: return "Brazilian (ABNT2)";
        case 0x3A: return "Arabic";
        case 0x3E: return "Korean";
        case 0x41: return "Czech";
        default: return nullptr;
    }
}

} // namespace krgb
