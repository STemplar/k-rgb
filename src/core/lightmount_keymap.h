// Physical RGB mapping for the be quiet! Light Mount vendor interface.
//
// LED 45 is the illuminated 3D Media Wheel. Vendor LED IDs 46..157 form the
// row-major keyboard matrix; 119, 126 and 137 are unpopulated positions.
// IDs 0..44 are the top light bar, 158..162 the left strip and 163..167 the
// right strip. Only M1..M5 are function-remappable; every entry below is
// independently RGB-addressable and may be exposed by the per-key editor.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace krgb::lightmount {

struct LightMountKey {
    std::uint16_t ledId;
    const char* name;       // Stable internal / CLI/profile name.
    const char* legend;     // Label shown on the physical keyboard; empty for accent LEDs.
    bool remappable;        // Function remapping is available only on M1..M5.
};

// Keep the physical keyboard elements first so existing key-order-dependent
// behaviour remains stable. Accent LEDs are appended afterwards and therefore
// become selectable/profile-addressable without changing the key matrix order.
inline constexpr std::array<LightMountKey, 165> kKeys = {{
    { 45, "MEDIA_WHEEL", "Mute",   false },

    { 46, "ESC",        "Esc",    false },
    { 47, "F1",         "F1",     false },
    { 48, "F2",         "F2",     false },
    { 49, "F3",         "F3",     false },
    { 50, "F4",         "F4",     false },
    { 51, "F5",         "F5",     false },
    { 52, "F6",         "F6",     false },
    { 53, "F7",         "F7",     false },
    { 54, "F8",         "F8",     false },
    { 55, "F9",         "F9",     false },
    { 56, "F10",        "F10",    false },
    { 57, "F11",        "F11",    false },
    { 58, "F12",        "F12",    false },
    { 59, "PRINT",      "Print",  false },
    { 60, "SCRLK",      "Scr Lk", false },
    { 61, "PAUSE",      "Pause",  false },

    { 62, "M1",         "M1",     true  },
    { 63, "GRAVE",      "` ~",    false },
    { 64, "1",          "1 !",    false },
    { 65, "2",          "2 @",    false },
    { 66, "3",          "3 #",    false },
    { 67, "4",          "4 $",    false },
    { 68, "5",          "5 %",    false },
    { 69, "6",          "6 ^",    false },
    { 70, "7",          "7 &",    false },
    { 71, "8",          "8 *",    false },
    { 72, "9",          "9 (",    false },
    { 73, "0",          "0 )",    false },
    { 74, "MINUS",      "- _",    false },
    { 75, "EQUAL",      "= +",    false },
    { 76, "BACKSPACE",  "Back",   false },
    { 77, "INS",        "Ins",    false },
    { 78, "HOME",       "Home",   false },
    { 79, "PGUP",       "Pg Up",  false },
    { 80, "NUMLOCK",    "Num",    false },
    { 81, "NUMSLASH",   "/",      false },
    { 82, "NUMSTAR",    "*",      false },
    { 83, "NUMMINUS",   "-",      false },

    { 84, "M2",         "M2",     true  },
    { 85, "TAB",        "Tab",    false },
    { 86, "Q",          "Q",      false },
    { 87, "W",          "W",      false },
    { 88, "E",          "E",      false },
    { 89, "R",          "R",      false },
    { 90, "T",          "T",      false },
    { 91, "Y",          "Y",      false },
    { 92, "U",          "U",      false },
    { 93, "I",          "I",      false },
    { 94, "O",          "O",      false },
    { 95, "P",          "P",      false },
    { 96, "LBRACKET",   "[ {",    false },
    { 97, "RBRACKET",   "] }",    false },
    { 98, "BACKSLASH",  "\\ |",   false },
    { 99, "DEL",        "Del",    false },
    {100, "END",        "End",    false },
    {101, "PGDN",       "Pg Dn",  false },
    {102, "NUM7",       "7",      false },
    {103, "NUM8",       "8",      false },
    {104, "NUM9",       "9",      false },
    {105, "NUMPLUS",    "+",      false },

    {106, "M3",         "M3",     true  },
    {107, "CAPS",       "Caps",   false },
    {108, "A",          "A",      false },
    {109, "S",          "S",      false },
    {110, "D",          "D",      false },
    {111, "F",          "F",      false },
    {112, "G",          "G",      false },
    {113, "H",          "H",      false },
    {114, "J",          "J",      false },
    {115, "K",          "K",      false },
    {116, "L",          "L",      false },
    {117, "SEMICOLON",  "; :",    false },
    {118, "APOSTROPHE", "' \"",   false },
    {120, "ENTER",      "Enter",  false },
    {121, "NUM4",       "4",      false },
    {122, "NUM5",       "5",      false },
    {123, "NUM6",       "6",      false },

    {124, "M4",         "M4",     true  },
    {125, "LSHIFT",     "Shift",  false },
    {127, "Z",          "Z",      false },
    {128, "X",          "X",      false },
    {129, "C",          "C",      false },
    {130, "V",          "V",      false },
    {131, "B",          "B",      false },
    {132, "N",          "N",      false },
    {133, "M",          "M",      false },
    {134, "COMMA",      ", <",    false },
    {135, "DOT",        ". >",    false },
    {136, "SLASH",      "/ ?",    false },
    {138, "RSHIFT",     "Shift",  false },
    {139, "UP",         "↑",      false },
    {140, "NUM1",       "1",      false },
    {141, "NUM2",       "2",      false },
    {142, "NUM3",       "3",      false },
    {143, "NUMENTER",   "Enter",  false },

    {144, "M5",         "M5",     true  },
    {145, "LCTRL",      "Ctrl",   false },
    {146, "LWIN",       "Win",    false },
    {147, "LALT",       "Alt",    false },
    {148, "SPACE",      "⎵",      false },
    {149, "RALT",       "Alt",    false },
    {150, "RWIN",       "Win",    false },
    {151, "FN",         "Fn",     false },
    {152, "RCTRL",      "Ctrl",   false },
    {153, "LEFT",       "←",      false },
    {154, "DOWN",       "↓",      false },
    {155, "RIGHT",      "→",      false },
    {156, "NUM0",       "0",      false },
    {157, "NUMDOT",     ".",      false },

    // Top light bar, left -> right.
    {  0, "TOPBAR_00", "", false }, {  1, "TOPBAR_01", "", false },
    {  2, "TOPBAR_02", "", false }, {  3, "TOPBAR_03", "", false },
    {  4, "TOPBAR_04", "", false }, {  5, "TOPBAR_05", "", false },
    {  6, "TOPBAR_06", "", false }, {  7, "TOPBAR_07", "", false },
    {  8, "TOPBAR_08", "", false }, {  9, "TOPBAR_09", "", false },
    { 10, "TOPBAR_10", "", false }, { 11, "TOPBAR_11", "", false },
    { 12, "TOPBAR_12", "", false }, { 13, "TOPBAR_13", "", false },
    { 14, "TOPBAR_14", "", false }, { 15, "TOPBAR_15", "", false },
    { 16, "TOPBAR_16", "", false }, { 17, "TOPBAR_17", "", false },
    { 18, "TOPBAR_18", "", false }, { 19, "TOPBAR_19", "", false },
    { 20, "TOPBAR_20", "", false }, { 21, "TOPBAR_21", "", false },
    { 22, "TOPBAR_22", "", false }, { 23, "TOPBAR_23", "", false },
    { 24, "TOPBAR_24", "", false }, { 25, "TOPBAR_25", "", false },
    { 26, "TOPBAR_26", "", false }, { 27, "TOPBAR_27", "", false },
    { 28, "TOPBAR_28", "", false }, { 29, "TOPBAR_29", "", false },
    { 30, "TOPBAR_30", "", false }, { 31, "TOPBAR_31", "", false },
    { 32, "TOPBAR_32", "", false }, { 33, "TOPBAR_33", "", false },
    { 34, "TOPBAR_34", "", false }, { 35, "TOPBAR_35", "", false },
    { 36, "TOPBAR_36", "", false }, { 37, "TOPBAR_37", "", false },
    { 38, "TOPBAR_38", "", false }, { 39, "TOPBAR_39", "", false },
    { 40, "TOPBAR_40", "", false }, { 41, "TOPBAR_41", "", false },
    { 42, "TOPBAR_42", "", false }, { 43, "TOPBAR_43", "", false },
    { 44, "TOPBAR_44", "", false },

    // Side strips. Physical ordering follows the validated vendor LED map.
    {158, "LEFT_STRIP_0",  "", false }, {159, "LEFT_STRIP_1",  "", false },
    {160, "LEFT_STRIP_2",  "", false }, {161, "LEFT_STRIP_3",  "", false },
    {162, "LEFT_STRIP_4",  "", false },
    {163, "RIGHT_STRIP_0", "", false }, {164, "RIGHT_STRIP_1", "", false },
    {165, "RIGHT_STRIP_2", "", false }, {166, "RIGHT_STRIP_3", "", false },
    {167, "RIGHT_STRIP_4", "", false },
}};

inline constexpr std::array<std::uint16_t, 3> kInactiveKeyLedIds = {{
    119, 126, 137
}};

inline constexpr std::size_t kPhysicalKeyCount = 110; // 109 keycaps + media wheel
inline constexpr std::size_t kRgbElementCount = kKeys.size();
// Backward-compatible physical-key count for code that uses this as a layout count.
inline constexpr std::size_t kKeyCount = kPhysicalKeyCount;

} // namespace krgb::lightmount
