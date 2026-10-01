// Logitech G610/G810 shared physical lighting definitions plus G PRO
// (046d:c339) TKL geometries.
//
// Sources: Logitech Gaming Software G610/G810 SVG/XML resources and their
// PerKeyLightingDefaults.txt files. G PRO TKL sets are inferred from the
// G610/G810 ANSI/ISO keyboard address sets by removing the 17-key numpad.
// G PRO protocol compatibility is supported by independent third-party reverse
// engineering, but remains hardware-unverified in k-rgb. Regional printed
// legends are not represented here because lighting control only needs
// physical key sets.
//
// HID++ 0x8080 can expose a broader Logitech address superset than a physical
// keyboard contains. The geometry tables below select the physically present
// addresses. The ISO105 set, media keys, indicators/controls and logo were
// physically verified on a PID C331 G810. G610 mappings are resource-derived
// and remain hardware-unverified in k-rgb.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace krgb::logitech::g610_g810 {

inline constexpr std::uint16_t kKeyboardKeyType = 0x0001;
inline constexpr std::uint16_t kMediaKeyType = 0x0002;
inline constexpr std::uint16_t kLogoKeyType = 0x0010;
inline constexpr std::uint16_t kIndicatorKeyType = 0x0040;

struct LightingElement {
    std::uint16_t keyType;
    std::uint8_t keyId;
    const char* name;   // Stable CLI/internal name.
    const char* label;  // Human-readable physical label.
};

