// KeyboardController — Qt-friendly wrapper around supported keyboard backends.
//
// Owns the device handle, polls for presence/hot-plug, and exposes apply
// operations (solid colour, hardware effect, off). All user-facing colour
// scaling (brightness) happens here.
#pragma once

#include <QColor>
#include <QHash>
#include <QObject>
#include <QString>

#include "core/aw410k_device.h"
#include "core/logitech_hidpp20_device.h"

class QTimer;

class KeyboardController : public QObject {
    Q_OBJECT

    enum class Backend {
        None,
        Alienware,
        LogitechHIDPP20,
    };

public:
    explicit KeyboardController(QObject* parent = nullptr);
    ~KeyboardController() override;

    bool    isConnected() const { return connected_; }
    QString devicePath() const { return path_; }
    QString modelName() const { return modelName_; }
    quint8  modelBit() const { return modelBit_; }  // Alienware KeyboardModel::bit
    bool    supportsAdvancedModes() const { return backend_ == Backend::Alienware; }
    bool    supportsPerKeyColors() const {
        return backend_ == Backend::Alienware ||
               (backend_ == Backend::LogitechHIDPP20 && logitechG810Iso105Visual_);
    }
    bool    supportsStaticRainbow() const { return supportsPerKeyColors(); }
    bool    supportsEffectMode(int modeValue) const {
        if(backend_ == Backend::Alienware) {
            return true;
        }
        if(backend_ == Backend::LogitechHIDPP20 && logitechG810Iso105Visual_) {
            const auto mode = static_cast<krgb::Mode>(modeValue);
            return mode == krgb::Mode::Breathing || mode == krgb::Mode::Spectrum;
        }
        return false;
    }
    bool    usesLogitechG810Iso105VisualLayout() const {
        return backend_ == Backend::LogitechHIDPP20 && logitechG810Iso105Visual_;
    }
    bool    supportsZoneColors() const {
        return backend_ == Backend::LogitechHIDPP20 && logitechZoneCount_ > 1;
    }
    int     zoneCount() const { return logitechZoneCount_; }

public Q_SLOTS:
    bool applySolid(const QColor& color, int brightnessPct);
    bool applyRainbow(int brightnessPct);  // per-key static rainbow across all keys
    // Per-key custom colours, keyed by key name (see keymap.h). Keys absent from
    // the map are turned off.
    bool applyPerKey(const QHash<QString, QColor>& keyColors, int brightnessPct);
    bool applyZones(const QHash<int, QColor>& zoneColors, int brightnessPct);
    bool applyEffect(int modeValue, int speedValue, int directionValue,
                     const QColor& color, int brightnessPct);
    bool applyOff();
    void refresh();  // re-check device presence (called by poll timer)

Q_SIGNALS:
    void connectionChanged(bool connected, const QString& path);
    void error(const QString& message);

private:
    bool ensureOpen();

    krgb::AW410KDevice device_;
    krgb::LogitechHIDPP20Device logitechDevice_;
    Backend             backend_ = Backend::None;
    bool               connected_ = false;
    QString            path_;
    QString            modelName_;
    quint8             modelBit_ = 0xFF;
    int                logitechZoneCount_ = 0;
    bool               logitechG810Iso105Visual_ = false;
    QTimer*            pollTimer_ = nullptr;
};
