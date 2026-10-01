// Logitech G810 ISO105 visual layout.
//
// Geometry is transcribed from Logitech Gaming Software
// G810_PIDC331_INTL.xml. The source uses a 1032x380-ish coordinate space and
// describes each keyboard group as a rectangle containing weighted rows.
// This header preserves those group/row proportions while normalizing the
// visible extent to (0,0). It contains exactly the 116 physical lighting
// elements verified on the PID c331 G810: 105 keyboard keys, 5 media keys,
// 1 logo and 5 indicator/control lights.
#pragma once

#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

#include "core/logitech_g610_g810_keymap.h"

namespace krgb::logitech::g810_iso105_visual {

struct VisualElement {
    std::uint16_t keyType;
    std::uint8_t keyId;
    float x;
    float y;
    float w;
    float h;
};

inline constexpr float kSourceMinX = 12.0f;
inline constexpr float kSourceMinY = 41.0f;
inline constexpr float kLayoutWidth = 1020.0f;
inline constexpr float kLayoutHeight = 339.0f;
inline constexpr std::size_t kExpectedElementCount = 116;

struct Cell {
    std::uint8_t keyId;
    float width = 1.0f;
    bool down = false;
};

inline const std::vector<VisualElement>& elements() {
    static const std::vector<VisualElement> data = [] {
        std::vector<VisualElement> out;
        out.reserve(kExpectedElementCount);

        auto addRow =
            [&out](std::uint16_t keyType,
                   float x, float y, float width, float rowHeight,
                   std::initializer_list<Cell> cells) {
                float totalWeight = 0.0f;
                for(const auto& cell : cells) {
                    totalWeight += cell.width;
                }

                float cursor = x;
                for(const auto& cell : cells) {
                    const float cellWidth =
                        totalWeight > 0.0f ? width * cell.width / totalWeight : 0.0f;
                    if(cell.keyId != 0) {
                        out.push_back({
                            keyType,
                            cell.keyId,
                            cursor - kSourceMinX,
                            y - kSourceMinY,
                            cellWidth,
                            rowHeight * (cell.down ? 2.0f : 1.0f),
                        });
                    }
                    cursor += cellWidth;
                }
            };

        // Logo.
        addRow(g610_g810::kLogoKeyType, 12.0f, 41.0f, 46.0f, 46.0f,
               {{0x01}});

        // Lock indicators.
        addRow(g610_g810::kIndicatorKeyType, 521.0f, 56.0f, 110.0f, 18.0f,
               {{0x00, 0.10f}, {0x05, 0.20f}, {0x00, 0.80f},
                {0x03, 0.20f}, {0x00, 0.80f}, {0x04, 0.20f},
                {0x00, 0.10f}});

        // Game-mode and lighting controls.
        addRow(g610_g810::kIndicatorKeyType, 710.0f, 48.0f, 120.0f, 35.0f,
               {{0x02}, {0x00, 1.54f}, {0x01}});

        // Media controls.
        addRow(g610_g810::kMediaKeyType, 858.0f, 48.0f, 35.0f, 35.0f,
               {{0xe2}});
        addRow(g610_g810::kMediaKeyType, 858.0f, 103.0f, 173.0f, 35.0f,
               {{0xcd}, {0x00, 0.38f}, {0xb7}, {0x00, 0.38f},
                {0xb6}, {0x00, 0.38f}, {0xb5}});

        // Function row.
        addRow(g610_g810::kKeyboardKeyType, 13.0f, 96.0f, 46.0f, 46.0f,
               {{0x29}});
        addRow(g610_g810::kKeyboardKeyType, 89.0f, 96.0f, 184.0f, 46.0f,
               {{0x3a}, {0x3b}, {0x3c}, {0x3d}});
        addRow(g610_g810::kKeyboardKeyType, 299.0f, 96.0f, 184.0f, 46.0f,
               {{0x3e}, {0x3f}, {0x40}, {0x41}});
        addRow(g610_g810::kKeyboardKeyType, 507.0f, 96.0f, 184.0f, 46.0f,
               {{0x42}, {0x43}, {0x44}, {0x45}});
        addRow(g610_g810::kKeyboardKeyType, 702.0f, 96.0f, 133.0f, 46.0f,
               {{0x46}, {0x47}, {0x48}});

        // Main ISO105 block (five equal-height rows).
        constexpr float mainRowH = 228.0f / 5.0f;
        addRow(g610_g810::kKeyboardKeyType, 13.0f, 152.0f,
               678.0f, mainRowH,
               {{0x35}, {0x1e}, {0x1f}, {0x20}, {0x21}, {0x22}, {0x23},
                {0x24}, {0x25}, {0x26}, {0x27}, {0x2d}, {0x2e},
                {0x2a, 1.50f}});
        addRow(g610_g810::kKeyboardKeyType, 13.0f, 152.0f + mainRowH,
               678.0f, mainRowH,
               {{0x2b, 1.30f}, {0x14}, {0x1a}, {0x08}, {0x15}, {0x17},
                {0x1c}, {0x18}, {0x0c}, {0x12}, {0x13}, {0x2f},
                {0x30, 1.30f}, {0x28, 1.25f, true}});
        addRow(g610_g810::kKeyboardKeyType, 13.0f, 152.0f + 2.0f * mainRowH,
               678.0f, mainRowH,
               {{0x39, 1.50f}, {0x04}, {0x16}, {0x07}, {0x09}, {0x0a},
                {0x0b}, {0x0d}, {0x0e}, {0x0f}, {0x33}, {0x34},
                {0x32, 1.08f}, {0x00, 1.17f}});
        addRow(g610_g810::kKeyboardKeyType, 13.0f, 152.0f + 3.0f * mainRowH,
               678.0f, mainRowH,
               {{0xe1, 1.25f}, {0x64}, {0x1d}, {0x1b}, {0x06}, {0x19},
                {0x05}, {0x11}, {0x10}, {0x36}, {0x37}, {0x38},
                {0xe5, 2.50f}});
        addRow(g610_g810::kKeyboardKeyType, 13.0f, 152.0f + 4.0f * mainRowH,
               678.0f, mainRowH,
               {{0xe0, 1.50f}, {0xe3, 1.25f}, {0xe2, 1.25f},
                {0x2c, 5.75f}, {0xe6, 1.25f}, {0xe7, 1.25f},
                {0x65, 1.25f}, {0xe4, 1.50f}});

        // Navigation, arrows and numpad (five equal-height rows).
        constexpr float numRowH = 228.0f / 5.0f;
        addRow(g610_g810::kKeyboardKeyType, 702.0f, 152.0f,
               330.0f, numRowH,
               {{0x49}, {0x4a}, {0x4b}, {0x00, 0.50f},
                {0x53}, {0x54}, {0x55}, {0x56}});
        addRow(g610_g810::kKeyboardKeyType, 702.0f, 152.0f + numRowH,
               330.0f, numRowH,
               {{0x4c}, {0x4d}, {0x4e}, {0x00, 0.50f},
                {0x5f}, {0x60}, {0x61}, {0x57, 1.0f, true}});
        addRow(g610_g810::kKeyboardKeyType, 702.0f, 152.0f + 2.0f * numRowH,
               330.0f, numRowH,
               {{0x00}, {0x00}, {0x00}, {0x00, 0.50f},
                {0x5c}, {0x5d}, {0x5e}, {0x00}});
        addRow(g610_g810::kKeyboardKeyType, 702.0f, 152.0f + 3.0f * numRowH,
               330.0f, numRowH,
               {{0x00}, {0x52}, {0x00}, {0x00, 0.50f},
                {0x59}, {0x5a}, {0x5b}, {0x58, 1.0f, true}});
        addRow(g610_g810::kKeyboardKeyType, 702.0f, 152.0f + 4.0f * numRowH,
               330.0f, numRowH,
               {{0x50}, {0x51}, {0x4f}, {0x00, 0.50f},
                {0x62, 2.0f}, {0x63}, {0x00}});

        return out;
    }();

    return data;
}

inline const g610_g810::LightingElement* definition(
    const VisualElement& element) {
    return g610_g810::findPhysical(element.keyType, element.keyId);
}

inline const VisualElement* findByName(std::string_view name) {
    for(const auto& element : elements()) {
        const auto* def = definition(element);
        if(def && g610_g810::asciiIEquals(def->name, name)) {
            return &element;
        }
    }
    return nullptr;
}

} // namespace krgb::logitech::g810_iso105_visual
