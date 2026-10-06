#include "keyboardcontroller.h"

#include "core/keymap.h"
#include "core/lightmount_effects.h"
#include "core/lightmount_keymap.h"
#include "core/logitech_g610_g810_keymap.h"
#include "core/logitech_g810_iso105_visual.h"

#include <QFileInfo>
#include <QSocketNotifier>
#include <QDateTime>
#include <QTimer>
#include <KLocalizedString>

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

using krgb::AW410KDevice;
using krgb::Mode;
using krgb::Speed;
using krgb::Direction;
using krgb::ColorMode;

namespace fs = std::filesystem;

namespace {

QColor scaled(const QColor& c, int pct) {
    pct = qBound(0, pct, 100);
    return QColor(c.red() * pct / 100, c.green() * pct / 100, c.blue() * pct / 100);
}

bool readSysfsHex(const fs::path& path, unsigned& value) {
    std::ifstream in(path);
    if(!in) {
        return false;
    }
    in >> std::hex >> value;
    return !in.fail();
}

std::uint8_t lightMountSpeed(int speedValue) {
    // The shared GUI currently exposes Slow/Normal/Fast. The Light Mount
    // protocol accepts 10..100; these values preserve the existing ordering
    // where a lower value means a faster animation.
    switch(static_cast<Speed>(speedValue)) {
        case Speed::Slowest: return 80;
        case Speed::Normal:  return 50;
        case Speed::Fastest: return 20;
    }
    return 50;
}

krgb::LightMountDirection lightMountDirection(int directionValue) {
    switch(static_cast<Direction>(directionValue)) {
        case Direction::Up:    return krgb::LightMountDirection::Up;
        case Direction::Down:  return krgb::LightMountDirection::Down;
        case Direction::Left:  return krgb::LightMountDirection::Left;
        case Direction::Right: return krgb::LightMountDirection::Right;
    }
    return krgb::LightMountDirection::Right;
}

} // namespace

KeyboardController::KeyboardController(QObject* parent)
    : QObject(parent) {
    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(2000);
    connect(pollTimer_, &QTimer::timeout, this, &KeyboardController::refresh);
    pollTimer_->start();
    refresh();
}

KeyboardController::~KeyboardController() {
    stopG810KeyPressEffect();
}

