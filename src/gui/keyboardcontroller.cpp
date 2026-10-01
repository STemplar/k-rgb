#include "keyboardcontroller.h"

#include "core/keymap.h"
#include "core/logitech_g610_g810_keymap.h"
#include "core/logitech_g810_iso105_visual.h"

#include <QFileInfo>
#include <QTimer>
#include <KLocalizedString>

using krgb::AW410KDevice;
using krgb::Mode;
using krgb::Speed;
using krgb::Direction;
using krgb::ColorMode;

namespace {

QColor scaled(const QColor& c, int pct) {
    pct = qBound(0, pct, 100);
    return QColor(c.red() * pct / 100, c.green() * pct / 100, c.blue() * pct / 100);
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

KeyboardController::~KeyboardController() = default;

void KeyboardController::refresh() {
    const krgb::KeyboardModel* model = nullptr;
    const std::string alienwarePath = AW410KDevice::findDevice(&model);

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
            if(logitechDevice_.isOpen()) {
                logitechDevice_.close();
            }
        }
        Q_EMIT connectionChanged(connected_, path_);
    }
}

namespace {
// Bit identifying the keys the currently-open device supports.
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

    if(!device_.setSolid(c.red(), c.green(), c.blue())) {
        Q_EMIT error(i18n("Failed to set solid colour."));
        return false;
    }
    return true;
}

bool KeyboardController::applyRainbow(int brightnessPct) {
    if(!ensureOpen()) {
        return false;
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
    if(!ensureOpen()) {
        return false;
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
                                     const QColor& color, int brightnessPct) {
    if(!ensureOpen()) {
        return false;
    }

    const auto mode = static_cast<Mode>(modeValue);

    if(backend_ == Backend::LogitechHIDPP20) {
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
        switch(static_cast<Speed>(speedValue)) {
            case Speed::Slowest: periodMs = 10000; break;
            case Speed::Normal:  periodMs = 5000;  break;
            case Speed::Fastest: periodMs = 2000;  break;
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

bool KeyboardController::applyOff() {
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
    if(!device_.setOff()) {
        Q_EMIT error(i18n("Failed to turn lighting off."));
        return false;
    }
    return true;
}
