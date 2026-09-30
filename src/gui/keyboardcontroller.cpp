#include "keyboardcontroller.h"

#include "core/keymap.h"

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
            newModelName = QString::fromStdString(logitechDevice_.displayName());
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
    }

    if(nowConnected != connected_ || newPath != path_ ||
       newBackend != backend_ || newModelName != modelName_ ||
       newLogitechZoneCount != logitechZoneCount_) {
        connected_ = nowConnected;
        path_ = newPath;
        modelName_ = newModelName;
        modelBit_ = newModelBit;
        backend_ = newBackend;
        logitechZoneCount_ = newLogitechZoneCount;

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
        Q_EMIT error(i18n("This Logitech model is connected, but the GUI rainbow "
                          "layout is not available yet."));
        return false;
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
        Q_EMIT error(i18n("Per-key GUI geometry is not available for this Logitech "
                          "model yet."));
        return false;
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
    if(backend_ == Backend::LogitechHIDPP20) {
        Q_EMIT error(i18n("Hardware effects are not exposed in the Logitech GUI yet."));
        return false;
    }
    const auto mode = static_cast<Mode>(modeValue);
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
