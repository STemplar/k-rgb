// ZoneGridWidget — a drag-to-arrange canvas of numbered case-lighting zones.
//
// The AW-ELC controller exposes N addressable zones with no fixed physical
// layout. In Arrange mode you drag zones to positions that mirror your case
// (saved per-machine); in Paint mode you select zones (click / box / Ctrl) and
// paint them a colour. Zones snap to a coarse grid.
#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QPoint>
#include <QRectF>
#include <QSet>
#include <QWidget>

class ZoneGridWidget : public QWidget {
    Q_OBJECT
public:
    explicit ZoneGridWidget(QWidget* parent = nullptr);

    void setZoneCount(int n);
    int  zoneCount() const { return zoneCount_; }

    QHash<int, QColor> zoneColors() const { return colors_; }
    void               setZoneColors(const QHash<int, QColor>& colors);

    // Per-machine layout: zone index -> (column, row) on the snap grid.
    QHash<int, QPoint> positions() const { return pos_; }
    void               setPositions(const QHash<int, QPoint>& positions);

    QList<int> selectedZones() const;
    bool       arrangeMode() const { return arrangeMode_; }

    // Fine snap grid → small (~quarter-size) markers that fit the window and can
    // be placed precisely over the case background. Hovering enlarges a marker
    // so its number stays readable while arranging.
    static constexpr int kCols    = 96;
    static constexpr int kMaxRows = 56;

public Q_SLOTS:
    void setArrangeMode(bool on);
    void paintSelection(const QColor& color);
    void clearSelection();   // off (black) the current selection
    void selectAll();
    void fillAll(const QColor& color);

Q_SIGNALS:
    void changed();                  // colours changed
    void layoutChanged();            // zone positions changed
    void selectionChanged(int count);

protected:
    void  paintEvent(QPaintEvent*) override;
    void  mousePressEvent(QMouseEvent*) override;
    void  mouseMoveEvent(QMouseEvent*) override;
    void  mouseReleaseEvent(QMouseEvent*) override;
    void  leaveEvent(QEvent*) override;
    QSize sizeHint() const override { return QSize(640, 360); }

private:
    void   drawCaseBackground(QPainter& p) const;  // Alienware Aurora R12 schematic
    double cellSize() const;
    QPoint posOf(int zone) const;     // grid (col,row), default if unset
    QRectF rectOf(int zone) const;    // pixel rect
    QPoint gridAt(const QPointF& p) const;
    int    zoneAt(const QPointF& p) const;

    int                zoneCount_ = 0;
    QHash<int, QColor> colors_;
    QHash<int, QPoint> pos_;
    QSet<int>          selected_;
    bool               arrangeMode_ = false;
    int                hoveredZone_ = -1;

    // interaction state
    bool               dragging_ = false;
    bool               moved_    = false;
    QPointF            pressPos_;
    QRectF             rubber_;
    QSet<int>          baseSelection_;
    QPoint             pressGrid_;
    QHash<int, QPoint> dragOrigin_;  // positions of moved zones at drag start
};
