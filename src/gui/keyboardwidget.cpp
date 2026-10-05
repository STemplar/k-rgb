#include "keyboardwidget.h"

#include "core/keymap.h"
#include "core/lightmount_ansi_visual.h"
#include "core/logitech_g810_iso105_visual.h"

#include <QFont>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace {
constexpr double kMargin   = 6.0;
constexpr double kKeyInset = 2.0;
const QColor     kUnset(45, 45, 48);
const QColor     kBackground(28, 28, 30);
const QColor     kSelect(80, 170, 255);
constexpr quint8 kLightMountLayoutModelBit = 0xFE;

QString labelFor(const QString& name) {
    static const QHash<QString, QString> pretty = {
        {QStringLiteral("BACKSPACE"), QStringLiteral("⌫")},
        {QStringLiteral("ENTER"), QStringLiteral("↵")},
        {QStringLiteral("NPENTER"), QStringLiteral("↵")},
        {QStringLiteral("LSHIFT"), QStringLiteral("⇧")},
        {QStringLiteral("RSHIFT"), QStringLiteral("⇧")},
        {QStringLiteral("CAPS"), QStringLiteral("Caps")},
        {QStringLiteral("TAB"), QStringLiteral("Tab")},
        {QStringLiteral("MINUS"), QStringLiteral("-")},
        {QStringLiteral("EQUALS"), QStringLiteral("=")},
        {QStringLiteral("NUMLK"), QStringLiteral("Num")},
        {QStringLiteral("SCRLK"), QStringLiteral("Scrl")},
        {QStringLiteral("PRTSC"), QStringLiteral("Prt")},
        {QStringLiteral("PAUSE"), QStringLiteral("Pause")},
        {QStringLiteral("UP"), QStringLiteral("↑")},
        {QStringLiteral("DOWN"), QStringLiteral("↓")},
        {QStringLiteral("LEFT"), QStringLiteral("←")},
        {QStringLiteral("RIGHT"), QStringLiteral("→")},
        {QStringLiteral("LWIN"), QStringLiteral("❖")},
        {QStringLiteral("RWIN"), QStringLiteral("❖")},
        {QStringLiteral("GRAVE"), QStringLiteral("`")},
        {QStringLiteral("EQUAL"), QStringLiteral("=")},
        {QStringLiteral("LBRACKET"), QStringLiteral("[")},
        {QStringLiteral("RBRACKET"), QStringLiteral("]")},
        {QStringLiteral("SEMICOLON"), QStringLiteral(";")},
        {QStringLiteral("APOSTROPHE"), QStringLiteral("'")},
        {QStringLiteral("ISO_ENTER"), QStringLiteral("\\")},
        {QStringLiteral("ISO_LSHIFT"), QStringLiteral("\\")},
        {QStringLiteral("COMMA"), QStringLiteral(",")},
        {QStringLiteral("DOT"), QStringLiteral(".")},
        {QStringLiteral("SLASH"), QStringLiteral("/")},
        {QStringLiteral("PRINT"), QStringLiteral("Prt")},
        {QStringLiteral("NUMLOCK"), QStringLiteral("Num")},
        {QStringLiteral("NUMSLASH"), QStringLiteral("/")},
        {QStringLiteral("NUMSTAR"), QStringLiteral("*")},
        {QStringLiteral("NUMMINUS"), QStringLiteral("-")},
        {QStringLiteral("NUMPLUS"), QStringLiteral("+")},
        {QStringLiteral("NUMDOT"), QStringLiteral(".")},
        {QStringLiteral("NUMENTER"), QStringLiteral("↵")},
        {QStringLiteral("PLAY"), QStringLiteral("▶")},
        {QStringLiteral("STOP"), QStringLiteral("■")},
        {QStringLiteral("PREV"), QStringLiteral("⏮")},
        {QStringLiteral("NEXT"), QStringLiteral("⏭")},
        {QStringLiteral("LIGHT"), QStringLiteral("Light")},
        {QStringLiteral("GAME"), QStringLiteral("Game")},
        {QStringLiteral("CAPS_LED"), QStringLiteral("Caps")},
        {QStringLiteral("SCROLL_LED"), QStringLiteral("Scrl")},
        {QStringLiteral("NUM_LED"), QStringLiteral("Num")},
        {QStringLiteral("LOGO"), QStringLiteral("G")},
        {QStringLiteral("MUTE"), QStringLiteral("🔇")},
        {QStringLiteral("VOLDN"), QStringLiteral("🔉")},
        {QStringLiteral("VOLUP"), QStringLiteral("🔊")},
    };
    const auto it = pretty.constFind(name);
    if(it != pretty.constEnd()) {
        return it.value();
    }
    if(name.startsWith(QStringLiteral("NP"))) {
        return name.mid(2);
    }
    return name;
}