void KeyboardController::refresh() {
    const krgb::KeyboardModel* model = nullptr;
    const std::string alienwarePath = AW410KDevice::findDevice(&model);
    const std::string lightMountPath = krgb::LightMountDevice::findDevicePath();

    bool nowConnected = false;
    QString newPath;
    QString newModelName;
    quint8 newModelBit = krgb::kAllModels;
    Backend newBackend = Backend::None;
    bool newLogitechG810Iso105Visual = false;

    if(!alienwarePath.empty()) {
        nowConnected = true;
        newPath = QString::fromStdString(alienwarePath);
        newModelName = model ? QString::fromLatin1(model->name) : QString();
        newModelBit = model ? model->bit : krgb::kAllModels;
        newBackend = Backend::Alienware;
        if(lightMountDevice_.isOpen()) {
            lightMountDevice_.close();
        }
        if(logitechDevice_.isOpen()) {
            logitechDevice_.close();
        }
    } else if(!lightMountPath.empty()) {
        nowConnected = true;
        newPath = QString::fromStdString(lightMountPath);
        newModelName = QStringLiteral("be quiet! Light Mount");
        newBackend = Backend::LightMount;
        if(device_.isOpen()) {
            device_.close();
        }
        if(logitechDevice_.isOpen()) {
            logitechDevice_.close();
        }
    } else {
        // Keep an already-open Logitech endpoint while its hidraw node exists.
        // Otherwise rescan Logitech endpoints and validate them through HID++
        // feature discovery before presenting them to the GUI.
        bool logitechPresent =
            logitechDevice_.isOpen() &&
            QFileInfo::exists(QString::fromStdString(logitechDevice_.path()));
        if(!logitechPresent) {
            if(logitechDevice_.isOpen()) {
                logitechDevice_.close();
            }
            std::string err;
            logitechPresent = logitechDevice_.openKeyboard(&err);
        }

        if(logitechPresent) {
            nowConnected = true;
            newPath = QString::fromStdString(logitechDevice_.path());

            // Prefer the HID++ 0x0005 product name over the generic USB
            // product string.  The G810 for example exposes "G810 Orion
            // Spectrum" through HID++, while USB reports "Gaming Keyboard
            // G810".  Prefix it with the normalized manufacturer name for the
            // user-facing identity.
            QString manufacturer =
                QString::fromStdString(logitechDevice_.usbIdentity().manufacturer).trimmed();
            if(manufacturer.contains(QStringLiteral("Logitech"),
                                     Qt::CaseInsensitive)) {
                manufacturer = QStringLiteral("Logitech");
            }

            krgb::LogitechHIDPP20DeviceTypeInfo deviceIdentity;
            std::string identityErr;
            if(logitechDevice_.getDeviceTypeAndName(deviceIdentity, &identityErr) &&
               !deviceIdentity.name.empty()) {
                const QString productName =
                    QString::fromStdString(deviceIdentity.name).trimmed();
                if(manufacturer.isEmpty() ||
                   productName.startsWith(manufacturer, Qt::CaseInsensitive)) {
                    newModelName = productName;
                } else {
                    newModelName = manufacturer + QLatin1Char(' ') + productName;
                }
            } else {
                newModelName = QString::fromStdString(logitechDevice_.displayName());
            }

            newBackend = Backend::LogitechHIDPP20;
            if(device_.isOpen()) {
                device_.close();
            }
            if(lightMountDevice_.isOpen()) {
                lightMountDevice_.close();
            }
        }
    }

    int newLogitechZoneCount = 0;
    if(newBackend == Backend::LogitechHIDPP20 && logitechDevice_.isOpen()) {
        std::uint8_t zones = 0;
        std::string zoneErr;
        if(logitechDevice_.getColorLed8070ZoneCount(zones, &zoneErr)) {
            newLogitechZoneCount = static_cast<int>(zones);
        }

        const auto* known =
            krgb::logitechKnownDeviceForProductId(logitechDevice_.productId());
        if(known && known->model == krgb::LogitechKnownModel::G810OrionSpectrum) {
            krgb::LogitechHIDPP20KeyboardLayoutInfo layoutInfo;
            std::string layoutErr;
            if(logitechDevice_.getKeyboardLayout(layoutInfo, &layoutErr)) {
                const auto* geometry =
                    krgb::logitech::g610_g810::inferGeometryForLayout(
                        krgb::logitech::g610_g810::kModelG810,
                        layoutInfo.countryCode);
                newLogitechG810Iso105Visual =
                    geometry && std::string_view(geometry->name) == "ISO105";
            }
        }
    }

    if(nowConnected != connected_ || newPath != path_ ||
       newBackend != backend_ || newModelName != modelName_ ||
       newLogitechZoneCount != logitechZoneCount_ ||
       newLogitechG810Iso105Visual != logitechG810Iso105Visual_) {
        connected_ = nowConnected;
        path_ = newPath;
        modelName_ = newModelName;
        modelBit_ = newModelBit;
        backend_ = newBackend;
        logitechZoneCount_ = newLogitechZoneCount;
        logitechG810Iso105Visual_ = newLogitechG810Iso105Visual;

        if(!connected_) {
            if(device_.isOpen()) {
                device_.close();
            }
            if(lightMountDevice_.isOpen()) {
                lightMountDevice_.close();
            }
            if(logitechDevice_.isOpen()) {
                logitechDevice_.close();
            }
        }
        Q_EMIT connectionChanged(connected_, path_);
    }
}

namespace {
// Bit identifying the keys the currently-open Alienware device supports.
std::uint8_t activeBit(const krgb::AW410KDevice& dev) {
    return dev.model() ? dev.model()->bit : krgb::kAllModels;
}
} // namespace

bool KeyboardController::ensureOpen() {
    if(backend_ == Backend::LogitechHIDPP20) {
        if(logitechDevice_.isOpen()) {
            return true;
        }
        std::string err;
        if(!logitechDevice_.openKeyboard(&err)) {
            Q_EMIT error(QString::fromStdString(err));
            return false;
        }
        return true;
    }

    if(backend_ == Backend::LightMount) {
        if(lightMountDevice_.isOpen()) {
            return true;
        }
        std::string err;
        if(!lightMountDevice_.open(&err)) {
            Q_EMIT error(QString::fromStdString(err));
            return false;
        }
        return true;
    }

    if(backend_ == Backend::Alienware && device_.isOpen()) {
        return true;
    }

    std::string err;
    if(!device_.open(&err)) {
        Q_EMIT error(QString::fromStdString(err));
        return false;
    }
    backend_ = Backend::Alienware;
    return true;
}

