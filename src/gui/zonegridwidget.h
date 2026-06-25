// ZoneGridWidget — a paintable grid of numbered case-lighting zones.
//
// The AW-ELC controller exposes N addressable zones with no known physical
// layout, so they're drawn as a simple numbered grid. Click a cell to select,
// drag to box-select, Ctrl-click to add/remove; the current selection can be
// painted a colour. Used by the GUI's per-zone case editor.
#pragma once

#include <QColor>
#include <QHash>
#include <QList>
#include <QRectF>
#include <QSet>
#include <QVector>
#include <QWidget>

class ZoneGridWidget : public QWidget {
    Q_OBJECT
public:
    explicit ZoneGridWidget(QWidget* parent = nullptr);

    void setZoneCount(int n);
    int  zoneCount() const { return zoneCount_; }

    QHash<int, QColor> zoneColors() const { return colors_; }
    void               setZoneColors(const QHash<int, QColor>& colors);

    QList<int> selectedZones() const;

public Q_SLOTS:
    void paintSelection(const QColor& color);
    void clearSelection();   // off (black) the current selection
    void selectAll();
    void fillAll(const QColor& color);

Q_SIGNALS:
    void changed();
    void selectionChanged(int count);

protected:
    void  paintEvent(QPaintEvent*) override;
    void  mousePressEvent(QMouseEvent*) override;
    void  mouseMoveEvent(QMouseEvent*) override;
    void  mouseReleaseEvent(QMouseEvent*) override;
    QSize sizeHint() const override;
    int   heightForWidth(int w) const override;
    bool  hasHeightForWidth() const override { return true; }

private:
    void recomputeLayout();
    int  zoneAt(const QPointF& p) const;
    int  rows() const;

    int                zoneCount_ = 0;
    int                cols_      = 16;
    QHash<int, QColor> colors_;
    QSet<int>          selected_;
    QVector<QRectF>    cells_;  // index == zone number

    bool          dragging_ = false;
    bool          moved_    = false;
    QPointF       pressPos_;
    QRectF        rubber_;
    QSet<int>     baseSelection_;
};
