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
    enum Kind { Solid, Rainbow, Effect, PerKey };

    Kind   kind       = Solid;
    int    effectMode = 0;  // krgb::Mode value, used when kind == Effect
    QColor color      = QColor(0, 170, 255);
    int    speed      = static_cast<int>(krgb::Speed::Normal);
    int    direction  = static_cast<int>(krgb::Direction::Left);
    int    brightness = 100;

    // Per-key colours, keyed by key name (see keymap.h). Used when kind == PerKey.
    // Keys absent from the map are treated as off (black).
    QHash<QString, QColor> keyColors;

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
