#include "keyboardwidget.h"

#include "core/keymap.h"
#include "core/logitech_g810_iso105_visual.h"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

namespace {
constexpr double kMargin   = 6.0;   // px around the whole layout
constexpr double kKeyInset = 2.0;   // px gap between adjacent keys
const QColor     kUnset(45, 45, 48);
const QColor     kBackground(28, 28, 30);
const QColor     kSelect(80, 170, 255);

// Friendlier glyphs for a few keys whose names are verbose.
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
        {QStringLiteral("MUTE"), QStringLiteral("\U0001f507")},
        {QStringLiteral("VOLDN"), QStringLiteral("\U0001f509")},
        {QStringLiteral("VOLUP"), QStringLiteral("\U0001f50a")},
    };
    const auto it = pretty.constFind(name);
    if(it != pretty.constEnd()) {
        return it.value();
    }
    if(name.startsWith(QStringLiteral("NP"))) {
        return name.mid(2);  // numpad: show just the glyph
    }
    return name;
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
    return krgb::kLayoutWidth;
}

double KeyboardWidget::layoutHeight() const {
    if(layoutKind_ == LayoutKind::LogitechG810Iso105) {
        return krgb::logitech::g810_iso105_visual::kLayoutHeight;
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
    colors_ = colors;
    update();
}

void KeyboardWidget::setModelBit(quint8 bit) {
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

    // Centre the layout within the widget.
    const double originX = (width() - unit * sourceW) / 2.0;
    const double originY = (height() - unit * sourceH) / 2.0;

    if(layoutKind_ == LayoutKind::LogitechG810Iso105) {
        const auto& elements = krgb::logitech::g810_iso105_visual::elements();
        rects_.reserve(static_cast<int>(elements.size()));
        for(const auto& element : elements) {
            const auto* def =
                krgb::logitech::g810_iso105_visual::definition(element);
            if(!def) {
                continue;
            }

            const QRectF cell(originX + element.x * unit + kKeyInset / 2.0,
                              originY + element.y * unit + kKeyInset / 2.0,
                              qMax(1.0, element.w * unit - kKeyInset),
                              qMax(1.0, element.h * unit - kKeyInset));
            const QString name = QString::fromLatin1(def->name);
            rects_.push_back({name, labelFor(name), cell});
        }
        return;
    }

    rects_.reserve(static_cast<int>(krgb::kKeyCount));
    for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
        const krgb::KeyDef& k = krgb::kKeyMap[i];
        if(!(k.models & modelBit_)) {
            continue;  // key absent on the active model
        }
        const QRectF cell(originX + k.x * unit + kKeyInset / 2.0,
                          originY + k.y * unit + kKeyInset / 2.0,
                          k.w * unit - kKeyInset,
                          k.h * unit - kKeyInset);
        const QString name = QString::fromLatin1(k.name);
        rects_.push_back({name, labelFor(name), cell});
    }
}

const QString KeyboardWidget::keyAt(const QPointF& p) const {
    for(const KeyRect& r : rects_) {
        if(r.cell.contains(p)) {
            return r.name;
        }
    }
    return QString();
}

void KeyboardWidget::paintEvent(QPaintEvent*) {
    recomputeLayout();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), kBackground);

    QFont font = painter.font();
    font.setPointSizeF(qMax(6.0, font.pointSizeF() - 1.0));
    painter.setFont(font);

    for(const KeyRect& r : rects_) {
        const QColor fill = colors_.value(r.name, kUnset);
        const bool sel = selected_.contains(r.name);

        painter.setBrush(fill);
        painter.setPen(QPen(sel ? kSelect : QColor(0, 0, 0, 160), sel ? 2.0 : 1.0));
        painter.drawRoundedRect(r.cell, 3.0, 3.0);

        // Label colour contrasts with the key fill.
        const double lum = 0.299 * fill.red() + 0.587 * fill.green() + 0.114 * fill.blue();
        painter.setPen(lum > 140 ? QColor(20, 20, 20) : QColor(225, 225, 225));
        if(r.cell.width() >= 10.0 && r.cell.height() >= 8.0) {
            painter.drawText(r.cell, Qt::AlignCenter, r.label);
        }
    }
}

void KeyboardWidget::mousePressEvent(QMouseEvent* ev) {
    if(ev->button() != Qt::LeftButton) {
        return;
    }
    const bool additive = ev->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier);
    pressPos_  = ev->position();
    dragging_  = true;
    moved_     = false;

    const QString hit = keyAt(pressPos_);
    if(!additive) {
        selected_.clear();
        if(!hit.isEmpty()) {
            selected_.insert(hit);
        }
    } else if(!hit.isEmpty()) {
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

void KeyboardWidget::mouseMoveEvent(QMouseEvent* ev) {
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
    for(const KeyRect& r : rects_) {
        if(rubber_.intersects(r.cell)) {
            selected_.insert(r.name);
        }
    }
    update();
    Q_EMIT selectionChanged(selected_.size());
}

void KeyboardWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if(ev->button() != Qt::LeftButton) {
        return;
    }
    dragging_ = false;
    rubber_   = QRectF();
    update();
}

void KeyboardWidget::paintSelection(const QColor& color) {
    if(selected_.isEmpty() || !color.isValid()) {
        return;
    }
    for(const QString& name : selected_) {
        colors_.insert(name, color);
    }
    update();
    Q_EMIT changed();
}

void KeyboardWidget::clearSelection() {
    if(selected_.isEmpty()) {
        return;
    }
    for(const QString& name : selected_) {
        colors_.remove(name);  // unassigned == off
    }
    update();
    Q_EMIT changed();
}

void KeyboardWidget::selectAll() {
    selected_.clear();

    if(layoutKind_ == LayoutKind::LogitechG810Iso105) {
        for(const auto& element : krgb::logitech::g810_iso105_visual::elements()) {
            const auto* def =
                krgb::logitech::g810_iso105_visual::definition(element);
            if(def) {
                selected_.insert(QString::fromLatin1(def->name));
            }
        }
    } else {
        for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
            if(krgb::kKeyMap[i].models & modelBit_) {
                selected_.insert(QString::fromLatin1(krgb::kKeyMap[i].name));
            }
        }
    }

    update();
    Q_EMIT selectionChanged(selected_.size());
}

void KeyboardWidget::fillAll(const QColor& color) {
    if(!color.isValid()) {
        return;
    }

    if(layoutKind_ == LayoutKind::LogitechG810Iso105) {
        for(const auto& element : krgb::logitech::g810_iso105_visual::elements()) {
            const auto* def =
                krgb::logitech::g810_iso105_visual::definition(element);
            if(def) {
                colors_.insert(QString::fromLatin1(def->name), color);
            }
        }
    } else {
        for(std::size_t i = 0; i < krgb::kKeyCount; ++i) {
            if(krgb::kKeyMap[i].models & modelBit_) {
                colors_.insert(QString::fromLatin1(krgb::kKeyMap[i].name), color);
            }
        }
    }

    update();
    Q_EMIT changed();
}