// Stable names/labels for the 105 addresses in the verified ISO105 set.
inline constexpr std::array<LightingElement, 105> kBaseKeyboardDefinitions = {{
    { kKeyboardKeyType, 0x29, "ESC", "Esc" },
    { kKeyboardKeyType, 0x3a, "F1", "F1" },
    { kKeyboardKeyType, 0x3b, "F2", "F2" },
    { kKeyboardKeyType, 0x3c, "F3", "F3" },
    { kKeyboardKeyType, 0x3d, "F4", "F4" },
    { kKeyboardKeyType, 0x3e, "F5", "F5" },
    { kKeyboardKeyType, 0x3f, "F6", "F6" },
    { kKeyboardKeyType, 0x40, "F7", "F7" },
    { kKeyboardKeyType, 0x41, "F8", "F8" },
    { kKeyboardKeyType, 0x42, "F9", "F9" },
    { kKeyboardKeyType, 0x43, "F10", "F10" },
    { kKeyboardKeyType, 0x44, "F11", "F11" },
    { kKeyboardKeyType, 0x45, "F12", "F12" },
    { kKeyboardKeyType, 0x46, "PRINT", "Print Screen" },
    { kKeyboardKeyType, 0x47, "SCRLK", "Scroll Lock" },
    { kKeyboardKeyType, 0x48, "PAUSE", "Pause/Break" },
    { kKeyboardKeyType, 0x35, "GRAVE", "` / ~" },
    { kKeyboardKeyType, 0x1e, "1", "1" },
    { kKeyboardKeyType, 0x1f, "2", "2" },
    { kKeyboardKeyType, 0x20, "3", "3" },
    { kKeyboardKeyType, 0x21, "4", "4" },
    { kKeyboardKeyType, 0x22, "5", "5" },
    { kKeyboardKeyType, 0x23, "6", "6" },
    { kKeyboardKeyType, 0x24, "7", "7" },
    { kKeyboardKeyType, 0x25, "8", "8" },
    { kKeyboardKeyType, 0x26, "9", "9" },
    { kKeyboardKeyType, 0x27, "0", "0" },
    { kKeyboardKeyType, 0x2d, "MINUS", "- / _" },
    { kKeyboardKeyType, 0x2e, "EQUAL", "= / +" },
    { kKeyboardKeyType, 0x2a, "BACKSPACE", "Backspace" },
    { kKeyboardKeyType, 0x2b, "TAB", "Tab" },
    { kKeyboardKeyType, 0x14, "Q", "Q" },
    { kKeyboardKeyType, 0x1a, "W", "W" },
    { kKeyboardKeyType, 0x08, "E", "E" },
    { kKeyboardKeyType, 0x15, "R", "R" },
    { kKeyboardKeyType, 0x17, "T", "T" },
    { kKeyboardKeyType, 0x1c, "Y", "Y" },
    { kKeyboardKeyType, 0x18, "U", "U" },
    { kKeyboardKeyType, 0x0c, "I", "I" },
    { kKeyboardKeyType, 0x12, "O", "O" },
    { kKeyboardKeyType, 0x13, "P", "P" },
    { kKeyboardKeyType, 0x2f, "LBRACKET", "[ / {" },
    { kKeyboardKeyType, 0x30, "RBRACKET", "] / }" },
    { kKeyboardKeyType, 0x28, "ENTER", "Enter" },
    { kKeyboardKeyType, 0x39, "CAPS", "Caps Lock" },
    { kKeyboardKeyType, 0x04, "A", "A" },
    { kKeyboardKeyType, 0x16, "S", "S" },
    { kKeyboardKeyType, 0x07, "D", "D" },
    { kKeyboardKeyType, 0x09, "F", "F" },
    { kKeyboardKeyType, 0x0a, "G", "G" },
    { kKeyboardKeyType, 0x0b, "H", "H" },
    { kKeyboardKeyType, 0x0d, "J", "J" },
    { kKeyboardKeyType, 0x0e, "K", "K" },
    { kKeyboardKeyType, 0x0f, "L", "L" },
    { kKeyboardKeyType, 0x33, "SEMICOLON", "; / :" },
    { kKeyboardKeyType, 0x34, "APOSTROPHE", "' / \"" },
    { kKeyboardKeyType, 0x32, "ISO_ENTER", "ISO \\\\ / | (left of Enter)" },
    { kKeyboardKeyType, 0xe1, "LSHIFT", "Left Shift" },
    { kKeyboardKeyType, 0x64, "ISO_LSHIFT", "ISO \\\\ / | (left of Z)" },
    { kKeyboardKeyType, 0x1d, "Z", "Z" },
    { kKeyboardKeyType, 0x1b, "X", "X" },
    { kKeyboardKeyType, 0x06, "C", "C" },
    { kKeyboardKeyType, 0x19, "V", "V" },
    { kKeyboardKeyType, 0x05, "B", "B" },
    { kKeyboardKeyType, 0x11, "N", "N" },
    { kKeyboardKeyType, 0x10, "M", "M" },
    { kKeyboardKeyType, 0x36, "COMMA", ", / <" },
    { kKeyboardKeyType, 0x37, "DOT", ". / >" },
    { kKeyboardKeyType, 0x38, "SLASH", "/ / ?" },
    { kKeyboardKeyType, 0xe5, "RSHIFT", "Right Shift" },
    { kKeyboardKeyType, 0xe0, "LCTRL", "Left Ctrl" },
    { kKeyboardKeyType, 0xe3, "LWIN", "Left GUI/Windows" },
    { kKeyboardKeyType, 0xe2, "LALT", "Left Alt" },
    { kKeyboardKeyType, 0x2c, "SPACE", "Space" },
    { kKeyboardKeyType, 0xe6, "RALT", "Right Alt/AltGr" },
    { kKeyboardKeyType, 0xe7, "RWIN", "Right GUI/Windows" },
    { kKeyboardKeyType, 0x65, "MENU", "Application/Menu" },
    { kKeyboardKeyType, 0xe4, "RCTRL", "Right Ctrl" },
    { kKeyboardKeyType, 0x49, "INS", "Insert" },
    { kKeyboardKeyType, 0x4a, "HOME", "Home" },
    { kKeyboardKeyType, 0x4b, "PGUP", "Page Up" },
    { kKeyboardKeyType, 0x53, "NUMLOCK", "Num Lock" },
    { kKeyboardKeyType, 0x54, "NUMSLASH", "Keypad /" },
    { kKeyboardKeyType, 0x55, "NUMSTAR", "Keypad *" },
    { kKeyboardKeyType, 0x56, "NUMMINUS", "Keypad -" },
    { kKeyboardKeyType, 0x4c, "DEL", "Delete" },
    { kKeyboardKeyType, 0x4d, "END", "End" },
    { kKeyboardKeyType, 0x4e, "PGDN", "Page Down" },
    { kKeyboardKeyType, 0x5f, "NUM7", "Keypad 7" },
    { kKeyboardKeyType, 0x60, "NUM8", "Keypad 8" },
    { kKeyboardKeyType, 0x61, "NUM9", "Keypad 9" },
    { kKeyboardKeyType, 0x57, "NUMPLUS", "Keypad +" },
    { kKeyboardKeyType, 0x5c, "NUM4", "Keypad 4" },
    { kKeyboardKeyType, 0x5d, "NUM5", "Keypad 5" },
    { kKeyboardKeyType, 0x5e, "NUM6", "Keypad 6" },
    { kKeyboardKeyType, 0x52, "UP", "Arrow Up" },
    { kKeyboardKeyType, 0x59, "NUM1", "Keypad 1" },
    { kKeyboardKeyType, 0x5a, "NUM2", "Keypad 2" },
    { kKeyboardKeyType, 0x5b, "NUM3", "Keypad 3" },
    { kKeyboardKeyType, 0x58, "NUMENTER", "Keypad Enter" },
    { kKeyboardKeyType, 0x50, "LEFT", "Arrow Left" },
    { kKeyboardKeyType, 0x51, "DOWN", "Arrow Down" },
    { kKeyboardKeyType, 0x4f, "RIGHT", "Arrow Right" },
    { kKeyboardKeyType, 0x62, "NUM0", "Keypad 0" },
    { kKeyboardKeyType, 0x63, "NUMDOT", "Keypad ." },
}};


