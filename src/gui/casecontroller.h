// CaseController — Qt-friendly wrapper around krgb::AlienFXDevice (the case /
// chassis lighting controller). Detects presence, and applies a whole-case
// colour. Separate from the keyboard's KeyboardController (different device).
#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include "core/alienfx_device.h"

class QTimer;

class CaseController : public QObject {
    Q_OBJECT
public:
    explicit CaseController(QObject* parent = nullptr);
    ~CaseController() override;

    bool    isAvailable() const { return available_; }
    int     zoneCount() const { return zoneCount_; }
    QString description() const;  // e.g. "AW-ELC controller, 77 zones"

public Q_SLOTS:
    bool applySolid(const QColor& color);
    // Per-zone colours, keyed by zone index. Zones absent from the map are off.
    bool applyZones(const QHash<int, QColor>& zoneColors);
    // Light only the given zones (white), the rest off — to locate a zone.
    bool identify(const QList<int>& zones);
    bool applyOff();
    void refresh();  // re-check presence (poll timer)

Q_SIGNALS:
    void availabilityChanged(bool available);
    void error(const QString& message);

private:
    bool ensureOpen();

    krgb::AlienFXDevice device_;
    bool                available_ = false;
    int                 zoneCount_ = 0;
    QTimer*             pollTimer_ = nullptr;
};
