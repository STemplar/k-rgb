#include "settings.h"

#include "casecontroller.h"
#include "keyboardcontroller.h"

#include <KConfigGroup>
#include <KSharedConfig>

namespace {

QString groupName(const QString& profile) {
    return QStringLiteral("Profile ") + profile;
}

// Serialise the per-key map as "NAME=#rrggbb" entries (stable, human-readable).
QStringList encodeKeyColors(const QHash<QString, QColor>& keyColors) {
    QStringList out;
    out.reserve(keyColors.size());
    for(auto it = keyColors.cbegin(); it != keyColors.cend(); ++it) {
        out << it.key() + QLatin1Char('=') + it.value().name();
    }
    return out;
}

QHash<QString, QColor> decodeKeyColors(const QStringList& entries) {
    QHash<QString, QColor> map;
    for(const QString& e : entries) {
        const int eq = e.indexOf(QLatin1Char('='));
        if(eq <= 0) {
            continue;
        }
        const QColor c(e.mid(eq + 1));
        if(c.isValid()) {
            map.insert(e.left(eq), c);
        }
    }
    return map;
}

QStringList encodeZoneColors(const QHash<int, QColor>& zoneColors) {
    QStringList out;
    out.reserve(zoneColors.size());
    for(auto it = zoneColors.cbegin(); it != zoneColors.cend(); ++it) {
        out << QString::number(it.key()) + QLatin1Char('=') + it.value().name();
    }
    return out;
}

QHash<int, QColor> decodeZoneColors(const QStringList& entries) {
    QHash<int, QColor> map;
    for(const QString& e : entries) {
        const int eq = e.indexOf(QLatin1Char('='));
        if(eq <= 0) {
            continue;
        }
        bool ok = false;
        const int idx = e.left(eq).toInt(&ok);
        const QColor c(e.mid(eq + 1));
        if(ok && c.isValid()) {
            map.insert(idx, c);
        }
    }
    return map;
}

void writeInto(KConfigGroup& g, const LightingSettings& s) {
    g.writeEntry("kind", static_cast<int>(s.kind));
    g.writeEntry("effectMode", s.effectMode);
    g.writeEntry("color", s.color);
    g.writeEntry("speed", s.speed);
    g.writeEntry("direction", s.direction);
    g.writeEntry("effectPeriodMs", s.effectPeriodMs);
    g.writeEntry("brightness", s.brightness);
    g.writeEntry("keyColors", encodeKeyColors(s.keyColors));
    g.writeEntry("keyboardZoneColors", encodeZoneColors(s.keyboardZoneColors));
    g.writeEntry("caseSet", s.caseSet);
    g.writeEntry("casePerZone", s.casePerZone);
    g.writeEntry("caseColor", s.caseColor);
    g.writeEntry("caseZoneColors", encodeZoneColors(s.caseZoneColors));
}

LightingSettings readFrom(const KConfigGroup& g) {
    LightingSettings s;
    s.kind       = static_cast<LightingSettings::Kind>(g.readEntry("kind", static_cast<int>(s.kind)));
    s.effectMode = g.readEntry("effectMode", s.effectMode);
    s.color      = g.readEntry("color", s.color);
    s.speed      = g.readEntry("speed", s.speed);
    s.direction  = g.readEntry("direction", s.direction);
    s.effectPeriodMs = g.readEntry("effectPeriodMs", s.effectPeriodMs);
    s.brightness = g.readEntry("brightness", s.brightness);
    s.keyColors  = decodeKeyColors(g.readEntry("keyColors", QStringList()));
    s.keyboardZoneColors =
        decodeZoneColors(g.readEntry("keyboardZoneColors", QStringList()));
    s.caseSet      = g.readEntry("caseSet", s.caseSet);
    s.casePerZone  = g.readEntry("casePerZone", s.casePerZone);
    s.caseColor    = g.readEntry("caseColor", s.caseColor);
    s.caseZoneColors = decodeZoneColors(g.readEntry("caseZoneColors", QStringList()));
    return s;
}

// On first run, seed the profile registry — migrating any pre-profiles
// [Lighting] group into a "Default" profile so existing users keep their setup.
void ensureInitialized() {
    auto cfg = KSharedConfig::openConfig();
    KConfigGroup reg(cfg, QStringLiteral("Profiles"));
    if(!reg.readEntry("names", QStringList()).isEmpty()) {
        return;
    }

    KConfigGroup def(cfg, groupName(Profiles::DefaultName));
    const KConfigGroup legacy(cfg, QStringLiteral("Lighting"));
    if(legacy.exists()) {
        writeInto(def, readFrom(legacy));
    } else {
        writeInto(def, LightingSettings{});  // factory default (solid blue)
    }

    reg.writeEntry("names", QStringList{Profiles::DefaultName});
    reg.writeEntry("current", Profiles::DefaultName);
    cfg->sync();
}

} // namespace

