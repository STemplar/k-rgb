// LightingSettings — the persisted lighting choice for one profile
// (KConfig-backed). Shared by the GUI (save on apply / load on start) and the
// headless `krgb --apply` restore path used by the autostart entry.
//
// Profiles namespace manages the set of named profiles and which one is active.
#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>

#include "core/aw410k_device.h"

class KeyboardController;
class CaseController;

struct LightingSettings {
    enum Kind { Solid, Rainbow, Effect, PerKey, Zones };

    Kind   kind       = Solid;
    int    effectMode = 0;
    QColor color      = QColor(0, 170, 255);
    QColor secondaryColor = QColor(255, 255, 255);
    // Protocol-specific colour variant for effects that support it. For the
    // be quiet! Mount protocol this is BeQuietMountColorMode (Single/Dual/Gradient).
    int    effectColorMode = 0;
    int    speed      = static_cast<int>(krgb::Speed::Normal);
    int    direction  = static_cast<int>(krgb::Direction::Left);
    // Exact firmware effect period for devices such as the G810. Zero keeps
    // compatibility with older profiles and derives the period from speed.
    int    effectPeriodMs = 0;
    int    brightness = 100;

    // Per-key colours, keyed by key name (see keymap.h). Used when kind == PerKey.
    // Keys absent from the map are treated as off (black).
    QHash<QString, QColor> keyColors;

    // Keyboard zone colours for HID++ zone-based keyboards such as G213.
    // Keys are zero-based logical zone indices reported by the device.
    QHash<int, QColor> keyboardZoneColors;

    // Case / chassis lighting (AW-ELC controller). caseSet means this profile
    // manages the case. casePerZone selects per-zone colours (caseZoneColors,
    // keyed by zone index) over the whole-case caseColor.
    bool               caseSet     = false;
    bool               casePerZone = false;
    QColor             caseColor   = QColor(0, 90, 255);
    QHash<int, QColor> caseZoneColors;

    // Persist to / load from the named profile's KConfig group.
    void               save(const QString& profile) const;
    static LightingSettings load(const QString& profile);

    // Apply keyboard lighting, and (if caseSet and caseController given) the case.
    bool apply(KeyboardController& controller, CaseController* caseController = nullptr) const;
};

// Named-profile registry. Profiles are stored as "Profile <name>" groups in the
// app's KConfig; the active profile is remembered across sessions.
namespace Profiles {

inline const QString DefaultName = QStringLiteral("Default");

QStringList names();                 // all profiles (guarantees Default exists)
QString     current();               // active profile name
void        setCurrent(const QString& name);
bool        exists(const QString& name);
void        add(const QString& name, const LightingSettings& seed);
void        remove(const QString& name);
void        rename(const QString& from, const QString& to);

} // namespace Profiles
