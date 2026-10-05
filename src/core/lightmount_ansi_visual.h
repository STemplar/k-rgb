// be quiet! Light Mount ANSI visual layout for the GUI per-key editor.
//
// Geometry is based on the physical ANSI product layout supplied during
// hardware validation. Lighting addresses and stable names come from the
// hardware-validated Light Mount vendor LED map (lightmount_keymap.h).
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/lightmount_keymap.h"

namespace krgb::lightmount::ansi_visual {

struct VisualKey {
    std::uint16_t ledId;
    const char* name;
    const char* label;
    float x;
    float y;
    float w;
    float h;
};

inline constexpr float kLayoutWidth = 24.4f;
inline constexpr float kLayoutHeight = 7.15f;
inline constexpr std::size_t kExpectedKeyCount = 109;

inline const LightMountKey* keyByName(std::string_view name) {
    for(const auto& key : kKeys) {
        if(name == key.name) {
            return &key;
        }
    }
    return nullptr;
}

inline const std::vector<VisualKey>& keys() {
    static const std::vector<VisualKey> data = [] {
        std::vector<VisualKey> out;
        out.reserve(kExpectedKeyCount);

        auto add = [&out](const char* name, float x, float y,
                          float w = 1.0f, float h = 1.0f) {
            if(const auto* key = keyByName(name)) {
                out.push_back({key->ledId, key->name, key->legend, x, y, w, h});
            }
        };

        // Macro column, physically separated from the ANSI key field.
        add("M1", 0.00f, 1.45f);
        add("M2", 0.00f, 2.55f);
        add("M3", 0.00f, 3.65f);
        add("M4", 0.00f, 4.75f);
        add("M5", 0.00f, 5.85f);

        // Function row.
        add("ESC",   1.35f, 0.00f);
        add("F1",    2.75f, 0.00f); add("F2", 3.75f, 0.00f);
        add("F3",    4.75f, 0.00f); add("F4", 5.75f, 0.00f);
        add("F5",    7.00f, 0.00f); add("F6", 8.00f, 0.00f);
        add("F7",    9.00f, 0.00f); add("F8",10.00f, 0.00f);
        add("F9",   11.25f, 0.00f); add("F10",12.25f,0.00f);
        add("F11",  13.25f, 0.00f); add("F12",14.25f,0.00f);
        add("PRINT", 15.65f, 0.00f); add("SCRLK",16.65f,0.00f);
        add("PAUSE", 17.65f, 0.00f);

        // Number row.
        float x = 1.35f;
        add("GRAVE", x,1.45f); x += 1.0f;
        for(const char* n : {"1","2","3","4","5","6","7","8","9","0","MINUS","EQUAL"}) {
            add(n,x,1.45f); x += 1.0f;
        }
        add("BACKSPACE",x,1.45f,2.0f);
        add("INS",15.65f,1.45f); add("HOME",16.65f,1.45f); add("PGUP",17.65f,1.45f);
        add("NUMLOCK",19.00f,1.45f); add("NUMSLASH",20.00f,1.45f);
        add("NUMSTAR",21.00f,1.45f); add("NUMMINUS",22.00f,1.45f);

        // QWERTY row.
        x = 1.35f;
        add("TAB",x,2.55f,1.5f); x += 1.5f;
        for(const char* n : {"Q","W","E","R","T","Y","U","I","O","P","LBRACKET","RBRACKET"}) {
            add(n,x,2.55f); x += 1.0f;
        }
        add("BACKSLASH",x,2.55f,1.5f);
        add("DEL",15.65f,2.55f); add("END",16.65f,2.55f); add("PGDN",17.65f,2.55f);
        add("NUM7",19.00f,2.55f); add("NUM8",20.00f,2.55f); add("NUM9",21.00f,2.55f);
        add("NUMPLUS",22.00f,2.55f,1.0f,2.0f);

        // Home row.
        x = 1.35f;
        add("CAPS",x,3.65f,1.75f); x += 1.75f;
        for(const char* n : {"A","S","D","F","G","H","J","K","L","SEMICOLON","APOSTROPHE"}) {
            add(n,x,3.65f); x += 1.0f;
        }
        add("ENTER",x,3.65f,2.25f);
        add("NUM4",19.00f,3.65f); add("NUM5",20.00f,3.65f); add("NUM6",21.00f,3.65f);

        // Shift row.
        x = 1.35f;
        add("LSHIFT",x,4.75f,2.25f); x += 2.25f;
        for(const char* n : {"Z","X","C","V","B","N","M","COMMA","DOT","SLASH"}) {
            add(n,x,4.75f); x += 1.0f;
        }
        add("RSHIFT",x,4.75f,2.75f);
        add("UP",16.65f,4.75f);
        add("NUM1",19.00f,4.75f); add("NUM2",20.00f,4.75f); add("NUM3",21.00f,4.75f);
        add("NUMENTER",22.00f,4.75f,1.0f,2.0f);

        // Bottom row.
        x = 1.35f;
        add("LCTRL",x,5.85f,1.25f); x += 1.25f;
        add("LWIN", x,5.85f,1.25f); x += 1.25f;
        add("LALT", x,5.85f,1.25f); x += 1.25f;
        add("SPACE",x,5.85f,6.25f); x += 6.25f;
        add("RALT", x,5.85f,1.25f); x += 1.25f;
        add("RWIN", x,5.85f,1.25f); x += 1.25f;
        add("FN",   x,5.85f,1.25f); x += 1.25f;
        add("RCTRL",x,5.85f,1.25f);
        add("LEFT",15.65f,5.85f); add("DOWN",16.65f,5.85f); add("RIGHT",17.65f,5.85f);
        add("NUM0",19.00f,5.85f,2.0f); add("NUMDOT",21.00f,5.85f);

        return out;
    }();
    return data;
}

} // namespace krgb::lightmount::ansi_visual