bool KeyboardController::applySolid(const QColor& color, int brightnessPct) {
    if(g810KeyPressActive_) {
        stopG810KeyPressEffect();
    }
    if(!ensureOpen()) {
        return false;
    }
    const QColor c = scaled(color, brightnessPct);

    if(backend_ == Backend::LogitechHIDPP20) {
        std::string err;
        if(!logitechDevice_.setSolid(c.red(), c.green(), c.blue(), &err)) {
            Q_EMIT error(QString::fromStdString(err));
            return false;
        }
        return true;
    }

    if(backend_ == Backend::LightMount) {
        if(!lightMountDevice_.setSolid(c.red(), c.green(), c.blue())) {
            Q_EMIT error(i18n("Failed to set Light Mount solid colour."));
            return false;
        }
        return true;
    }

    if(!device_.setSolid(c.red(), c.green(), c.blue())) {
        Q_EMIT error(i18n("Failed to set solid colour."));
        return false;
    }
    return true;
}

bool KeyboardController::applyRainbow(int brightnessPct) {
    if(g810KeyPressActive_) {
        stopG810KeyPressEffect();
    }
    if(!ensureOpen()) {
        return false;
    }

    if(backend_ == Backend::LightMount) {
        const qreal v = qBound(0, brightnessPct, 100) / 100.0;
        std::vector<krgb::LightMountLedColor> leds;
        leds.reserve(krgb::lightmount::kKeys.size());
        const std::size_t total = krgb::lightmount::kKeys.size();
        for(std::size_t i = 0; i < total; ++i) {
            const QColor c = QColor::fromHsvF(
                total ? static_cast<qreal>(i) / total : 0.0, 1.0, v);
            leds.push_back({
                krgb::lightmount::kKeys[i].ledId,
                static_cast<std::uint8_t>(c.red()),
                static_cast<std::uint8_t>(c.green()),
                static_cast<std::uint8_t>(c.blue()),
            });
        }
        if(!lightMountDevice_.setCustomMode() || !lightMountDevice_.setLeds(leds)) {
            Q_EMIT error(i18n("Failed to set Light Mount static rainbow."));
            return false;
        }
        return true;
    }

    if(backend_ == Backend::LogitechHIDPP20) {
        if(!logitechG810Iso105Visual_) {
            Q_EMIT error(i18n("Static per-key rainbow is not available for this Logitech layout."));
            return false;
        }

        const qreal v = qBound(0, brightnessPct, 100) / 100.0;
        std::vector<krgb::LogitechHIDPP20KeyColor> keyboard;
        std::vector<krgb::LogitechHIDPP20KeyColor> media;
        std::vector<krgb::LogitechHIDPP20KeyColor> logo;
        std::vector<krgb::LogitechHIDPP20KeyColor> indicators;

        for(const auto& element : krgb::logitech::g810_iso105_visual::elements()) {
            const qreal hue =
                (element.x + element.w / 2.0f) /
                krgb::logitech::g810_iso105_visual::kLayoutWidth;
            const QColor c = QColor::fromHsvF(hue, 1.0, v);
            const krgb::LogitechHIDPP20KeyColor value{
                element.keyId,
                static_cast<std::uint8_t>(c.red()),
                static_cast<std::uint8_t>(c.green()),
                static_cast<std::uint8_t>(c.blue()),
            };

            switch(element.keyType) {
                case krgb::logitech::g610_g810::kKeyboardKeyType:
                    keyboard.push_back(value);
                    break;
                case krgb::logitech::g610_g810::kMediaKeyType:
                    media.push_back(value);
                    break;
                case krgb::logitech::g610_g810::kLogoKeyType:
                    logo.push_back(value);
                    break;
                case krgb::logitech::g610_g810::kIndicatorKeyType:
                    indicators.push_back(value);
                    break;
            }
        }

        std::string err;
        const auto writeGroup =
            [this, &err](std::uint16_t keyType,
                         const std::vector<krgb::LogitechHIDPP20KeyColor>& values) {
                return values.empty() ||
                       logitechDevice_.setPerKey8080Colors(keyType, values, &err);
            };

        if(!writeGroup(krgb::logitech::g610_g810::kKeyboardKeyType, keyboard) ||
           !writeGroup(krgb::logitech::g610_g810::kMediaKeyType, media) ||
           !writeGroup(krgb::logitech::g610_g810::kLogoKeyType, logo) ||
           !writeGroup(krgb::logitech::g610_g810::kIndicatorKeyType, indicators)) {
            Q_EMIT error(QString::fromStdString(err));
            return false;
        }
        return true;
    }

    const qreal v = qBound(0, brightnessPct, 100) / 100.0;
    const std::uint8_t bit = activeBit(device_);
    const std::size_t total = krgb::modelKeyCount(bit);
    std::vector<krgb::KeyColor> keys;
    keys.reserve(total);
    std::size_t n = 0;
    for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
        if(!(krgb::kKeyMap[i].models & bit)) {
            continue;  // key not present on this model
        }
        const qreal hue = total ? static_cast<qreal>(n++) / total : 0.0;
        const QColor c = QColor::fromHsvF(hue, 1.0, v);
        keys.push_back({krgb::kKeyMap[i].idx,
                        static_cast<std::uint8_t>(c.red()),
                        static_cast<std::uint8_t>(c.green()),
                        static_cast<std::uint8_t>(c.blue())});
    }
    if(!device_.setPerKey(keys)) {
        Q_EMIT error(i18n("Failed to set rainbow."));
        return false;
    }
    return true;
}