// Additional addresses used by the other physical G810 geometries.\ninline constexpr std::array<LightingElement, 8> kAdditionalKeyboardDefinitions = {{
    { kKeyboardKeyType, 0x31, "BACKSLASH", "\\ / |" },
    { kKeyboardKeyType, 0x87, "INTL1", "International 1" },
    { kKeyboardKeyType, 0x88, "INTL2", "International 2" },
    { kKeyboardKeyType, 0x89, "INTL3", "International 3" },
    { kKeyboardKeyType, 0x8a, "INTL4", "International 4" },
    { kKeyboardKeyType, 0x8b, "INTL5", "International 5" },
    { kKeyboardKeyType, 0x90, "LANG1", "LANG1" },
    { kKeyboardKeyType, 0x91, "LANG2", "LANG2" },
}};

// Physical key-ID sequences extracted from Logitech Gaming Software G810 SVG
// resources. Layouts with identical geometry/key sets intentionally share one
// sequence; their printed legends differ but their 0x8080 addresses do not.
inline constexpr std::array<std::uint8_t, 105> kIso105Ids = {{
    0x29, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b, 0x14,
    0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x28, 0x39, 0x04, 0x16, 0x07,
    0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x32, 0xe1, 0x64, 0x1d, 0x1b, 0x06, 0x19, 0x05,
    0x11, 0x10, 0x36, 0x37, 0x38, 0xe5, 0xe0, 0xe3, 0xe2, 0x2c, 0xe6, 0xe7, 0x65, 0xe4, 0x49, 0x4a,
    0x4b, 0x4c, 0x4d, 0x4e, 0x52, 0x50, 0x51, 0x4f, 0x53, 0x54, 0x55, 0x56, 0x5f, 0x60, 0x61, 0x57,
    0x5c, 0x5d, 0x5e, 0x59, 0x5a, 0x5b, 0x58, 0x62, 0x63
}};