bool isBeQuietLayout(KeyboardWidget::LayoutKind kind) {
    return kind == KeyboardWidget::LayoutKind::BeQuietLightMountAnsi;
}

void drawBeQuietMuteMark(QPainter& painter, const QRectF& cell,
                         const QColor& color) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal d = qMin(cell.width(), cell.height());
    const QRectF icon(cell.center().x() - d * 0.25,
                      cell.center().y() - d * 0.22,
                      d * 0.50, d * 0.44);

    QPen pen(color, qMax<qreal>(1.2, d * 0.035));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    QPainterPath speaker;
    speaker.moveTo(icon.left(), icon.center().y() - icon.height() * 0.16);
    speaker.lineTo(icon.left() + icon.width() * 0.22,
                   icon.center().y() - icon.height() * 0.16);
    speaker.lineTo(icon.left() + icon.width() * 0.46, icon.top());
    speaker.lineTo(icon.left() + icon.width() * 0.46, icon.bottom());
    speaker.lineTo(icon.left() + icon.width() * 0.22,
                   icon.center().y() + icon.height() * 0.16);
    speaker.lineTo(icon.left(), icon.center().y() + icon.height() * 0.16);
    speaker.closeSubpath();
    painter.drawPath(speaker);

    painter.drawLine(QPointF(icon.left() + icon.width() * 0.62,
                             icon.top() + icon.height() * 0.24),
                     QPointF(icon.right(),
                             icon.bottom() - icon.height() * 0.24));
    painter.drawLine(QPointF(icon.right(),
                             icon.top() + icon.height() * 0.24),
                     QPointF(icon.left() + icon.width() * 0.62,
                             icon.bottom() - icon.height() * 0.24));
    painter.restore();
}

enum class BeQuietArrow { Up, Down, Left, Right };

void drawBeQuietTriangle(QPainter& painter, const QPointF& center,
                         qreal keySize, BeQuietArrow direction,
                         const QColor& color, qreal scale) {
    const qreal halfW = keySize * scale * 0.50;
    const qreal halfH = keySize * scale * 0.42;

    QPainterPath path;
    switch(direction) {
        case BeQuietArrow::Up:
            path.moveTo(center.x(), center.y() - halfH);
            path.lineTo(center.x() - halfW, center.y() + halfH);
            path.lineTo(center.x() + halfW, center.y() + halfH);
            break;
        case BeQuietArrow::Down:
            path.moveTo(center.x(), center.y() + halfH);
            path.lineTo(center.x() - halfW, center.y() - halfH);
            path.lineTo(center.x() + halfW, center.y() - halfH);
            break;
        case BeQuietArrow::Left:
            path.moveTo(center.x() - halfH, center.y());
            path.lineTo(center.x() + halfH, center.y() - halfW);
            path.lineTo(center.x() + halfH, center.y() + halfW);
            break;
        case BeQuietArrow::Right:
            path.moveTo(center.x() + halfH, center.y());
            path.lineTo(center.x() - halfH, center.y() - halfW);
            path.lineTo(center.x() - halfH, center.y() + halfW);
            break;
    }
    path.closeSubpath();

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPath(path);
    painter.restore();
}

bool dedicatedBeQuietCursor(const QString& name, BeQuietArrow& direction) {
    if(name == QStringLiteral("UP")) {
        direction = BeQuietArrow::Up;
        return true;
    }
    if(name == QStringLiteral("DOWN")) {
        direction = BeQuietArrow::Down;
        return true;
    }
    if(name == QStringLiteral("LEFT")) {
        direction = BeQuietArrow::Left;
        return true;
    }
    if(name == QStringLiteral("RIGHT")) {
        direction = BeQuietArrow::Right;
        return true;
    }
    return false;
}