bool KeyboardController::applyPerKey(const QHash<QString, QColor>& keyColors, int brightnessPct) {
    if(g810KeyPressActive_) {
        stopG810KeyPressEffect();
    }
    if(!ensureOpen()) {
        return false;
    }

    if(backend_ == Backend::LightMount) {
        const int pct = qBound(0, brightnessPct, 100);
        std::vector<krgb::LightMountLedColor> leds;
        leds.reserve(krgb::lightmount::kKeys.size());
        for(const auto& key : krgb::lightmount::kKeys) {
            const QColor source =
                keyColors.value(QString::fromLatin1(key.name), QColor(0, 0, 0));
            leds.push_back({
                key.ledId,
                static_cast<std::uint8_t>(source.red() * pct / 100),
                static_cast<std::uint8_t>(source.green() * pct / 100),
                static_cast<std::uint8_t>(source.blue() * pct / 100),
            });
        }
        if(!lightMountDevice_.setCustomMode() || !lightMountDevice_.setLeds(leds)) {
            Q_EMIT error(i18n("Failed to set Light Mount per-key colours."));
            return false;
        }
        return true;
    }

    if(backend_ == Backend::LogitechHIDPP20) {
        if(!logitechG810Iso105Visual_) {
            Q_EMIT error(i18n("Per-key GUI geometry is not available for this Logitech layout."));
            return false;
        }

        const int pct = qBound(0, brightnessPct, 100);
        std::vector<krgb::LogitechHIDPP20KeyColor> keyboard;
        std::vector<krgb::LogitechHIDPP20KeyColor> media;
        std::vector<krgb::LogitechHIDPP20KeyColor> logo;
        std::vector<krgb::LogitechHIDPP20KeyColor> indicators;

        for(const auto& element : krgb::logitech::g810_iso105_visual::elements()) {
            const auto* def =
                krgb::logitech::g810_iso105_visual::definition(element);
            if(!def) {
                continue;
            }

            const QColor source =
                keyColors.value(QString::fromLatin1(def->name), QColor(0, 0, 0));
            const krgb::LogitechHIDPP20KeyColor value{
                element.keyId,
                static_cast<std::uint8_t>(source.red() * pct / 100),
                static_cast<std::uint8_t>(source.green() * pct / 100),
                static_cast<std::uint8_t>(source.blue() * pct / 100),
            };

            switch(element.keyType) {
                case krgb::logitech::g610_g810::kKeyboardKeyType:
                    keyboard.push_back(value);
                    break;
                case krgb::logitech::g610_g810::kMediaKeyType:
                    media.push_back(value);
                    break;
                case krgb::logitech::g610_g810::kLogoKeyType:
                    logo.push_back(value);
                    break;
                case krgb::logitech::g610_g810::kIndicatorKeyType:
                    indicators.push_back(value);
                    break;
            }
        }

        std::string err;
        const auto writeGroup =
            [this, &err](std::uint16_t keyType,
                         const std::vector<krgb::LogitechHIDPP20KeyColor>& values) {
                return values.empty() ||
                       logitechDevice_.setPerKey8080Colors(keyType, values, &err);
            };

        if(!writeGroup(krgb::logitech::g610_g810::kKeyboardKeyType, keyboard) ||
           !writeGroup(krgb::logitech::g610_g810::kMediaKeyType, media) ||
           !writeGroup(krgb::logitech::g610_g810::kLogoKeyType, logo) ||
           !writeGroup(krgb::logitech::g610_g810::kIndicatorKeyType, indicators)) {
            Q_EMIT error(QString::fromStdString(err));
            return false;
        }
        return true;
    }

    const int pct = qBound(0, brightnessPct, 100);
    const std::uint8_t bit = activeBit(device_);
    std::vector<krgb::KeyColor> keys;
    keys.reserve(krgb::kKeyCount);
    for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
        if(!(krgb::kKeyMap[i].models & bit)) {
            continue;  // key not present on this model
        }
        const QString name = QString::fromLatin1(krgb::kKeyMap[i].name);
        const QColor c = keyColors.value(name, QColor(0, 0, 0));
        keys.push_back({krgb::kKeyMap[i].idx,
                        static_cast<std::uint8_t>(c.red() * pct / 100),
                        static_cast<std::uint8_t>(c.green() * pct / 100),
                        static_cast<std::uint8_t>(c.blue() * pct / 100)});
    }
    if(!device_.setPerKey(keys)) {
        Q_EMIT error(i18n("Failed to set per-key colours."));
        return false;
    }
    return true;
}

