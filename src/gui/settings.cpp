#include "settings.h"

#include "casecontroller.h"
#include "keyboardcontroller.h"
#include "core/effect_code.h"
#include "core/lightmount_effects.h"

#include <KConfigGroup>
#include <KSharedConfig>

namespace {

QString groupName(const QString& profile) {
    return QStringLiteral("Profile ") + profile;
}

QStringList encodeKeyColors(const QHash<QString, QColor>& colors) {
    QStringList out;
    out.reserve(colors.size());
    for(auto it = colors.cbegin(); it != colors.cend(); ++it) {
        out << it.key() + QLatin1Char('=') + it.value().name();
    }
    return out;
}

QHash<QString, QColor> decodeKeyColors(const QStringList& entries) {
    QHash<QString, QColor> out;
    for(const QString& entry : entries) {
        const int eq = entry.indexOf(QLatin1Char('='));
        if(eq <= 0) continue;
        const QColor color(entry.mid(eq + 1));
        if(color.isValid()) out.insert(entry.left(eq), color);
    }
    return out;
}

QStringList encodeZoneColors(const QHash<int, QColor>& colors) {
    QStringList out;
    out.reserve(colors.size());
    for(auto it = colors.cbegin(); it != colors.cend(); ++it) {
        out << QString::number(it.key()) + QLatin1Char('=') + it.value().name();
    }
    return out;
}

QHash<int, QColor> decodeZoneColors(const QStringList& entries) {
    QHash<int, QColor> out;
    for(const QString& entry : entries) {
        const int eq = entry.indexOf(QLatin1Char('='));
        if(eq <= 0) continue;
        bool ok = false;
        const int index = entry.left(eq).toInt(&ok);
        const QColor color(entry.mid(eq + 1));
        if(ok && color.isValid()) out.insert(index, color);
    }
    return out;
}

void writeInto(KConfigGroup& g, const LightingSettings& s) {
    g.writeEntry("kind", static_cast<int>(s.kind));
    g.writeEntry("effectMode", s.effectMode);
    g.writeEntry("color", s.color);
    g.writeEntry("secondaryColor", s.secondaryColor);
    g.writeEntry("effectColorMode", s.effectColorMode);
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
    s.kind = static_cast<LightingSettings::Kind>(
        g.readEntry("kind", static_cast<int>(s.kind)));
    s.effectMode = g.readEntry("effectMode", s.effectMode);
    s.color = g.readEntry("color", s.color);
    s.secondaryColor = g.readEntry("secondaryColor", s.secondaryColor);
    s.effectColorMode = g.readEntry("effectColorMode", s.effectColorMode);
    s.speed = g.readEntry("speed", s.speed);
    s.direction = g.readEntry("direction", s.direction);
    s.effectPeriodMs = g.readEntry("effectPeriodMs", s.effectPeriodMs);
    s.brightness = g.readEntry("brightness", s.brightness);
    s.keyColors = decodeKeyColors(g.readEntry("keyColors", QStringList()));
    s.keyboardZoneColors = decodeZoneColors(
        g.readEntry("keyboardZoneColors", QStringList()));
    s.caseSet = g.readEntry("caseSet", s.caseSet);
    s.casePerZone = g.readEntry("casePerZone", s.casePerZone);
    s.caseColor = g.readEntry("caseColor", s.caseColor);
    s.caseZoneColors = decodeZoneColors(
        g.readEntry("caseZoneColors", QStringList()));
    return s;
}

bool applyBeQuietEffect(const LightingSettings& s, KeyboardController& controller) {
    const auto native = static_cast<krgb::LightMountEffect>(
        krgb::nativeEffectValue(s.effectMode));
    const krgb::LightMountColor primary{
        static_cast<std::uint8_t>(s.color.red()),
        static_cast<std::uint8_t>(s.color.green()),
        static_cast<std::uint8_t>(s.color.blue())};
    const krgb::LightMountColor secondary{
        static_cast<std::uint8_t>(s.secondaryColor.red()),
        static_cast<std::uint8_t>(s.secondaryColor.green()),
        static_cast<std::uint8_t>(s.secondaryColor.blue())};
    const auto colorMode = static_cast<krgb::LightMountColorMode>(s.effectColorMode);
    const auto brightness = static_cast<std::uint8_t>(qBound(10, s.brightness, 100));
    const auto speed = static_cast<std::uint8_t>(qBound(10, s.speed, 100));
    auto direction = static_cast<krgb::LightMountDirection>(s.direction);

    krgb::LightMountGeneralEffect effect;
    switch(native) {
        case krgb::LightMountEffect::Static:
            effect = krgb::lightmount::makeStaticEffect(primary, brightness);
            break;
        case krgb::LightMountEffect::ColorWave:
            if(direction > krgb::LightMountDirection::Right)
                direction = krgb::LightMountDirection::Right;
            if(colorMode == krgb::LightMountColorMode::Dual) {
                effect = krgb::lightmount::makeColorWaveDualEffect(
                    direction, brightness, speed, primary, secondary);
            } else if(colorMode == krgb::LightMountColorMode::Gradient) {
                effect = krgb::lightmount::makeColorWaveGradientEffect(
                    direction, brightness, speed,
                    krgb::lightmount::defaultRainbowGradient());
            } else {
                effect = krgb::lightmount::makeColorWaveSingleEffect(
                    direction, brightness, speed, primary);
            }
            break;
        case krgb::LightMountEffect::Tornado:
            if(direction != krgb::LightMountDirection::Clockwise &&
               direction != krgb::LightMountDirection::CounterClockwise)
                direction = krgb::LightMountDirection::Clockwise;
            effect = krgb::lightmount::makeTornadoEffect(
                direction, brightness, speed);
            break;
        case krgb::LightMountEffect::Breathing:
            effect = krgb::lightmount::makeBreathingEffect(brightness, speed);
            break;
        case krgb::LightMountEffect::Reactive:
            effect = krgb::lightmount::makeReactiveEffect(
                brightness, speed, primary, secondary);
            break;
        case krgb::LightMountEffect::Matrix:
            if(direction > krgb::LightMountDirection::Right)
                direction = krgb::LightMountDirection::Down;
            effect = krgb::lightmount::makeMatrixEffect(
                direction, brightness, speed);
            break;
        default:
            return false;
    }
    return controller.applyLightMountGeneralEffect(effect);
}

bool applyProtocolEffect(const LightingSettings& s, KeyboardController& controller) {
    if(!krgb::isProtocolEffectCode(s.effectMode)) return false;

    switch(krgb::protocolFromEffectCode(s.effectMode)) {
        case krgb::EffectProtocol::BeQuietMount:
            return applyBeQuietEffect(s, controller);

        case krgb::EffectProtocol::LogitechHIDPP2: {
            const auto id = krgb::nativeEffectValue(s.effectMode);
            if(id == 1) {
                return controller.applySolid(s.color, s.brightness);
            }
            return controller.applyLogitechColorLed8070Effect(
                id,
                s.color,
                s.effectPeriodMs > 0 ? s.effectPeriodMs : 5000,
                qBound(1, s.brightness, 100),
                qBound(0, s.direction, 8));
        }
    }
    return false;
}

void ensureInitialized() {
    auto cfg = KSharedConfig::openConfig();
    KConfigGroup reg(cfg, QStringLiteral("Profiles"));
    if(!reg.readEntry("names", QStringList()).isEmpty()) return;

    KConfigGroup def(cfg, groupName(Profiles::DefaultName));
    const KConfigGroup legacy(cfg, QStringLiteral("Lighting"));
    if(legacy.exists()) writeInto(def, readFrom(legacy));
    else writeInto(def, LightingSettings{});

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

bool LightingSettings::apply(KeyboardController& controller,
                             CaseController* caseController) const {
    if(caseSet && caseController && caseController->isAvailable()) {
        if(casePerZone) caseController->applyZones(caseZoneColors);
        else caseController->applySolid(caseColor);
    }

    switch(kind) {
        case Solid:
            return controller.applySolid(color, brightness);
        case Rainbow:
            return controller.applyRainbow(brightness);
        case Effect:
            if(krgb::isProtocolEffectCode(effectMode))
                return applyProtocolEffect(*this, controller);
            return controller.applyEffect(effectMode, speed, direction,
                                          color, brightness, effectPeriodMs);
        case PerKey:
            return controller.applyPerKey(keyColors, brightness);
        case Zones:
            return controller.applyZones(keyboardZoneColors, brightness);
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
    if(name.isEmpty() || exists(name)) return;
    seed.save(name);
}

void remove(const QString& name) {
    ensureInitialized();
    auto cfg = KSharedConfig::openConfig();
    KConfigGroup reg(cfg, QStringLiteral("Profiles"));
    QStringList all = reg.readEntry("names", QStringList());
    all.removeAll(name);
    if(all.isEmpty()) {
        KConfigGroup def(cfg, groupName(DefaultName));
        writeInto(def, LightingSettings{});
        all << DefaultName;
    }
    reg.writeEntry("names", all);
    if(reg.readEntry("current", DefaultName) == name)
        reg.writeEntry("current", all.first());
    cfg->deleteGroup(groupName(name));
    cfg->sync();
}

void rename(const QString& from, const QString& to) {
    if(from == to || to.isEmpty() || !exists(from) || exists(to)) return;

    auto cfg = KSharedConfig::openConfig();
    LightingSettings::load(from).save(to);
    cfg->deleteGroup(groupName(from));

    KConfigGroup reg(cfg, QStringLiteral("Profiles"));
    QStringList all = reg.readEntry("names", QStringList());
    all.removeAll(from);
    reg.writeEntry("names", all);
    if(reg.readEntry("current", DefaultName) == from)
        reg.writeEntry("current", to);
    cfg->sync();
}

} // namespace Profiles