bool numpadBeQuietCursor(const QString& name, BeQuietArrow& direction) {
    if(name == QStringLiteral("NUM8")) {
        direction = BeQuietArrow::Up;
        return true;
    }
    if(name == QStringLiteral("NUM2")) {
        direction = BeQuietArrow::Down;
        return true;
    }
    if(name == QStringLiteral("NUM4")) {
        direction = BeQuietArrow::Left;
        return true;
    }
    if(name == QStringLiteral("NUM6")) {
        direction = BeQuietArrow::Right;
        return true;
    }
    return false;
}

void drawBeQuietSpaceMark(QPainter& painter, const QRectF& cell,
                          const QColor& color) {
    painter.save();
    QPen pen(color, qMax<qreal>(1.0, cell.height() * 0.035));
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    const qreal width = cell.width() * 0.17;
    const qreal y = cell.top() + cell.height() * 0.46;
    painter.drawLine(QPointF(cell.center().x() - width / 2.0, y),
                     QPointF(cell.center().x() + width / 2.0, y));
    painter.restore();
}

QFont beQuietLegendFont(const QFont& base, const QRectF& cell,
                        const QString& text) {
    QFont font(base);
    font.setFamilies({QStringLiteral("Noto Sans"),
                      QStringLiteral("DejaVu Sans"),
                      QStringLiteral("sans-serif")});
    font.setStyleHint(QFont::SansSerif);
    font.setWeight(QFont::DemiBold);

    const qreal keySize = qMin(cell.width(), cell.height());
    font.setPixelSize(qBound(9, static_cast<int>(keySize * 0.32), 16));

    while(font.pixelSize() > 8) {
        const QFontMetricsF fm(font);
        if(fm.horizontalAdvance(text) <= cell.width() * 0.84) {
            break;
        }
        font.setPixelSize(font.pixelSize() - 1);
    }
    return font;
}

QRectF beQuietLegendRect(const QRectF& cell) {
    const qreal keySize = qMin(cell.width(), cell.height());
    return QRectF(cell.left() + 1.5,
                  cell.top() + keySize * 0.10,
                  qMax<qreal>(1.0, cell.width() - 3.0),
                  keySize * 0.48);
}

bool looksLikeUkrainePreset(const QHash<QString, QColor>& colors) {
    if(colors.size() < 50) {
        return false;
    }
    const QColor blue(0, 87, 183);
    const QColor yellow(255, 215, 0);
    bool hasBlue = false;
    bool hasYellow = false;
    for(auto it = colors.cbegin(); it != colors.cend(); ++it) {
        if(it.value() == blue) {
            hasBlue = true;
        } else if(it.value() == yellow) {
            hasYellow = true;
        } else {
            return false;
        }
    }
    return hasBlue && hasYellow;
}

bool looksLikeUsaPreset(const QHash<QString, QColor>& colors) {
    if(colors.size() < 50) {
        return false;
    }
    const QColor red(140, 22, 36);
    const QColor white(255, 255, 255);
    const QColor blue(28, 28, 75);
    bool hasRed = false;
    bool hasWhite = false;
    bool hasBlue = false;
    for(auto it = colors.cbegin(); it != colors.cend(); ++it) {
        if(it.value() == red) {
            hasRed = true;
        } else if(it.value() == white) {
            hasWhite = true;
        } else if(it.value() == blue) {
            hasBlue = true;
        } else {
            return false;
        }
    }
    return hasRed && hasWhite && hasBlue;
}

QHash<QString, QColor> lightMountUkrainePreset() {
    QHash<QString, QColor> result;
    const QColor blue(0, 87, 183);
    const QColor yellow(255, 215, 0);
    const float mid = krgb::lightmount::ansi_visual::kLayoutHeight / 2.0f;
    for(const auto& key : krgb::lightmount::ansi_visual::keys()) {
        const float cy = key.y + key.h / 2.0f;
        result.insert(QString::fromLatin1(key.name), cy < mid ? blue : yellow);
    }
    return result;
}