bool KeyboardController::applyZones(
    const QHash<int, QColor>& zoneColors, int brightnessPct) {
    if(g810KeyPressActive_) {
        stopG810KeyPressEffect();
    }

    if(!ensureOpen()) {
        return false;
    }
    if(backend_ != Backend::LogitechHIDPP20 || logitechZoneCount_ <= 0) {
        Q_EMIT error(i18n("The connected keyboard does not expose HID++ zone lighting."));
        return false;
    }

    const int pct = qBound(0, brightnessPct, 100);
    std::vector<krgb::LogitechHIDPP20ZoneColor> colors;
    colors.reserve(static_cast<std::size_t>(logitechZoneCount_));

    for(int zone = 0; zone < logitechZoneCount_; ++zone) {
        const QColor source = zoneColors.value(zone, QColor(0, 0, 0));
        colors.push_back({
            static_cast<std::uint8_t>(zone),
            static_cast<std::uint8_t>(source.red() * pct / 100),
            static_cast<std::uint8_t>(source.green() * pct / 100),
            static_cast<std::uint8_t>(source.blue() * pct / 100),
        });
    }

    std::string err;
    if(!logitechDevice_.setColorLed8070Zones(colors, &err)) {
        Q_EMIT error(QString::fromStdString(err));
        return false;
    }
    return true;
}