inline constexpr std::array<std::uint8_t, 104> kAnsi104Ids = {{
    0x29, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b, 0x14,
    0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x31, 0x39, 0x04, 0x16, 0x07,
    0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x28, 0xe1, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11,
    0x10, 0x36, 0x37, 0x38, 0xe5, 0xe0, 0xe3, 0xe2, 0x2c, 0xe6, 0xe7, 0x65, 0xe4, 0x49, 0x4a, 0x4b,
    0x4c, 0x4d, 0x4e, 0x52, 0x50, 0x51, 0x4f, 0x53, 0x54, 0x55, 0x56, 0x5f, 0x60, 0x61, 0x57, 0x5c,
    0x5d, 0x5e, 0x59, 0x5a, 0x5b, 0x58, 0x62, 0x63
}};

inline constexpr std::array<std::uint8_t, 87> kAnsi87TklIds = {{
    0x29, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b, 0x14,
    0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x31, 0x39, 0x04, 0x16, 0x07,
    0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x28, 0xe1, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11,
    0x10, 0x36, 0x37, 0x38, 0xe5, 0xe0, 0xe3, 0xe2, 0x2c, 0xe6, 0xe7, 0x65, 0xe4, 0x49, 0x4a, 0x4b,
    0x4c, 0x4d, 0x4e, 0x52, 0x50, 0x51, 0x4f
}};

inline constexpr std::array<std::uint8_t, 88> kIso88TklIds = {{
    0x29, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b, 0x14,
    0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x28, 0x39, 0x04, 0x16, 0x07,
    0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x32, 0xe1, 0x64, 0x1d, 0x1b, 0x06, 0x19, 0x05,
    0x11, 0x10, 0x36, 0x37, 0x38, 0xe5, 0xe0, 0xe3, 0xe2, 0x2c, 0xe6, 0xe7, 0x65, 0xe4, 0x49, 0x4a,
    0x4b, 0x4c, 0x4d, 0x4e, 0x52, 0x50, 0x51, 0x4f
}};

inline constexpr std::array<std::uint8_t, 104> kIntl104Ids = {{
    0x29, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b, 0x14,
    0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x28, 0x39, 0x04, 0x16, 0x07,
    0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x32, 0xe1, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11,
    0x10, 0x36, 0x37, 0x38, 0xe5, 0xe0, 0xe3, 0xe2, 0x2c, 0xe6, 0xe7, 0x65, 0xe4, 0x49, 0x4a, 0x4b,
    0x4c, 0x4d, 0x4e, 0x52, 0x50, 0x51, 0x4f, 0x53, 0x54, 0x55, 0x56, 0x5f, 0x60, 0x61, 0x57, 0x5c,
    0x5d, 0x5e, 0x59, 0x5a, 0x5b, 0x58, 0x62, 0x63
}};

inline constexpr std::array<std::uint8_t, 108> kJpn108Ids = {{
    0x29, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x89, 0x2a, 0x2b,
    0x14, 0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x28, 0x39, 0x04, 0x16,
    0x07, 0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x32, 0xe1, 0x1d, 0x1b, 0x06, 0x19, 0x05,
    0x11, 0x10, 0x36, 0x37, 0x38, 0x87, 0xe5, 0xe0, 0xe3, 0xe2, 0x8b, 0x2c, 0x8a, 0x88, 0xe6, 0x65,
    0xe4, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x52, 0x50, 0x51, 0x4f, 0x53, 0x54, 0x55, 0x56, 0x5f,
    0x60, 0x61, 0x57, 0x5c, 0x5d, 0x5e, 0x59, 0x5a, 0x5b, 0x58, 0x62, 0x63
}};