QHash<QString, QColor> lightMountUsaPreset() {
    QHash<QString, QColor> result;
    const QColor red(140, 22, 36);
    const QColor white(255, 255, 255);
    const QColor blue(28, 28, 75);
    const float blueRight = krgb::lightmount::ansi_visual::kLayoutWidth / 3.0f;
    const float stripeHeight = krgb::lightmount::ansi_visual::kLayoutHeight / 8.0f;

    for(const auto& key : krgb::lightmount::ansi_visual::keys()) {
        const float cx = key.x + key.w / 2.0f;
        const float cy = key.y + key.h / 2.0f;
        QColor color;
        if(cx < blueRight) {
            color = blue;
        } else {
            const int stripe = static_cast<int>(cy / stripeHeight);
            color = (stripe % 2 == 0) ? red : white;
        }
        result.insert(QString::fromLatin1(key.name), color);
    }
    return result;
}
} // namespace

KeyboardWidget::KeyboardWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(false);
    setMinimumSize(480, 150);
    setFocusPolicy(Qt::ClickFocus);
}

double KeyboardWidget::layoutWidth() const {
    if(layoutKind_ == LayoutKind::LogitechG810Iso105) {
        return krgb::logitech::g810_iso105_visual::kLayoutWidth;
    }
    if(layoutKind_ == LayoutKind::BeQuietLightMountAnsi) {
        return krgb::lightmount::ansi_visual::kLayoutWidth;
    }
    return krgb::kLayoutWidth;
}

double KeyboardWidget::layoutHeight() const {
    if(layoutKind_ == LayoutKind::LogitechG810Iso105) {
        return krgb::logitech::g810_iso105_visual::kLayoutHeight;
    }
    if(layoutKind_ == LayoutKind::BeQuietLightMountAnsi) {
        return krgb::lightmount::ansi_visual::kLayoutHeight;
    }
    return krgb::kLayoutHeight;
}

int KeyboardWidget::heightForWidth(int w) const {
    const double inner = w - 2 * kMargin;
    return static_cast<int>(inner * layoutHeight() / layoutWidth() + 2 * kMargin);
}

void KeyboardWidget::setLayoutKind(LayoutKind kind) {
    if(kind == layoutKind_) {
        return;
    }
    layoutKind_ = kind;
    selected_.clear();
    updateGeometry();
    update();
    Q_EMIT selectionChanged(0);
}

void KeyboardWidget::setKeyColors(const QHash<QString, QColor>& colors) {
    if(layoutKind_ == LayoutKind::BeQuietLightMountAnsi) {
        if(looksLikeUkrainePreset(colors)) {
            colors_ = lightMountUkrainePreset();
            update();
            return;
        }
        if(looksLikeUsaPreset(colors)) {
            colors_ = lightMountUsaPreset();
            update();
            return;
        }
    }
    colors_ = colors;
    update();
}

void KeyboardWidget::setModelBit(quint8 bit) {
    if(bit == kLightMountLayoutModelBit) {
        modelBit_ = bit;
        setLayoutKind(LayoutKind::BeQuietLightMountAnsi);
        return;
    }
    if(bit == modelBit_) {
        return;
    }
    modelBit_ = bit;
    selected_.clear();
    update();
    Q_EMIT selectionChanged(0);
}

QStringList KeyboardWidget::selectedKeys() const {
    QStringList list(selected_.cbegin(), selected_.cend());
    list.sort();
    return list;
}