bool KeyboardController::applyEffect(int modeValue, int speedValue, int directionValue,
                                     const QColor& color, int brightnessPct,
                                     int exactPeriodMs) {
    if(!ensureOpen()) {
        return false;
    }

    const auto mode = static_cast<Mode>(modeValue);

    if(backend_ == Backend::LightMount) {
        const std::uint8_t brightness = static_cast<std::uint8_t>(
            qBound(10, brightnessPct, 100));
        const std::uint8_t speed = lightMountSpeed(speedValue);
        const QColor c = scaled(color, 100);
        const krgb::LightMountColor selected{
            static_cast<std::uint8_t>(c.red()),
            static_cast<std::uint8_t>(c.green()),
            static_cast<std::uint8_t>(c.blue()),
        };

        krgb::LightMountGeneralEffect effect;
        switch(mode) {
            case Mode::Breathing:
                effect = krgb::lightmount::makeBreathingEffect(brightness, speed);
                break;
            case Mode::Pulse:
                effect = krgb::lightmount::makeReactiveEffect(
                    brightness, speed, selected, {0, 0, 0});
                break;
            case Mode::Spectrum:
                effect = krgb::lightmount::makeTornadoEffect(
                    krgb::LightMountDirection::Clockwise, brightness, speed);
                break;
            case Mode::SingleWave:
                effect = krgb::lightmount::makeColorWaveSingleEffect(
                    lightMountDirection(directionValue), brightness, speed, selected);
                break;
            case Mode::RainbowWave:
                effect = krgb::lightmount::makeColorWaveGradientEffect(
                    lightMountDirection(directionValue), brightness, speed,
                    krgb::lightmount::defaultRainbowGradient());
                break;
            case Mode::Scanner:
                effect = krgb::lightmount::makeMatrixEffect(
                    lightMountDirection(directionValue), brightness, speed);
                break;
            default:
                Q_EMIT error(i18n("This effect is not implemented for the Light Mount."));
                return false;
        }

        return applyLightMountGeneralEffect(effect);
    }

    if(backend_ == Backend::LogitechHIDPP20) {
        if(mode == Mode::Pulse && logitechG810Iso105Visual_) {
            const int fadeMs = exactPeriodMs > 0 ? exactPeriodMs : 150;
            return startG810KeyPressEffect(color, brightnessPct, fadeMs);
        }
        if(g810KeyPressActive_) {
            stopG810KeyPressEffect();
        }
        if(!supportsEffectMode(modeValue)) {
            Q_EMIT error(i18n("This hardware effect is not available for the connected Logitech keyboard."));
            return false;
        }

        std::uint16_t effectId = 0;
        switch(mode) {
            case Mode::Breathing:
                effectId = 0x000a;
                break;
            case Mode::Spectrum:
                effectId = 0x0003;
                break;
            case Mode::RainbowWave:
                effectId = 0x0004;
                break;
            case Mode::Scanner:
                effectId = 0x0005;
                break;
            default:
                Q_EMIT error(i18n("This hardware effect is not implemented for Logitech."));
                return false;
        }

        std::uint16_t periodMs = 5000;
        if(exactPeriodMs > 0) {
            periodMs = static_cast<std::uint16_t>(
                qBound(1, exactPeriodMs, 65535));
        } else {
            switch(static_cast<Speed>(speedValue)) {
                case Speed::Slowest: periodMs = 10000; break;
                case Speed::Normal:  periodMs = 5000;  break;
                case Speed::Fastest: periodMs = 2000;  break;
            }
        }

        const QColor c = scaled(color, brightnessPct);
        std::uint8_t wireDirection = 1;
        switch(static_cast<Direction>(directionValue)) {
            case Direction::Right: wireDirection = 1; break;
            case Direction::Down:  wireDirection = 2; break;
            case Direction::Left:  wireDirection = 6; break;
            case Direction::Up:    wireDirection = 7; break;
        }

        std::string err;
        if(!logitechDevice_.setColorLed8070Effect(
               effectId,
               static_cast<std::uint8_t>(c.red()),
               static_cast<std::uint8_t>(c.green()),
               static_cast<std::uint8_t>(c.blue()),
               periodMs,
               100,
               wireDirection,
               &err)) {
            Q_EMIT error(QString::fromStdString(err));
            return false;
        }
        return true;
    }

    ColorMode cm = ColorMode::Single;
    if(mode == Mode::Spectrum || mode == Mode::RainbowWave) {
        cm = ColorMode::Rainbow;
    }
    const QColor c = scaled(color, brightnessPct);
    const bool ok = device_.setEffect(mode,
                                      static_cast<Speed>(speedValue),
                                      static_cast<Direction>(directionValue),
                                      cm, c.red(), c.green(), c.blue());
    if(!ok) {
        Q_EMIT error(i18n("Failed to set effect."));
        return false;
    }
    return true;
}

