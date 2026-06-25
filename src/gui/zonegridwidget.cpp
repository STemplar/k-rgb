#include "zonegridwidget.h"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

namespace {
constexpr double kMargin = 6.0;
constexpr double kInset  = 2.0;
const QColor     kUnset(45, 45, 48);
const QColor     kBackground(28, 28, 30);
const QColor     kSelect(80, 170, 255);

int clampi(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }
}  // namespace

ZoneGridWidget::ZoneGridWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    setFocusPolicy(Qt::ClickFocus);
}

double ZoneGridWidget::cellSize() const {
    return (width() - 2 * kMargin) / static_cast<double>(kCols);
}

QPoint ZoneGridWidget::posOf(int zone) const {
    return pos_.value(zone, QPoint(zone % 16, zone / 16));  // default: 16-wide grid
}

QRectF ZoneGridWidget::rectOf(int zone) const {
    const double cell = cellSize();
    const QPoint g = posOf(zone);
    return QRectF(kMargin + g.x() * cell + kInset / 2.0,
                  kMargin + g.y() * cell + kInset / 2.0,
                  cell - kInset, cell - kInset);
}

QPoint ZoneGridWidget::gridAt(const QPointF& p) const {
    const double cell = cellSize();
    const int col = clampi(static_cast<int>((p.x() - kMargin) / cell), 0, kCols - 1);
    const int row = clampi(static_cast<int>((p.y() - kMargin) / cell), 0, kMaxRows - 1);
    return QPoint(col, row);
}

int ZoneGridWidget::zoneAt(const QPointF& p) const {
    for(int z = 0; z < zoneCount_; ++z) {
        if(rectOf(z).contains(p)) {
            return z;
        }
    }
    return -1;
}

void ZoneGridWidget::setZoneCount(int n) {
    zoneCount_ = qMax(0, n);
    QSet<int> keep;
    for(int z : selected_) {
        if(z < zoneCount_) {
            keep.insert(z);
        }
    }
    selected_ = keep;
    update();
}

void ZoneGridWidget::setZoneColors(const QHash<int, QColor>& colors) {
    colors_ = colors;
    update();
}

void ZoneGridWidget::setPositions(const QHash<int, QPoint>& positions) {
    pos_ = positions;
    update();
}

QList<int> ZoneGridWidget::selectedZones() const {
    QList<int> list(selected_.cbegin(), selected_.cend());
    std::sort(list.begin(), list.end());
    return list;
}

void ZoneGridWidget::setArrangeMode(bool on) {
    arrangeMode_ = on;
    setCursor(on ? Qt::OpenHandCursor : Qt::ArrowCursor);
    update();
}

void ZoneGridWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), kBackground);

    QFont font = painter.font();
    font.setPointSizeF(qMax(6.0, font.pointSizeF() - 1.5));
    painter.setFont(font);

    for(int z = 0; z < zoneCount_; ++z) {
        const QRectF r = rectOf(z);
        const QColor fill = colors_.value(z, kUnset);
        const bool sel = selected_.contains(z);
        painter.setBrush(fill);
        painter.setPen(QPen(sel ? kSelect : QColor(0, 0, 0, 160), sel ? 2.0 : 1.0));
        painter.drawRoundedRect(r, 3.0, 3.0);

        const double lum = 0.299 * fill.red() + 0.587 * fill.green() + 0.114 * fill.blue();
        painter.setPen(lum > 140 ? QColor(20, 20, 20) : QColor(220, 220, 220));
        painter.drawText(r, Qt::AlignCenter, QString::number(z));
    }

    if(!rubber_.isNull()) {
        painter.setPen(QPen(kSelect, 1.0, Qt::DashLine));
        painter.setBrush(QColor(80, 170, 255, 40));
        painter.drawRect(rubber_);
    }
}

void ZoneGridWidget::mousePressEvent(QMouseEvent* ev) {
    if(ev->button() != Qt::LeftButton) {
        return;
    }
    pressPos_ = ev->position();
    dragging_ = true;
    moved_    = false;
    const int hit = zoneAt(pressPos_);

    if(arrangeMode_) {
        if(hit < 0) {
            selected_.clear();
            update();
            Q_EMIT selectionChanged(0);
            dragging_ = false;
            return;
        }
        if(!selected_.contains(hit)) {
            selected_ = {hit};
            Q_EMIT selectionChanged(1);
        }
        setCursor(Qt::ClosedHandCursor);
        pressGrid_ = gridAt(pressPos_);
        dragOrigin_.clear();
        for(int z : selected_) {
            dragOrigin_.insert(z, posOf(z));
        }
        update();
        return;
    }

    // Paint mode: select.
    const bool additive = ev->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    if(!additive) {
        selected_.clear();
        if(hit >= 0) {
            selected_.insert(hit);
        }
    } else if(hit >= 0) {
        if(selected_.contains(hit)) {
            selected_.remove(hit);
        } else {
            selected_.insert(hit);
        }
    }
    baseSelection_ = selected_;
    update();
    Q_EMIT selectionChanged(selected_.size());
}

void ZoneGridWidget::mouseMoveEvent(QMouseEvent* ev) {
    if(!dragging_) {
        return;
    }
    const QPointF p = ev->position();
    if(!moved_ && (p - pressPos_).manhattanLength() < 4) {
        return;
    }
    moved_ = true;

    if(arrangeMode_) {
        const QPoint delta = gridAt(p) - pressGrid_;
        for(auto it = dragOrigin_.cbegin(); it != dragOrigin_.cend(); ++it) {
            const QPoint np(clampi(it.value().x() + delta.x(), 0, kCols - 1),
                            clampi(it.value().y() + delta.y(), 0, kMaxRows - 1));
            pos_.insert(it.key(), np);
        }
        update();
        return;
    }

    // Paint mode: rubber-band select.
    rubber_ = QRectF(pressPos_, p).normalized();
    selected_ = baseSelection_;
    for(int z = 0; z < zoneCount_; ++z) {
        if(rubber_.intersects(rectOf(z))) {
            selected_.insert(z);
        }
    }
    update();
    Q_EMIT selectionChanged(selected_.size());
}

void ZoneGridWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if(ev->button() != Qt::LeftButton) {
        return;
    }
    dragging_ = false;
    rubber_   = QRectF();
    if(arrangeMode_) {
        setCursor(Qt::OpenHandCursor);
        if(moved_) {
            Q_EMIT layoutChanged();
        }
    }
    update();
}

void ZoneGridWidget::paintSelection(const QColor& color) {
    if(selected_.isEmpty() || !color.isValid()) {
        return;
    }
    for(int z : selected_) {
        colors_.insert(z, color);
    }
    update();
    Q_EMIT changed();
}

void ZoneGridWidget::clearSelection() {
    if(selected_.isEmpty()) {
        return;
    }
    for(int z : selected_) {
        colors_.remove(z);
    }
    update();
    Q_EMIT changed();
}

void ZoneGridWidget::selectAll() {
    selected_.clear();
    for(int z = 0; z < zoneCount_; ++z) {
        selected_.insert(z);
    }
    update();
    Q_EMIT selectionChanged(selected_.size());
}

void ZoneGridWidget::fillAll(const QColor& color) {
    if(!color.isValid()) {
        return;
    }
    for(int z = 0; z < zoneCount_; ++z) {
        colors_.insert(z, color);
    }
    update();
    Q_EMIT changed();
}