void KeyboardWidget::recomputeLayout() {
    rects_.clear();
    const double sourceW = layoutWidth();
    const double sourceH = layoutHeight();
    const double availW = width() - 2 * kMargin;
    const double availH = height() - 2 * kMargin;
    const double unit = qMin(availW / sourceW, availH / sourceH);
    const double originX = (width() - unit * sourceW) / 2.0;
    const double originY = (height() - unit * sourceH) / 2.0;

    if(layoutKind_ == LayoutKind::LogitechG810Iso105) {
        const auto& elements = krgb::logitech::g810_iso105_visual::elements();
        rects_.reserve(static_cast<int>(elements.size()));
        for(const auto& element : elements) {
            const auto* def = krgb::logitech::g810_iso105_visual::definition(element);
            if(!def) continue;
            const QRectF cell(originX + element.x * unit + kKeyInset / 2.0,
                              originY + element.y * unit + kKeyInset / 2.0,
                              qMax(1.0, element.w * unit - kKeyInset),
                              qMax(1.0, element.h * unit - kKeyInset));
            const QString name = QString::fromLatin1(def->name);
            rects_.push_back({name, labelFor(name), cell, false});
        }
        return;
    }

    if(layoutKind_ == LayoutKind::BeQuietLightMountAnsi) {
        const auto& keys = krgb::lightmount::ansi_visual::keys();
        rects_.reserve(static_cast<int>(keys.size()));
        for(const auto& key : keys) {
            const QRectF cell(originX + key.x * unit + kKeyInset / 2.0,
                              originY + key.y * unit + kKeyInset / 2.0,
                              qMax(1.0, key.w * unit - kKeyInset),
                              qMax(1.0, key.h * unit - kKeyInset));
            const QString name = QString::fromLatin1(key.name);
            const QString label = QString::fromUtf8(key.label);
            rects_.push_back({name, label.isEmpty() ? labelFor(name) : label,
                              cell, key.round});
        }
        return;
    }

    rects_.reserve(static_cast<int>(krgb::kKeyCount));
    for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
        const krgb::KeyDef& k = krgb::kKeyMap[i];
        if(!(k.models & modelBit_)) continue;
        const QRectF cell(originX + k.x * unit + kKeyInset / 2.0,
                          originY + k.y * unit + kKeyInset / 2.0,
                          k.w * unit - kKeyInset,
                          k.h * unit - kKeyInset);
        const QString name = QString::fromLatin1(k.name);
        rects_.push_back({name, labelFor(name), cell, false});
    }
}

const QString KeyboardWidget::keyAt(const QPointF& p) const {
    for(const KeyRect& r : rects_) {
        if(r.round) {
            const QPointF center = r.cell.center();
            const double rx = r.cell.width() / 2.0;
            const double ry = r.cell.height() / 2.0;
            if(rx > 0.0 && ry > 0.0) {
                const double dx = (p.x() - center.x()) / rx;
                const double dy = (p.y() - center.y()) / ry;
                if(dx * dx + dy * dy <= 1.0) return r.name;
            }
        } else if(r.cell.contains(p)) {
            return r.name;
        }
    }
    return QString();
}

void KeyboardWidget::paintEvent(QPaintEvent*) {
    recomputeLayout();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), kBackground);

    const bool beQuiet = isBeQuietLayout(layoutKind_);

    for(const KeyRect& r : rects_) {
        const QColor fill = colors_.value(r.name, kUnset);
        const bool sel = selected_.contains(r.name);
        painter.setBrush(fill);
        painter.setPen(QPen(sel ? kSelect : QColor(0, 0, 0, 160), sel ? 2.0 : 1.0));
        if(r.round) {
            painter.drawEllipse(r.cell);
            const QRectF inner = r.cell.adjusted(r.cell.width() * 0.13,
                                                 r.cell.height() * 0.13,
                                                 -r.cell.width() * 0.13,
                                                 -r.cell.height() * 0.13);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(180, 180, 180, 90), 1.0));
            painter.drawEllipse(inner);
        } else {
            painter.drawRoundedRect(r.cell, beQuiet ? 2.0 : 3.0,
                                    beQuiet ? 2.0 : 3.0);
        }

        const double lum = 0.299 * fill.red() + 0.587 * fill.green() + 0.114 * fill.blue();
        const QColor textColor = lum > 140 ? QColor(20, 20, 20) : QColor(225, 225, 225);
        painter.setPen(textColor);
        if(r.cell.width() < 10.0 || r.cell.height() < 8.0) continue;

        if(beQuiet) {
            if(r.round) {
                drawBeQuietMuteMark(painter, r.cell, textColor);
                continue;
            }

            const qreal keySize = qMin(r.cell.width(), r.cell.height());
            BeQuietArrow arrow;
            if(dedicatedBeQuietCursor(r.name, arrow)) {
                // The physical Light Mount cursor legends are small filled
                // triangles printed in the upper-middle of each keycap rather
                // than large symbols centred on the whole key.
                const QPointF center(r.cell.center().x(),
                                     r.cell.top() + r.cell.height() * 0.39);
                drawBeQuietTriangle(painter, center, keySize,
                                    arrow, textColor, 0.26);
                continue;
            }

            if(r.name == QStringLiteral("SPACE")) {
                drawBeQuietSpaceMark(painter, r.cell, textColor);
                continue;
            }

            const QFont font = beQuietLegendFont(painter.font(), r.cell, r.label);
            painter.setFont(font);
            painter.drawText(beQuietLegendRect(r.cell),
                             Qt::AlignHCenter | Qt::AlignTop,
                             r.label);

            if(numpadBeQuietCursor(r.name, arrow)) {
                const QPointF center(r.cell.center().x(),
                                     r.cell.top() + r.cell.height() * 0.74);
                drawBeQuietTriangle(painter, center, keySize,
                                    arrow, textColor, 0.27);
            }
        } else {
            QFont font = painter.font();
            font.setPointSizeF(qMax(6.0, font.pointSizeF() - 1.0));
            painter.setFont(font);
            painter.drawText(r.cell, Qt::AlignCenter, r.label);
        }
    }
}