bool KeyboardController::openG810InputMonitors(QString& error) {
    stopG810KeyPressEffect();

    const unsigned wantedPid = logitechDevice_.productId();
    std::error_code ec;
    const fs::path inputClass("/sys/class/input");
    if(!fs::exists(inputClass, ec)) {
        error = i18n("Linux input device directory is unavailable.");
        return false;
    }

    for(const auto& entry : fs::directory_iterator(inputClass, ec)) {
        if(ec) {
            break;
        }

        const std::string name = entry.path().filename().string();
        if(name.rfind("event", 0) != 0) {
            continue;
        }

        unsigned vendor = 0;
        unsigned product = 0;
        const fs::path idDir = entry.path() / "device" / "id";
        if(!readSysfsHex(idDir / "vendor", vendor) ||
           !readSysfsHex(idDir / "product", product) ||
           vendor != krgb::LogitechHIDPP20Device::kVendorId ||
           product != wantedPid) {
            continue;
        }

        const std::string devPath = "/dev/input/" + name;
        const int fd = ::open(devPath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if(fd < 0) {
            continue;
        }

        auto* notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this,
                [this, fd](QSocketDescriptor, QSocketNotifier::Type) {
                    handleG810InputReady(fd);
                });
        g810InputFds_.push_back(fd);
        g810InputNotifiers_.push_back(notifier);
        g810LastScanByFd_.insert(fd, 0);
    }

    if(g810InputFds_.isEmpty()) {
        error = i18n(
            "Could not open the G810 keyboard input event device. "
            "Install/reload the k-rgb udev rule and replug the keyboard.");
        return false;
    }
    return true;
}

void KeyboardController::stopG810KeyPressEffect() {
    if(g810KeyPressFadeTimer_) {
        g810KeyPressFadeTimer_->stop();
        g810KeyPressFadeTimer_->deleteLater();
        g810KeyPressFadeTimer_ = nullptr;
    }

    for(QSocketNotifier* notifier : g810InputNotifiers_) {
        if(notifier) {
            notifier->setEnabled(false);
            notifier->deleteLater();
        }
    }
    g810InputNotifiers_.clear();

    for(int fd : g810InputFds_) {
        if(fd >= 0) {
            ::close(fd);
        }
    }
    g810InputFds_.clear();
    g810LastScanByFd_.clear();
    g810KeyPressStartedMs_.clear();
    g810KeyPressActive_ = false;
}

bool KeyboardController::startG810KeyPressEffect(
    const QColor& foreground, int brightnessPct, int fadeMs) {

    if(!ensureOpen() || backend_ != Backend::LogitechHIDPP20 ||
       !logitechG810Iso105Visual_) {
        Q_EMIT error(i18n("Key Press is only available for the Logitech G810."));
        return false;
    }

    g810KeyPressForeground_ = scaled(foreground, brightnessPct);
    g810KeyPressBackground_ = QColor(0, 0, 0);
    g810KeyPressFadeMs_ = qBound(1, fadeMs, 65535);

    QString inputError;
    if(!openG810InputMonitors(inputError)) {
        Q_EMIT error(inputError);
        return false;
    }

    // LGS default: foreground #00dcff, background #000000, rate 150 ms.
    // 0x8070 exposes no Key Press firmware effect on the G810, so render the
    // keyboard portion through 0x8080 and use evdev key events as the trigger.
    std::string err;
    if(!logitechDevice_.setSolid(
           static_cast<std::uint8_t>(g810KeyPressBackground_.red()),
           static_cast<std::uint8_t>(g810KeyPressBackground_.green()),
           static_cast<std::uint8_t>(g810KeyPressBackground_.blue()), &err)) {
        stopG810KeyPressEffect();
        Q_EMIT error(QString::fromStdString(err));
        return false;
    }

    g810KeyPressActive_ = true;
    g810KeyPressFadeTimer_ = new QTimer(this);
    g810KeyPressFadeTimer_->setInterval(25);
    connect(g810KeyPressFadeTimer_, &QTimer::timeout, this, [this]() {
        if(!g810KeyPressActive_ || g810KeyPressStartedMs_.isEmpty()) {
            return;
        }
        renderG810KeyPressFrame();
    });
    g810KeyPressFadeTimer_->start();
    return true;
}

void KeyboardController::handleG810InputReady(int fd) {
    input_event events[32]{};
    for(;;) {
        const ssize_t n = ::read(fd, events, sizeof(events));
        if(n <= 0) {
            break;
        }

        const int count = static_cast<int>(n / sizeof(input_event));
        for(int i = 0; i < count; ++i) {
            const input_event& ev = events[i];
            if(ev.type == EV_MSC && ev.code == MSC_SCAN) {
                g810LastScanByFd_[fd] = static_cast<quint32>(ev.value);
                continue;
            }
            if(ev.type != EV_KEY) {
                continue;
            }

            const quint32 scan = g810LastScanByFd_.value(fd, 0);
            g810LastScanByFd_[fd] = 0;
            if(ev.value != 1 || scan == 0) {
                continue; // key-down only; ignore releases and repeats
            }

            // Linux HID input commonly exposes MSC_SCAN as
            // (usagePage << 16) | usage.  G810 keyboard keys are page 0x07.
            const quint16 usagePage = static_cast<quint16>((scan >> 16) & 0xffff);
            const quint16 usage = static_cast<quint16>(scan & 0xffff);
            if(usagePage == 0x0007 && usage <= 0xff) {
                handleG810KeyPress(
                    krgb::logitech::g610_g810::kKeyboardKeyType,
                    static_cast<std::uint8_t>(usage));
            }
        }
    }
}

