// be quiet! Light Mount US-ANSI visual layout for the GUI per-key editor.
//
// Chassis: manufacturer dimensions, 461 x 132 mm without the palmrest.
// Keys use the conventional full-size US-ANSI grid with a 19.05 mm unit.
// Placement of that grid within the chassis and the extra controls follows the
// reference image. The user supplied a 21 mm media wheel diameter.
// Lighting addresses and legends come from the validated vendor
// LED/key map. The top light bar and both side strips are represented as
// individually selectable RGB elements as well.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

#include "core/lightmount_keymap.h"
#include "core/lightmount_map.h"

namespace krgb::lightmount::ansi_visual {

struct VisualKey {
    std::uint16_t ledId;
    const char* name;
    const char* label;
    float x; // millimetres in the calibrated top view
    float y;
    float w;
    float h;
    bool round;
};

inline constexpr float kLayoutWidth = 461.0f; // mm, left to right
inline constexpr float kLayoutHeight = 132.0f; // mm, front to back
inline constexpr float kSideStripWidthMm = 6.0f;
inline constexpr float kSideStripLengthMm = 65.0f;
inline constexpr float kSideStripDisplayGapMm = 3.0f;
inline constexpr float kSideDisplayMarginMm = kSideStripWidthMm + kSideStripDisplayGapMm;
inline constexpr float kVisualWidth = kLayoutWidth + 2.0f * kSideDisplayMarginMm;
inline constexpr float kChassisHeightMm = 44.0f; // vertical; not part of the 2D layout
inline constexpr float kKeyUnitMm = 19.05f;
inline constexpr float kKeycapGapMm = 1.05f; // 18 mm cap on the 19.05 mm grid
inline constexpr float kMediaWheelDiameterMm = 21.0f;
inline constexpr float kFunctionRowYmm = 7.0f;
inline constexpr float kNavigationOffsetU = 15.25f;
inline constexpr float kNumpadOffsetU = 18.5f;
inline constexpr float kChassisSideKeyMarginMm = 6.0f;
inline constexpr float kMacroColumnXmm = kChassisSideKeyMarginMm;
inline constexpr float kMainBlockXmm = kLayoutWidth - kChassisSideKeyMarginMm -
    (kNumpadOffsetU + 4.0f) * kKeyUnitMm;
inline constexpr float kTypingRowOffsetU = 1.25f;
inline constexpr std::size_t kExpectedPhysicalKeyCount = 110; // 109 keys + 3D Media Wheel
inline constexpr std::size_t kExpectedAccentCount = 55;      // 45 top + 5 left + 5 right
inline constexpr std::size_t kExpectedElementCount =
    kExpectedPhysicalKeyCount + kExpectedAccentCount;
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
        out.reserve(kExpectedElementCount);
        constexpr float sourceWidth = 25.11f;
        constexpr float sourceHeight = 7.42f;

        auto add = [&out](const char* name, float x, float y,
                          float w = 1.0f, float h = 1.0f,
                          const char* label = nullptr) {
            if(const auto* key = keyByName(name)) {
                out.push_back({key->ledId, key->name,
                               label ? label : key->legend,
                               x, y, w, h, false});
            }
        };

        // Reserve a narrow chassis margin at both sides and a small band above
        // the keyboard for the accent LEDs shown in the supplied product image.
        constexpr float xShift = 0.28f;
        constexpr float yShift = 0.32f;

        // Top light bar: 45 separately addressable LEDs, left -> right,
        // spanning the full chassis width in the supplied ANSI reference.
        constexpr float topX = 0.0f;
        constexpr float topY = 0.0f;
        constexpr float topW = sourceWidth;
        constexpr float topH = 6.0f * sourceHeight / kLayoutHeight;
        constexpr float topSegmentW = topW / static_cast<float>(kTopBarCount);
        for(std::size_t i = 0; i < kTopBarCount; ++i) {
            const std::uint16_t id = static_cast<std::uint16_t>(kTopBarFirst + i);
            char name[16];
            std::snprintf(name, sizeof(name), "TOPBAR_%02zu", i);
            if(const auto* key = keyByName(name)) {
                out.push_back({id, key->name, "",
                               topX + static_cast<float>(i) * topSegmentW,
                               topY,
                               topSegmentW,
                               topH,
                               false});
            }
        }

        // The physical 3D Media Wheel is visibly larger than a keycap. Its
        // mute mark is drawn as vector artwork by KeyboardWidget.
        out.push_back({k3DMediaWheelLed, kMediaWheelName, "",
                       0.00f + xShift, 0.00f + yShift,
                       1.32f, 1.32f, true});

        // Macro column.
        add("M1", 0.12f + xShift, 1.52f + yShift);
        add("M2", 0.12f + xShift, 2.62f + yShift);
        add("M3", 0.12f + xShift, 3.72f + yShift);
        add("M4", 0.12f + xShift, 4.82f + yShift);
        add("M5", 0.12f + xShift, 5.92f + yShift);

