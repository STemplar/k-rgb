// be quiet! Light Mount US-ANSI visual layout for the GUI per-key editor.
//
// Geometry follows the physical keyboard image supplied during hardware
// validation. Lighting addresses come from the validated vendor LED map.
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/lightmount_keymap.h"
#include "core/lightmount_map.h"

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
inline constexpr std::size_t kExpectedKeyCount = 110; // 109 keys + media wheel
inline constexpr const char* kMediaWheelName = "MEDIA_WHEEL";

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

        // The label argument is deliberately the primary function only.  The
        // physical keymap also stores shifted legends (1!, 2@, etc.), but they
        // make the compact GUI drawing unreadable.
        auto add = [&out](const char* name, float x, float y,
                          float w = 1.0f, float h = 1.0f,
                          const char* label = nullptr) {
            if(const auto* key = keyByName(name)) {
                out.push_back({key->ledId, key->name,
                               label ? label : key->name,
                               x, y, w, h});
            }
        };

        // 3D Media Wheel and macro column.
        out.push_back({k3DMediaWheelLed, kMediaWheelName, "Mute",
                       0.00f, 0.00f, 1.0f, 1.0f});
        add("M1", 0.00f, 1.45f);
        add("M2", 0.00f, 2.55f);
        add("M3", 0.00f, 3.65f);
        add("M4", 0.00f, 4.75f);
        add("M5", 0.00f, 5.85f);

        // Function row.
        add("ESC",   1.35f, 0.00f, 1, 1, "Esc");
        add("F1",    2.75f, 0.00f); add("F2", 3.75f, 0.00f);
        add("F3",    4.75f, 0.00f); add("F4", 5.75f, 0.00f);
        add("F5",    7.00f, 0.00f); add("F6", 8.00f, 0.00f);
        add("F7",    9.00f, 0.00f); add("F8",10.00f, 0.00f);
        add("F9",   11.25f, 0.00f); add("F10",12.25f,0.00f);
        add("F11",  13.25f, 0.00f); add("F12",14.25f,0.00f);
        add("PRINT", 16.75f, 0.00f, 1, 1, "Print");
        add("SCRLK", 17.75f, 0.00f, 1, 1, "ScrLk");
        add("PAUSE", 18.75f, 0.00f, 1, 1, "Pause");

        // Number row. Main ANSI block ends at x=16.35; navigation starts at
        // x=16.75, leaving the physical gap visible on a full-size keyboard.
        float x = 1.35f;
        add("GRAVE", x,1.45f,1,1,"`"); x += 1.0f;
        for(const char* n : {"1","2","3","4","5","6","7","8","9","0"}) {
            add(n,x,1.45f); x += 1.0f;
        }
        add("MINUS",x,1.45f,1,1,"-"); x += 1.0f;
        add("EQUAL",x,1.45f,1,1,"="); x += 1.0f;
        add("BACKSPACE",x,1.45f,2.0f,1,"Back");
        add("INS", 16.75f,1.45f,1,1,"Ins");
        add("HOME",17.75f,1.45f,1,1,"Home");
        add("PGUP",18.75f,1.45f,1,1,"PgUp");
        add("NUMLOCK",20.00f,1.45f,1,1,"Num");
        add("NUMSLASH",21.00f,1.45f,1,1,"/");
        add("NUMSTAR",22.00f,1.45f,1,1,"*");
        add("NUMMINUS",23.00f,1.45f,1,1,"-");

        // QWERTY row.
        x = 1.35f;
        add("TAB",x,2.55f,1.5f,1,"Tab"); x += 1.5f;
        for(const char* n : {"Q","W","E","R","T","Y","U","I","O","P"}) {
            add(n,x,2.55f); x += 1.0f;
        }
        add("LBRACKET",x,2.55f,1,1,"["); x += 1.0f;
        add("RBRACKET",x,2.55f,1,1,"]"); x += 1.0f;
        add("BACKSLASH",x,2.55f,1.5f,1,"\\");
        add("DEL", 16.75f,2.55f,1,1,"Del");
        add("END", 17.75f,2.55f,1,1,"End");
        add("PGDN",18.75f,2.55f,1,1,"PgDn");
        add("NUM7",20.00f,2.55f,1,1,"7");
        add("NUM8",21.00f,2.55f,1,1,"8");
        add("NUM9",22.00f,2.55f,1,1,"9");
        add("NUMPLUS",23.00f,2.55f,1.0f,2.0f,"+");

        // Home row.
        x = 1.35f;
        add("CAPS",x,3.65f,1.75f,1,"Caps"); x += 1.75f;
        for(const char* n : {"A","S","D","F","G","H","J","K","L"}) {
            add(n,x,3.65f); x += 1.0f;
        }
        add("SEMICOLON",x,3.65f,1,1,";"); x += 1.0f;
        add("APOSTROPHE",x,3.65f,1,1,"'"); x += 1.0f;
        add("ENTER",x,3.65f,2.25f,1,"Enter");
        add("NUM4",20.00f,3.65f,1,1,"4");
        add("NUM5",21.00f,3.65f,1,1,"5");
        add("NUM6",22.00f,3.65f,1,1,"6");

        // Shift row. The inverted-T cursor cluster is separated from the main
        // block exactly like a conventional full-size ANSI layout.
        x = 1.35f;
        add("LSHIFT",x,4.75f,2.25f,1,"Shift"); x += 2.25f;
        for(const char* n : {"Z","X","C","V","B","N","M"}) {
            add(n,x,4.75f); x += 1.0f;
        }
        add("COMMA",x,4.75f,1,1,","); x += 1.0f;
        add("DOT",x,4.75f,1,1,"."); x += 1.0f;
        add("SLASH",x,4.75f,1,1,"/"); x += 1.0f;
        add("RSHIFT",x,4.75f,2.75f,1,"Shift");
        add("UP",17.75f,4.75f,1,1,"↑");
        add("NUM1",20.00f,4.75f,1,1,"1");
        add("NUM2",21.00f,4.75f,1,1,"2");
        add("NUM3",22.00f,4.75f,1,1,"3");
        add("NUMENTER",23.00f,4.75f,1.0f,2.0f,"Enter");

        // Bottom row and lower half of cursor cluster.
        x = 1.35f;
        add("LCTRL",x,5.85f,1.25f,1,"Ctrl"); x += 1.25f;
        add("LWIN", x,5.85f,1.25f,1,"Win"); x += 1.25f;
        add("LALT", x,5.85f,1.25f,1,"Alt"); x += 1.25f;
        add("SPACE",x,5.85f,6.25f,1,"Space"); x += 6.25f;
        add("RALT", x,5.85f,1.25f,1,"Alt"); x += 1.25f;
        add("RWIN", x,5.85f,1.25f,1,"Win"); x += 1.25f;
        add("FN",   x,5.85f,1.25f,1,"Fn"); x += 1.25f;
        add("RCTRL",x,5.85f,1.25f,1,"Ctrl");
        add("LEFT", 16.75f,5.85f,1,1,"←");
        add("DOWN", 17.75f,5.85f,1,1,"↓");
        add("RIGHT",18.75f,5.85f,1,1,"→");
        add("NUM0",20.00f,5.85f,2.0f,1,"0");
        add("NUMDOT",22.00f,5.85f,1,1,".");

        return out;
    }();
    return data;
}

} // namespace krgb::lightmount::ansi_visual