void KeyboardController::handleG810KeyPress(
    std::uint16_t keyType, std::uint8_t keyId) {

    if(!g810KeyPressActive_ ||
       keyType != krgb::logitech::g610_g810::kKeyboardKeyType ||
       !krgb::logitech::g610_g810::findPhysical(keyType, keyId)) {
        return;
    }

    const quint32 address =
        (static_cast<quint32>(keyType) << 8) | static_cast<quint32>(keyId);
    g810KeyPressStartedMs_[address] =
        static_cast<qint64>(QDateTime::currentMSecsSinceEpoch());
    renderG810KeyPressFrame();
}

bool KeyboardController::renderG810KeyPressFrame(bool) {
    if(!g810KeyPressActive_) {
        return false;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    std::vector<krgb::LogitechHIDPP20KeyColor> keyboard;
    keyboard.reserve(
        krgb::logitech::g810_iso105_visual::kExpectedElementCount);

    QList<quint32> expired;
    for(const auto& element : krgb::logitech::g810_iso105_visual::elements()) {
        if(element.keyType != krgb::logitech::g610_g810::kKeyboardKeyType) {
            continue;
        }

        const quint32 address =
            (static_cast<quint32>(element.keyType) << 8) |
            static_cast<quint32>(element.keyId);
        QColor color = g810KeyPressBackground_;

        const auto it = g810KeyPressStartedMs_.constFind(address);
        if(it != g810KeyPressStartedMs_.cend()) {
            const qint64 elapsed = now - it.value();
            if(elapsed >= g810KeyPressFadeMs_) {
                expired.push_back(address);
            } else {
                const qreal amount =
                    1.0 - static_cast<qreal>(qMax<qint64>(0, elapsed)) /
                              static_cast<qreal>(g810KeyPressFadeMs_);
                color = QColor(
                    static_cast<int>(g810KeyPressBackground_.red() +
                        (g810KeyPressForeground_.red() -
                         g810KeyPressBackground_.red()) * amount),
                    static_cast<int>(g810KeyPressBackground_.green() +
                        (g810KeyPressForeground_.green() -
                         g810KeyPressBackground_.green()) * amount),
                    static_cast<int>(g810KeyPressBackground_.blue() +
                        (g810KeyPressForeground_.blue() -
                         g810KeyPressBackground_.blue()) * amount));
            }
        }

        keyboard.push_back({
            element.keyId,
            static_cast<std::uint8_t>(color.red()),
            static_cast<std::uint8_t>(color.green()),
            static_cast<std::uint8_t>(color.blue()),
        });
    }

    for(quint32 address : expired) {
        g810KeyPressStartedMs_.remove(address);
    }

    std::string err;
    if(!logitechDevice_.setPerKey8080Colors(
           krgb::logitech::g610_g810::kKeyboardKeyType, keyboard, &err)) {
        Q_EMIT error(QString::fromStdString(err));
        stopG810KeyPressEffect();
        return false;
    }
    return true;
}

bool KeyboardController::applyOff() {
    if(g810KeyPressActive_) {
        stopG810KeyPressEffect();
    }
    if(!ensureOpen()) {
        return false;
    }
    if(backend_ == Backend::LogitechHIDPP20) {
        std::string err;
        if(!logitechDevice_.setSolid(0, 0, 0, &err)) {
            Q_EMIT error(QString::fromStdString(err));
            return false;
        }
        return true;
    }
    if(backend_ == Backend::LightMount) {
        if(!lightMountDevice_.setLightingMode(krgb::LightMountLightingMode::Off)) {
            Q_EMIT error(i18n("Failed to turn Light Mount lighting off."));
            return false;
        }
        return true;
    }
    if(!device_.setOff()) {
        Q_EMIT error(i18n("Failed to turn lighting off."));
        return false;
    }
    return true;
}