void KeyboardWidget::mousePressEvent(QMouseEvent* ev) {
    if(ev->button() != Qt::LeftButton) return;
    const bool additive = ev->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    pressPos_ = ev->position();
    dragging_ = true;
    moved_ = false;
    const QString hit = keyAt(pressPos_);
    if(!additive) {
        selected_.clear();
        if(!hit.isEmpty()) selected_.insert(hit);
    } else if(!hit.isEmpty()) {
        if(selected_.contains(hit)) selected_.remove(hit);
        else selected_.insert(hit);
    }
    baseSelection_ = selected_;
    update();
    Q_EMIT selectionChanged(selected_.size());
}

void KeyboardWidget::mouseMoveEvent(QMouseEvent* ev) {
    if(!dragging_) return;
    const QPointF p = ev->position();
    if(!moved_ && (p - pressPos_).manhattanLength() < 4) return;
    moved_ = true;
    rubber_ = QRectF(pressPos_, p).normalized();
    selected_ = baseSelection_;
    for(const KeyRect& r : rects_) {
        if(rubber_.intersects(r.cell)) selected_.insert(r.name);
    }
    update();
    Q_EMIT selectionChanged(selected_.size());
}

void KeyboardWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if(ev->button() != Qt::LeftButton) return;
    dragging_ = false;
    rubber_ = QRectF();
    update();
}

void KeyboardWidget::paintSelection(const QColor& color) {
    if(selected_.isEmpty() || !color.isValid()) return;
    for(const QString& name : selected_) colors_.insert(name, color);
    update();
    Q_EMIT changed();
}

void KeyboardWidget::clearSelection() {
    if(selected_.isEmpty()) return;
    for(const QString& name : selected_) colors_.remove(name);
    update();
    Q_EMIT changed();
}

void KeyboardWidget::selectAll() {
    selected_.clear();
    if(layoutKind_ == LayoutKind::LogitechG810Iso105) {
        for(const auto& element : krgb::logitech::g810_iso105_visual::elements()) {
            const auto* def = krgb::logitech::g810_iso105_visual::definition(element);
            if(def) selected_.insert(QString::fromLatin1(def->name));
        }
    } else if(layoutKind_ == LayoutKind::BeQuietLightMountAnsi) {
        for(const auto& key : krgb::lightmount::ansi_visual::keys()) {
            selected_.insert(QString::fromLatin1(key.name));
        }
    } else {
        for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
            if(krgb::kKeyMap[i].models & modelBit_)
                selected_.insert(QString::fromLatin1(krgb::kKeyMap[i].name));
        }
    }
    update();
    Q_EMIT selectionChanged(selected_.size());
}

void KeyboardWidget::fillAll(const QColor& color) {
    if(!color.isValid()) return;
    if(layoutKind_ == LayoutKind::LogitechG810Iso105) {
        for(const auto& element : krgb::logitech::g810_iso105_visual::elements()) {
            const auto* def = krgb::logitech::g810_iso105_visual::definition(element);
            if(def) colors_.insert(QString::fromLatin1(def->name), color);
        }
    } else if(layoutKind_ == LayoutKind::BeQuietLightMountAnsi) {
        for(const auto& key : krgb::lightmount::ansi_visual::keys()) {
            colors_.insert(QString::fromLatin1(key.name), color);
        }
    } else {
        for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
            if(krgb::kKeyMap[i].models & modelBit_)
                colors_.insert(QString::fromLatin1(krgb::kKeyMap[i].name), color);
        }
    }
    update();
    Q_EMIT changed();
}
