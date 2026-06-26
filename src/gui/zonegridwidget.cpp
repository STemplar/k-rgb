#include "zonegridwidget.h"

#include <QEvent>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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
    setMinimumHeight(300);
    setMouseTracking(true);  // track hover to enlarge the marker under the cursor
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

    drawCaseBackground(painter);

    const double cell = cellSize();
    const bool   showNumbers = cell >= 13.0;  // too small to label at quarter size
    QFont font = painter.font();
    font.setPointSizeF(qMax(5.5, font.pointSizeF() - 2.0));
    painter.setFont(font);

    for(int z = 0; z < zoneCount_; ++z) {
        const QRectF r = rectOf(z);
        const QColor fill = colors_.value(z, kUnset);
        const bool sel = selected_.contains(z);
        painter.setBrush(fill);
        painter.setPen(QPen(sel ? kSelect : QColor(0, 0, 0, 180), sel ? 1.6 : 0.8));
        painter.drawRoundedRect(r, 2.0, 2.0);
        if(showNumbers) {
            const double lum = 0.299 * fill.red() + 0.587 * fill.green() + 0.114 * fill.blue();
            painter.setPen(lum > 140 ? QColor(20, 20, 20) : QColor(220, 220, 220));
            painter.drawText(r, Qt::AlignCenter, QString::number(z));
        }
    }

    // Enlarge the hovered marker so its number is readable while arranging.
    if(hoveredZone_ >= 0 && hoveredZone_ < zoneCount_ && !dragging_) {
        const QPointF c = rectOf(hoveredZone_).center();
        const double s = 24.0;
        const QRectF big(c.x() - s / 2, c.y() - s / 2, s, s);
        const QColor fill = colors_.value(hoveredZone_, kUnset);
        painter.setBrush(fill);
        painter.setPen(QPen(kSelect, 2.0));
        painter.drawRoundedRect(big, 3.0, 3.0);
        QFont bf = font;
        bf.setPointSizeF(9.0);
        painter.setFont(bf);
        const double lum = 0.299 * fill.red() + 0.587 * fill.green() + 0.114 * fill.blue();
        painter.setPen(lum > 140 ? QColor(20, 20, 20) : QColor(235, 235, 235));
        painter.drawText(big, Qt::AlignCenter, QString::number(hoveredZone_));
        painter.setFont(font);
    }

    if(!rubber_.isNull()) {
        painter.setPen(QPen(kSelect, 1.0, Qt::DashLine));
        painter.setBrush(QColor(80, 170, 255, 40));
        painter.drawRect(rubber_);
    }
}

// A stylised Alienware Aurora R12 front view to arrange zones against (original
// schematic, not a product photo). Muted so the zone markers stand out on top.
void ZoneGridWidget::drawCaseBackground(QPainter& p) const {
    const double W = width(), H = height();
    const double ch = H * 0.90;
    const double cw = ch * 0.46;             // tall, fairly narrow tower
    const double cx = W / 2.0, cy = H / 2.0;
    const QRectF body(cx - cw / 2, cy - ch / 2, cw, ch);
    const double bevel = cw * 0.16;

    p.save();
    const QColor edge(74, 80, 92), bodyCol(40, 42, 49), faceCol(46, 49, 57);

    // Faceted tower outline.
    QPainterPath path;
    path.moveTo(body.left() + bevel, body.top());
    path.lineTo(body.right() - bevel, body.top());
    path.lineTo(body.right(), body.top() + bevel);
    path.lineTo(body.right(), body.bottom() - bevel);
    path.lineTo(body.right() - bevel, body.bottom());
    path.lineTo(body.left() + bevel, body.bottom());
    path.lineTo(body.left(), body.bottom() - bevel);
    path.lineTo(body.left(), body.top() + bevel);
    path.closeSubpath();
    p.setPen(QPen(edge, 1.5));
    p.setBrush(bodyCol);
    p.drawPath(path);

    // Front face plate + centre seam.
    const QRectF face(body.left() + cw * 0.12, body.top() + ch * 0.06,
                      cw * 0.76, ch * 0.88);
    p.setBrush(faceCol);
    p.setPen(QPen(edge, 1.0));
    p.drawRoundedRect(face, cw * 0.06, cw * 0.06);

    // Top vent slits.
    p.setPen(QPen(edge, 1.0));
    for(int i = 0; i < 4; ++i) {
        const double y = body.top() + bevel + 6 + i * 5;
        p.drawLine(QPointF(cx - cw * 0.22, y), QPointF(cx + cw * 0.22, y));
    }

    // The AlienFX light ring (front, lower-centre).
    const QPointF ringC(cx, body.top() + ch * 0.46);
    const double ringR = cw * 0.27;
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(90, 140, 190), 2.4));
    p.drawEllipse(ringC, ringR, ringR);
    p.setPen(QPen(QColor(60, 78, 104), 1.0));
    p.drawEllipse(ringC, ringR * 0.66, ringR * 0.66);

    // Alien-head power button (diamond) near the top.
    const QPointF head(cx, body.top() + ch * 0.02 + bevel * 0.2);
    const double hr = cw * 0.07;
    QPainterPath diamond;
    diamond.moveTo(head.x(), head.y() - hr);
    diamond.lineTo(head.x() + hr * 0.7, head.y());
    diamond.lineTo(head.x(), head.y() + hr);
    diamond.lineTo(head.x() - hr * 0.7, head.y());
    diamond.closeSubpath();
    p.setPen(QPen(QColor(90, 140, 190), 1.2));
    p.drawPath(diamond);

    // Label.
    p.setPen(QColor(120, 126, 138));
    QFont f = p.font();
    f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.0));
    p.setFont(f);
    p.drawText(QRectF(cx - cw, body.bottom() + 1, cw * 2, 16),
               Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("Alienware Aurora R12 — reference"));
    p.restore();
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
    const QPointF p = ev->position();

    if(!dragging_) {
        const int hit = zoneAt(p);  // hover highlight
        if(hit != hoveredZone_) {
            hoveredZone_ = hit;
            update();
        }
        return;
    }
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

void ZoneGridWidget::leaveEvent(QEvent*) {
    if(hoveredZone_ != -1) {
        hoveredZone_ = -1;
        update();
    }
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
