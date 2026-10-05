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
    const char* primary;
    const char* secondary;
    float x;
    float y;
    float w;
    float h;
    bool round;
};

inline constexpr float kLayoutWidth = 24.55f;
inline constexpr float kLayoutHeight = 7.10f;
inline constexpr std::size_t kExpectedKeyCount = 110; // 109 keys + 3D Media Wheel
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

        auto add = [&out](const char* name, float x, float y,
                          float w = 1.0f, float h = 1.0f,
                          const char* primary = nullptr,
                          const char* secondary = nullptr) {
            if(const auto* key = keyByName(name)) {
                out.push_back({key->ledId, key->name,
                               primary ? primary : key->legend,
                               secondary ? secondary : "",
                               x, y, w, h, false});
            }
        };

        // The physical 3D Media Wheel is visibly larger than a keycap. The
        // supplied product image puts it at roughly 1.3 key widths in diameter.
        out.push_back({k3DMediaWheelLed, kMediaWheelName, "Mute", "",
                       0.00f, 0.00f, 1.32f, 1.32f, true});

        // Macro column.
        add("M1", 0.12f, 1.52f);
        add("M2", 0.12f, 2.62f);
        add("M3", 0.12f, 3.72f);
        add("M4", 0.12f, 4.82f);
        add("M5", 0.12f, 5.92f);

        // Function row.
        add("ESC",   1.55f, 0.12f, 1, 1, "Esc");
        add("F1",    2.95f, 0.12f); add("F2", 3.95f, 0.12f);
        add("F3",    4.95f, 0.12f); add("F4", 5.95f, 0.12f);
        add("F5",    7.20f, 0.12f); add("F6", 8.20f, 0.12f);
        add("F7",    9.20f, 0.12f); add("F8",10.20f, 0.12f);
        add("F9",   11.45f, 0.12f); add("F10",12.45f,0.12f);
        add("F11",  13.45f, 0.12f); add("F12",14.45f,0.12f);
        add("PRINT", 16.90f, 0.12f, 1, 1, "Print");
        add("SCRLK", 17.90f, 0.12f, 1, 1, "Scr Lk");
        add("PAUSE", 18.90f, 0.12f, 1, 1, "Pause");

        // Main typing rows. Row pitch is 1.10 key units, matching the product
        // image; the navigation and numpad blocks use exactly the same pitch.
        constexpr float y1 = 1.52f;
        constexpr float y2 = 2.62f;
        constexpr float y3 = 3.72f;
        constexpr float y4 = 4.82f;
        constexpr float y5 = 5.92f;

        float x = 1.55f;
        add("GRAVE", x,y1,1,1,"`","~"); x += 1.0f;
        add("1",x,y1,1,1,"1","!"); x += 1.0f;
        add("2",x,y1,1,1,"2","@"); x += 1.0f;
        add("3",x,y1,1,1,"3","#"); x += 1.0f;
        add("4",x,y1,1,1,"4","$"); x += 1.0f;
        add("5",x,y1,1,1,"5","%"); x += 1.0f;
        add("6",x,y1,1,1,"6","^"); x += 1.0f;
        add("7",x,y1,1,1,"7","&"); x += 1.0f;
        add("8",x,y1,1,1,"8","*"); x += 1.0f;
        add("9",x,y1,1,1,"9","("); x += 1.0f;
        add("0",x,y1,1,1,"0",")"); x += 1.0f;
        add("MINUS",x,y1,1,1,"-","_"); x += 1.0f;
        add("EQUAL",x,y1,1,1,"=","+"); x += 1.0f;
        add("BACKSPACE",x,y1,2.0f,1,"Back");

        // Standard six-key navigation cluster.
        add("INS", 16.90f,y1,1,1,"Ins");
        add("HOME",17.90f,y1,1,1,"Home");
        add("PGUP",18.90f,y1,1,1,"PgUp");

        // Standard four-column numpad. The + and Enter keys each span exactly
        // two row pitches, so their top/bottom edges line up with neighbours.
        add("NUMLOCK",20.20f,y1,1,1,"Num");
        add("NUMSLASH",21.20f,y1,1,1,"/");
        add("NUMSTAR",22.20f,y1,1,1,"*");
        add("NUMMINUS",23.20f,y1,1,1,"-");

        x = 1.55f;
        add("TAB",x,y2,1.5f,1,"Tab"); x += 1.5f;
        for(const char* n : {"Q","W","E","R","T","Y","U","I","O","P"}) {
            add(n,x,y2); x += 1.0f;
        }
        add("LBRACKET",x,y2,1,1,"[","{"); x += 1.0f;
        add("RBRACKET",x,y2,1,1,"]","}"); x += 1.0f;
        add("BACKSLASH",x,y2,1.5f,1,"\\","|");
        add("DEL", 16.90f,y2,1,1,"Del");
        add("END", 17.90f,y2,1,1,"End");
        add("PGDN",18.90f,y2,1,1,"PgDn");
        add("NUM7",20.20f,y2,1,1,"7");
        add("NUM8",21.20f,y2,1,1,"8");
        add("NUM9",22.20f,y2,1,1,"9");
        add("NUMPLUS",23.20f,y2,1.0f,2.10f,"+");

        x = 1.55f;
        add("CAPS",x,y3,1.75f,1,"Caps"); x += 1.75f;
        for(const char* n : {"A","S","D","F","G","H","J","K","L"}) {
            add(n,x,y3); x += 1.0f;
        }
        add("SEMICOLON",x,y3,1,1,";",":"); x += 1.0f;
        add("APOSTROPHE",x,y3,1,1,"'","\""); x += 1.0f;
        add("ENTER",x,y3,2.25f,1,"Enter");
        add("NUM4",20.20f,y3,1,1,"4");
        add("NUM5",21.20f,y3,1,1,"5");
        add("NUM6",22.20f,y3,1,1,"6");

        x = 1.55f;
        add("LSHIFT",x,y4,2.25f,1,"Shift"); x += 2.25f;
        for(const char* n : {"Z","X","C","V","B","N","M"}) {
            add(n,x,y4); x += 1.0f;
        }
        add("COMMA",x,y4,1,1,",","<"); x += 1.0f;
        add("DOT",x,y4,1,1,".",">"); x += 1.0f;
        add("SLASH",x,y4,1,1,"/","?"); x += 1.0f;
        add("RSHIFT",x,y4,2.75f,1,"Shift");

        // Conventional inverted-T cursor block, centred under the six-key
        // navigation cluster.
        add("UP",17.90f,y4,1,1,"↑");
        add("NUM1",20.20f,y4,1,1,"1");
        add("NUM2",21.20f,y4,1,1,"2");
        add("NUM3",22.20f,y4,1,1,"3");
        add("NUMENTER",23.20f,y4,1.0f,2.10f,"Enter");

        x = 1.55f;
        add("LCTRL",x,y5,1.25f,1,"Ctrl"); x += 1.25f;
        add("LWIN", x,y5,1.25f,1,"Win"); x += 1.25f;
        add("LALT", x,y5,1.25f,1,"Alt"); x += 1.25f;
        add("SPACE",x,y5,6.25f,1,"Space"); x += 6.25f;
        add("RALT", x,y5,1.25f,1,"Alt"); x += 1.25f;
        add("RWIN", x,y5,1.25f,1,"Win"); x += 1.25f;
        add("FN",   x,y5,1.25f,1,"Fn"); x += 1.25f;
        add("RCTRL",x,y5,1.25f,1,"Ctrl");
        add("LEFT", 16.90f,y5,1,1,"←");
        add("DOWN", 17.90f,y5,1,1,"↓");
        add("RIGHT",18.90f,y5,1,1,"→");
        add("NUM0",20.20f,y5,2.0f,1,"0");
        add("NUMDOT",22.20f,y5,1,1,".");

        return out;
    }();
    return data;
}

} // namespace krgb::lightmount::ansi_visual