void LightingSettings::save(const QString& profile) const {
    ensureInitialized();
    auto cfg = KSharedConfig::openConfig();
    KConfigGroup g(cfg, groupName(profile));
    writeInto(g, *this);

    // Make sure the profile is registered.
    KConfigGroup reg(cfg, QStringLiteral("Profiles"));
    QStringList all = reg.readEntry("names", QStringList());
    if(!all.contains(profile)) {
        all << profile;
        reg.writeEntry("names", all);
    }
    cfg->sync();
}

LightingSettings LightingSettings::load(const QString& profile) {
    ensureInitialized();
    const KConfigGroup g(KSharedConfig::openConfig(), groupName(profile));
    return readFrom(g);
}

bool LightingSettings::apply(KeyboardController& controller, CaseController* caseController) const {
    if(caseSet && caseController && caseController->isAvailable()) {
        if(casePerZone) {
            caseController->applyZones(caseZoneColors);
        } else {
            caseController->applySolid(caseColor);  // whole-case colour (applied as-is)
        }
    }
    switch(kind) {
        case Solid:   return controller.applySolid(color, brightness);
        case Rainbow: return controller.applyRainbow(brightness);
        case Effect:  return controller.applyEffect(effectMode, speed, direction, color, brightness,
                                                     effectPeriodMs);
        case PerKey:  return controller.applyPerKey(keyColors, brightness);
        case Zones:    return controller.applyZones(keyboardZoneColors, brightness);
    }
    return false;
}

namespace Profiles {

QStringList names() {
    ensureInitialized();
    const KConfigGroup reg(KSharedConfig::openConfig(), QStringLiteral("Profiles"));
    return reg.readEntry("names", QStringList{DefaultName});
}

QString current() {
    ensureInitialized();
    const KConfigGroup reg(KSharedConfig::openConfig(), QStringLiteral("Profiles"));
    const QString c = reg.readEntry("current", DefaultName);
    return names().contains(c) ? c : DefaultName;
}

void setCurrent(const QString& name) {
    ensureInitialized();
    auto cfg = KSharedConfig::openConfig();
    KConfigGroup reg(cfg, QStringLiteral("Profiles"));
    reg.writeEntry("current", name);
    cfg->sync();
}

bool exists(const QString& name) {
    return names().contains(name);
}

void add(const QString& name, const LightingSettings& seed) {
    if(name.isEmpty() || exists(name)) {
        return;
    }
    seed.save(name);  // save() registers the profile
}

void remove(const QString& name) {
    ensureInitialized();
    auto cfg = KSharedConfig::openConfig();
    KConfigGroup reg(cfg, QStringLiteral("Profiles"));
    QStringList all = reg.readEntry("names", QStringList());
    all.removeAll(name);
    if(all.isEmpty()) {
        // Never leave zero profiles; recreate a fresh Default.
        KConfigGroup def(cfg, groupName(DefaultName));
        writeInto(def, LightingSettings{});
        all << DefaultName;
    }
    reg.writeEntry("names", all);
    if(reg.readEntry("current", DefaultName) == name) {
        reg.writeEntry("current", all.first());
    }
    cfg->deleteGroup(groupName(name));
    cfg->sync();
}

void rename(const QString& from, const QString& to) {
    if(from == to || to.isEmpty() || !exists(from) || exists(to)) {
        return;
    }
    auto cfg = KSharedConfig::openConfig();
    // Copy the group's contents to the new name, then drop the old group.
    LightingSettings::load(from).save(to);
    cfg->deleteGroup(groupName(from));

    KConfigGroup reg(cfg, QStringLiteral("Profiles"));
    QStringList all = reg.readEntry("names", QStringList());
    all.removeAll(from);
    reg.writeEntry("names", all);  // save(to) already appended `to`
    if(reg.readEntry("current", DefaultName) == from) {
        reg.writeEntry("current", to);
    }
    cfg->sync();
}

} // namespace Profiles