        // Function row: F1 starts two units after Escape. The three groups
        // have half-unit gaps; F12 ends at the main typing block's right edge.
        add("ESC",   1.55f + xShift, 0.12f + yShift);
        add("F1",    3.55f + xShift, 0.12f + yShift); add("F2", 4.55f + xShift, 0.12f + yShift);
        add("F3",    5.55f + xShift, 0.12f + yShift); add("F4", 6.55f + xShift, 0.12f + yShift);
        add("F5",    8.05f + xShift, 0.12f + yShift); add("F6", 9.05f + xShift, 0.12f + yShift);
        add("F7",   10.05f + xShift, 0.12f + yShift); add("F8",11.05f + xShift, 0.12f + yShift);
        add("F9",   12.55f + xShift, 0.12f + yShift); add("F10",13.55f + xShift,0.12f + yShift);
        add("F11",  14.55f + xShift, 0.12f + yShift); add("F12",15.55f + xShift,0.12f + yShift);
        add("PRINT", 16.90f + xShift, 0.12f + yShift);
        add("SCRLK", 17.90f + xShift, 0.12f + yShift);
        add("PAUSE", 18.90f + xShift, 0.12f + yShift);

        // Source row coordinates; converted to the standard 19.05 mm pitch
        // below. Navigation and numpad blocks use the same pitch.
        constexpr float y1 = 1.52f + yShift;
        constexpr float y2 = 2.62f + yShift;
        constexpr float y3 = 3.72f + yShift;
        constexpr float y4 = 4.82f + yShift;
        constexpr float y5 = 5.92f + yShift;

        float x = 1.55f + xShift;
        add("GRAVE", x,y1); x += 1.0f;
        for(const char* n : {"1","2","3","4","5","6","7","8","9","0","MINUS","EQUAL"}) {
            add(n,x,y1); x += 1.0f;
        }
        add("BACKSPACE",x,y1,2.0f);

        // Standard six-key navigation cluster.
        add("INS", 16.90f + xShift,y1);
        add("HOME",17.90f + xShift,y1);
        add("PGUP",18.90f + xShift,y1);

        // Standard four-column numpad. + and Enter each span exactly two row
        // pitches so their edges line up with neighbouring keys.
        add("NUMLOCK",20.20f + xShift,y1);
        add("NUMSLASH",21.20f + xShift,y1);
        add("NUMSTAR",22.20f + xShift,y1);
        add("NUMMINUS",23.20f + xShift,y1);

        x = 1.55f + xShift;
        add("TAB",x,y2,1.5f); x += 1.5f;
        for(const char* n : {"Q","W","E","R","T","Y","U","I","O","P","LBRACKET","RBRACKET"}) {
            add(n,x,y2); x += 1.0f;
        }
        add("BACKSLASH",x,y2,1.5f);
        add("DEL", 16.90f + xShift,y2);
        add("END", 17.90f + xShift,y2);
        add("PGDN",18.90f + xShift,y2);
        add("NUM7",20.20f + xShift,y2);
        add("NUM8",21.20f + xShift,y2);
        add("NUM9",22.20f + xShift,y2);
        add("NUMPLUS",23.20f + xShift,y2,1.0f,2.10f);

        x = 1.55f + xShift;
        add("CAPS",x,y3,1.75f); x += 1.75f;
        for(const char* n : {"A","S","D","F","G","H","J","K","L","SEMICOLON","APOSTROPHE"}) {
            add(n,x,y3); x += 1.0f;
        }
        add("ENTER",x,y3,2.25f);
        add("NUM4",20.20f + xShift,y3);
        add("NUM5",21.20f + xShift,y3);
        add("NUM6",22.20f + xShift,y3);

        x = 1.55f + xShift;
        add("LSHIFT",x,y4,2.25f); x += 2.25f;
        for(const char* n : {"Z","X","C","V","B","N","M","COMMA","DOT","SLASH"}) {
            add(n,x,y4); x += 1.0f;
        }
        add("RSHIFT",x,y4,2.75f);

        // Conventional inverted-T cursor block.
        add("UP",17.90f + xShift,y4);
        add("NUM1",20.20f + xShift,y4);
        add("NUM2",21.20f + xShift,y4);
        add("NUM3",22.20f + xShift,y4);
        add("NUMENTER",23.20f + xShift,y4,1.0f,2.10f);

        x = 1.55f + xShift;
        add("LCTRL",x,y5,1.25f); x += 1.25f;
        add("LWIN", x,y5,1.25f); x += 1.25f;
        add("LALT", x,y5,1.25f); x += 1.25f;
        add("SPACE",x,y5,6.25f); x += 6.25f;
        add("RALT", x,y5,1.25f); x += 1.25f;
        add("RWIN", x,y5,1.25f); x += 1.25f;
        add("FN",   x,y5,1.25f); x += 1.25f;
        add("RCTRL",x,y5,1.25f);
        add("LEFT", 16.90f + xShift,y5);
        add("DOWN", 17.90f + xShift,y5);
        add("RIGHT",18.90f + xShift,y5);
        add("NUM0",20.20f + xShift,y5,2.0f);
        add("NUMDOT",22.20f + xShift,y5);