inline constexpr std::array<std::uint8_t, 106> kKor106Ids = {{
    0x29, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x35, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x2d, 0x2e, 0x2a, 0x2b, 0x14,
    0x1a, 0x08, 0x15, 0x17, 0x1c, 0x18, 0x0c, 0x12, 0x13, 0x2f, 0x30, 0x31, 0x39, 0x04, 0x16, 0x07,
    0x09, 0x0a, 0x0b, 0x0d, 0x0e, 0x0f, 0x33, 0x34, 0x28, 0xe1, 0x1d, 0x1b, 0x06, 0x19, 0x05, 0x11,
    0x10, 0x36, 0x37, 0x38, 0xe5, 0xe0, 0xe3, 0xe2, 0x91, 0x2c, 0x90, 0xe6, 0xe7, 0x65, 0xe4, 0x49,
    0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x52, 0x50, 0x51, 0x4f, 0x53, 0x54, 0x55, 0x56, 0x5f, 0x60, 0x61,
    0x57, 0x5c, 0x5d, 0x5e, 0x59, 0x5a, 0x5b, 0x58, 0x62, 0x63
}};

enum : std::uint8_t {
    kModelG610 = 0x01,
    kModelG810 = 0x02,
    kModelGPro = 0x04,
    kModelG610G810 = kModelG610 | kModelG810,
};

struct KeyboardGeometry {
    const char* name;      // Stable CLI/internal geometry name.
    const char* sources;   // LGS SVG resources with this physical key set.
    const std::uint8_t* keyIds;
    std::size_t keyCount;
    std::uint8_t models;   // kModelG610 / kModelG810 / kModelGPro bitmask.
};

// Five unique physical key sets occur across the shipped G610/G810 SVG
// resources. G PRO ANSI87/ISO88 physical TKL forms are visually confirmed;
// their HID++ key-ID sets remain inferred from the corresponding G610/G810
// full-size address sets by removing the standard 17-key numeric keypad.
inline constexpr std::array<KeyboardGeometry, 7> kGeometries = {{
    { "ANSI104",
      "G610_CHT/KOR/THAI/US + G810_CHT/THAI/US",
      kAnsi104Ids.data(), kAnsi104Ids.size(), kModelG610G810 },
    { "ISO105",
      "G610_DEU/ESP/FRA/INTL/ITA/NORDIC/PIDC333_INTL/SW/TUR/UK + "
      "G810_DEU/FRA/INTL/ITA/NORDIC/PIDC331_INTL/RU/SW/TUR/UK",
      kIso105Ids.data(), kIso105Ids.size(), kModelG610G810 },
    { "JIS108",
      "G610_JPN + G810_JPN",
      kJpn108Ids.data(), kJpn108Ids.size(), kModelG610G810 },
    { "KOR106",
      "G810_KOR",
      kKor106Ids.data(), kKor106Ids.size(), kModelG810 },
    { "INTL104",
      "G610_INTL2/PIDC338_INTL/RU + G810_PIDC337_INTL",
      kIntl104Ids.data(), kIntl104Ids.size(), kModelG610G810 },
    { "ANSI87",
      "G PRO c339 ANSI87 visually confirmed; HID++ IDs inferred from ANSI104 minus numpad",
      kAnsi87TklIds.data(), kAnsi87TklIds.size(), kModelGPro },
    { "ISO88",
      "G PRO c339 ISO88 visually confirmed; HID++ IDs inferred from ISO105 minus numpad",
      kIso88TklIds.data(), kIso88TklIds.size(), kModelGPro },
}};

inline bool geometrySupportsModel(const KeyboardGeometry& geometry,
                                  std::uint8_t modelMask) {
    return (geometry.models & modelMask) != 0;
}

inline constexpr std::array<LightingElement, 5> kMedia = {{
    { kMediaKeyType, 0xb5, "NEXT", "Next track" },
    { kMediaKeyType, 0xb6, "PREV", "Previous track" },
    { kMediaKeyType, 0xb7, "STOP", "Stop" },
    { kMediaKeyType, 0xcd, "PLAY", "Play/Pause" },
    { kMediaKeyType, 0xe2, "MUTE", "Mute" },
}};

