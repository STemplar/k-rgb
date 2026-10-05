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
#include <QVector>

#include "core/aw410k_device.h"
#include "core/lightmount_device.h"
#include "core/logitech_hidpp20_device.h"

class QSocketNotifier;
class QTimer;

class KeyboardController : public QObject {
    Q_OBJECT

    enum class Backend {
        None,
        Alienware,
        LightMount,
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
               backend_ == Backend::LightMount ||
               (backend_ == Backend::LogitechHIDPP20 && logitechG810Iso105Visual_);
    }
    bool    supportsStaticRainbow() const { return supportsPerKeyColors(); }
    bool    supportsEffectMode(int modeValue) const {
        if(backend_ == Backend::Alienware) {
            return true;
        }
        if(backend_ == Backend::LightMount) {
            const auto mode = static_cast<krgb::Mode>(modeValue);
            return mode == krgb::Mode::Breathing ||
                   mode == krgb::Mode::Pulse ||       // IO Center Reactive
                   mode == krgb::Mode::Spectrum ||    // IO Center Tornado
                   mode == krgb::Mode::SingleWave ||  // IO Center Color Wave, single colour
                   mode == krgb::Mode::RainbowWave || // IO Center Color Wave, rainbow gradient
                   mode == krgb::Mode::Scanner;       // IO Center Matrix
        }
        if(backend_ == Backend::LogitechHIDPP20 && logitechG810Iso105Visual_) {
            const auto mode = static_cast<krgb::Mode>(modeValue);
            return mode == krgb::Mode::Breathing ||
                   mode == krgb::Mode::Pulse ||       // LGS Key Press (software)
                   mode == krgb::Mode::Spectrum ||
                   mode == krgb::Mode::RainbowWave ||
                   mode == krgb::Mode::Scanner;
        }
        return false;
    }
    bool    usesLogitechG810Iso105VisualLayout() const {
        return backend_ == Backend::LogitechHIDPP20 && logitechG810Iso105Visual_;
    }
    bool    usesBeQuietLightMount() const { return backend_ == Backend::LightMount; }
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
                     const QColor& color, int brightnessPct,
                     int exactPeriodMs = 0);
    bool applyOff();
    void refresh();  // re-check device presence (called by poll timer)

Q_SIGNALS:
    void connectionChanged(bool connected, const QString& path);
    void error(const QString& message);

private:
    bool ensureOpen();

    // Logitech G810 "Key Press" is an LGS software effect: pressed keys light
    // to the foreground colour and fade back to the background.  Input is read
    // from the kernel evdev nodes belonging to the same USB keyboard; lighting
    // is rendered through HID++ 0x8080.
    bool startG810KeyPressEffect(const QColor& foreground, int brightnessPct,
                                 int fadeMs);
    void stopG810KeyPressEffect();
    bool openG810InputMonitors(QString& error);
    void handleG810InputReady(int fd);
    void handleG810KeyPress(std::uint16_t keyType, std::uint8_t keyId);
    bool renderG810KeyPressFrame(bool includeStaticGroups = false);

    krgb::AW410KDevice device_;
    krgb::LightMountDevice lightMountDevice_;
    krgb::LogitechHIDPP20Device logitechDevice_;
    Backend             backend_ = Backend::None;
    bool               connected_ = false;
    QString            path_;
    QString            modelName_;
    quint8             modelBit_ = 0xFF;
    int                logitechZoneCount_ = 0;
    bool               logitechG810Iso105Visual_ = false;
    QTimer*            pollTimer_ = nullptr;

    bool                    g810KeyPressActive_ = false;
    QColor                  g810KeyPressForeground_{0, 220, 255};
    QColor                  g810KeyPressBackground_{0, 0, 0};
    int                     g810KeyPressFadeMs_ = 150;
    QTimer*                 g810KeyPressFadeTimer_ = nullptr;
    QVector<int>            g810InputFds_;
    QVector<QSocketNotifier*> g810InputNotifiers_;
    QHash<int, quint32>     g810LastScanByFd_;
    QHash<quint32, qint64>  g810KeyPressStartedMs_;
};
