#include "zonegridwidget.h"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

#include <cmath>

namespace {
constexpr double kMargin = 6.0;
constexpr double kInset  = 2.0;
const QColor     kUnset(45, 45, 48);
const QColor     kBackground(28, 28, 30);
const QColor     kSelect(80, 170, 255);
}  // namespace

ZoneGridWidget::ZoneGridWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(80);
    setFocusPolicy(Qt::ClickFocus);
}

int ZoneGridWidget::rows() const {
    if(zoneCount_ <= 0 || cols_ <= 0) {
        return 0;
    }
    return (zoneCount_ + cols_ - 1) / cols_;
}

void ZoneGridWidget::setZoneCount(int n) {
    zoneCount_ = qMax(0, n);
    cols_ = qBound(1, zoneCount_ > 0 ? qMin(16, zoneCount_) : 1, 16);
    // Drop selections/colours for zones that no longer exist.
    QSet<int> keep;
    for(int z : selected_) {
        if(z < zoneCount_) {
            keep.insert(z);
        }
    }
    selected_ = keep;
    updateGeometry();
    update();
}

QSize ZoneGridWidget::sizeHint() const {
    return QSize(480, qMax(80, heightForWidth(480)));
}

int ZoneGridWidget::heightForWidth(int w) const {
    if(zoneCount_ <= 0) {
        return 80;
    }
    const double cell = (w - 2 * kMargin) / cols_;
    return static_cast<int>(cell * rows() + 2 * kMargin);
}

void ZoneGridWidget::setZoneColors(const QHash<int, QColor>& colors) {
    colors_ = colors;
    update();
}

QList<int> ZoneGridWidget::selectedZones() const {
    QList<int> list(selected_.cbegin(), selected_.cend());
    std::sort(list.begin(), list.end());
    return list;
}

void ZoneGridWidget::recomputeLayout() {
    cells_.clear();
    if(zoneCount_ <= 0) {
        return;
    }
    cells_.reserve(zoneCount_);
    const double availW = width() - 2 * kMargin;
    const double availH = height() - 2 * kMargin;
    const double cell = qMin(availW / cols_, availH / qMax(1, rows()));
    const double originX = (width() - cell * cols_) / 2.0;
    const double originY = kMargin;
    for(int z = 0; z < zoneCount_; ++z) {
        const int r = z / cols_;
        const int c = z % cols_;
        cells_.push_back(QRectF(originX + c * cell + kInset / 2.0,
                                originY + r * cell + kInset / 2.0,
                                cell - kInset, cell - kInset));
    }
}

int ZoneGridWidget::zoneAt(const QPointF& p) const {
    for(int z = 0; z < cells_.size(); ++z) {
        if(cells_[z].contains(p)) {
            return z;
        }
    }
    return -1;
}

void ZoneGridWidget::paintEvent(QPaintEvent*) {
    recomputeLayout();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), kBackground);

    QFont font = painter.font();
    font.setPointSizeF(qMax(6.0, font.pointSizeF() - 1.5));
    painter.setFont(font);

    for(int z = 0; z < cells_.size(); ++z) {
        const QColor fill = colors_.value(z, kUnset);
        const bool sel = selected_.contains(z);
        painter.setBrush(fill);
        painter.setPen(QPen(sel ? kSelect : QColor(0, 0, 0, 160), sel ? 2.0 : 1.0));
        painter.drawRoundedRect(cells_[z], 3.0, 3.0);

        const double lum = 0.299 * fill.red() + 0.587 * fill.green() + 0.114 * fill.blue();
        painter.setPen(lum > 140 ? QColor(20, 20, 20) : QColor(220, 220, 220));
        painter.drawText(cells_[z], Qt::AlignCenter, QString::number(z));
    }
}

void ZoneGridWidget::mousePressEvent(QMouseEvent* ev) {
    if(ev->button() != Qt::LeftButton) {
        return;
    }
    const bool additive = ev->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    pressPos_ = ev->position();
    dragging_ = true;
    moved_    = false;

    const int hit = zoneAt(pressPos_);
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
    moved_  = true;
    rubber_ = QRectF(pressPos_, p).normalized();
    selected_ = baseSelection_;
    for(int z = 0; z < cells_.size(); ++z) {
        if(rubber_.intersects(cells_[z])) {
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
        colors_.remove(z);  // unassigned == off
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