inline constexpr std::array<LightingElement, 5> kIndicators = {{
    { kIndicatorKeyType, 0x01, "LIGHT", "Lighting / backlight button" },
    { kIndicatorKeyType, 0x02, "GAME", "Game-mode status/button" },
    { kIndicatorKeyType, 0x03, "CAPS_LED", "Caps Lock indicator" },
    { kIndicatorKeyType, 0x04, "SCROLL_LED", "Scroll Lock indicator" },
    { kIndicatorKeyType, 0x05, "NUM_LED", "Num Lock indicator" },
}};

inline constexpr std::array<LightingElement, 1> kLogo = {{
    { kLogoKeyType, 0x01, "LOGO", "Logo" },
}};

constexpr char asciiLower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

constexpr bool asciiIEquals(std::string_view a, std::string_view b) {
    if(a.size() != b.size()) {
        return false;
    }
    for(std::size_t i = 0; i < a.size(); ++i) {
        if(asciiLower(a[i]) != asciiLower(b[i])) {
            return false;
        }
    }
    return true;
}

template <std::size_t N>
inline const LightingElement* findById(
    const std::array<LightingElement, N>& elements, std::uint8_t keyId) {
    for(const auto& element : elements) {
        if(element.keyId == keyId) {
            return &element;
        }
    }
    return nullptr;
}

inline const LightingElement* keyboardDefinitionById(std::uint8_t keyId) {
    if(const auto* key = findById(kBaseKeyboardDefinitions, keyId)) {
        return key;
    }
    return findById(kAdditionalKeyboardDefinitions, keyId);
}

inline const LightingElement* keyboardDefinitionByName(std::string_view name) {
    for(const auto& key : kBaseKeyboardDefinitions) {
        if(asciiIEquals(key.name, name)) {
            return &key;
        }
    }
    for(const auto& key : kAdditionalKeyboardDefinitions) {
        if(asciiIEquals(key.name, name)) {
            return &key;
        }
    }
    return nullptr;
}

inline const KeyboardGeometry* findGeometry(std::string_view name) {
    for(const auto& geometry : kGeometries) {
        if(asciiIEquals(geometry.name, name)) {
            return &geometry;
        }
    }
    return nullptr;
}

inline bool geometryHasKey(const KeyboardGeometry& geometry, std::uint8_t keyId) {
    for(std::size_t i = 0; i < geometry.keyCount; ++i) {
        if(geometry.keyIds[i] == keyId) {
            return true;
        }
    }
    return false;
}

inline const LightingElement* findKeyboardById(
    const KeyboardGeometry& geometry, std::uint8_t keyId) {
    if(!geometryHasKey(geometry, keyId)) {
        return nullptr;
    }
    return keyboardDefinitionById(keyId);
}

inline const LightingElement* findKeyboardByName(
    const KeyboardGeometry& geometry, std::string_view name) {
    const auto* key = keyboardDefinitionByName(name);
    if(!key || !geometryHasKey(geometry, key->keyId)) {
        return nullptr;
    }
    return key;
}

// Backwards-compatible ISO105 helpers used by older diagnostic callers.
inline const LightingElement* findKeyboardById(std::uint8_t keyId) {
    return findById(kBaseKeyboardDefinitions, keyId);
}

inline const LightingElement* findKeyboardByName(std::string_view name) {
    for(const auto& key : kBaseKeyboardDefinitions) {
        if(asciiIEquals(key.name, name)) {
            return &key;
        }
    }
    return nullptr;
}

inline std::size_t physicalLightingCount(const KeyboardGeometry& geometry) {
    return geometry.keyCount + kMedia.size() + kIndicators.size() + kLogo.size();
}

inline const LightingElement* findPhysical(
    std::uint16_t keyType, std::uint8_t keyId) {
    switch(keyType) {
        case kKeyboardKeyType: return keyboardDefinitionById(keyId);
        case kMediaKeyType: return findById(kMedia, keyId);
        case kIndicatorKeyType: return findById(kIndicators, keyId);
        case kLogoKeyType: return findById(kLogo, keyId);
        default: return nullptr;
    }
}

} // namespace krgb::logitech::g610_g810