        // Side strips: five individually addressable LEDs on each side,
        // centred alongside the home row. Source coordinates preserve the
        // segment order; the user supplied a total length of 65 mm per strip.
        constexpr float sideTop = y2;
        constexpr float sideHeight = (y4 + 1.0f) - y2;
        constexpr float sideSegmentH = sideHeight / 5.0f;
        constexpr float sideW = 0.14f;
        constexpr float leftX = 0.02f;
        constexpr float rightX = sourceWidth - sideW - 0.02f;

        for(std::size_t i = 0; i < 5; ++i) {
            char leftName[20];
            std::snprintf(leftName, sizeof(leftName), "LEFT_STRIP_%zu", i);
            if(const auto* key = keyByName(leftName)) {
                out.push_back({key->ledId, key->name, "",
                               leftX,
                               sideTop + static_cast<float>(i) * sideSegmentH,
                               sideW,
                               sideSegmentH,
                               false});
            }
        }

        // The validated right-strip LED order runs bottom -> top, so place ID
        // 163 at the bottom and ID 167 at the top while preserving the physical
        // LED numbering.
        for(std::size_t i = 0; i < 5; ++i) {
            char rightName[20];
            std::snprintf(rightName, sizeof(rightName), "RIGHT_STRIP_%zu", i);
            if(const auto* key = keyByName(rightName)) {
                const std::size_t visualIndex = 4 - i;
                out.push_back({key->ledId, key->name, "",
                               rightX,
                               sideTop + static_cast<float>(visualIndex) * sideSegmentH,
                               sideW,
                               sideSegmentH,
                               false});
            }
        }

        // Keep drawing and flag presets in one physical coordinate system.
        // Fit standard key units within the chassis, preserving ANSI widths.
        // Conventional full-size layout: a quarter-unit gap on either side
        // of the navigation block. A quarter-unit gap below the function row
        // leaves room for the bottom chassis border without changing key pitch.
        for(auto& key : out) {
            const std::string_view name(key.name);
            if(key.round) {
                // Wheel centre is the intersection of the macro-column and
                // function-row centre lines, as confirmed on the hardware.
                key.x = kMacroColumnXmm + (kKeyUnitMm - kMediaWheelDiameterMm) / 2.0f;
                key.y = kFunctionRowYmm + (kKeyUnitMm - kMediaWheelDiameterMm) / 2.0f;
                key.w = key.h = kMediaWheelDiameterMm;
                continue;
            }
            if(name.substr(0, 7) != "TOPBAR_" && name.substr(0, 11) != "LEFT_STRIP_" &&
               name.substr(0, 12) != "RIGHT_STRIP_") {
                if(key.x < 1.55f + xShift) {
                    key.x = kMacroColumnXmm;
                } else if(key.x >= 20.20f + xShift) {
                    key.x = kMainBlockXmm +
                        (kNumpadOffsetU + key.x - (20.20f + xShift)) * kKeyUnitMm;
                } else if(key.x >= 16.90f + xShift) {
                    key.x = kMainBlockXmm +
                        (kNavigationOffsetU + key.x - (16.90f + xShift)) * kKeyUnitMm;
                } else {
                    key.x = kMainBlockXmm + (key.x - (1.55f + xShift)) * kKeyUnitMm;
                }
                key.y = key.y < y1 ? kFunctionRowYmm : kFunctionRowYmm +
                    (kTypingRowOffsetU + (key.y - y1) / 1.10f) * kKeyUnitMm;
                key.w *= kKeyUnitMm;
                key.h = (key.h > 1.0f ? 2.0f : 1.0f) * kKeyUnitMm;
                continue;
            }
            key.x *= kLayoutWidth / sourceWidth;
            key.w *= kLayoutWidth / sourceWidth;
            key.y *= kLayoutHeight / sourceHeight;
            key.h *= kLayoutHeight / sourceHeight;
            if(name.substr(0, 11) == "LEFT_STRIP_" || name.substr(0, 12) == "RIGHT_STRIP_") {
                // The strips sit underneath the chassis. Show them outside
                // its outline so they remain visible and selectable from above.
                key.x = name.substr(0, 11) == "LEFT_STRIP_" ? -kSideDisplayMarginMm :
                    kLayoutWidth + kSideStripDisplayGapMm;
                key.w = kSideStripWidthMm;
                constexpr float sideDisplayHeight = kSideStripLengthMm;
                constexpr float sideDisplaySegmentH = sideDisplayHeight / 5.0f;
                key.y = kFunctionRowYmm + (kTypingRowOffsetU + 2.5f) * kKeyUnitMm -
                    sideDisplayHeight / 2.0f +
                    (key.y - sideTop * kLayoutHeight / sourceHeight) /
                    (sideHeight * kLayoutHeight / sourceHeight) * sideDisplayHeight;
                key.h = sideDisplaySegmentH;
            }
        }
        return out;
    }();
    return data;
}

} // namespace krgb::lightmount::ansi_visual
