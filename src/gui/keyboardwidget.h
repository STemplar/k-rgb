// KeyboardWidget — graphical keyboard layout for editing per-key colours.
//
// Draws the active keyboard in its real position. Click a key to select it,
// drag to rubber-band a group, Ctrl-click to add/remove. The current selection
// can then be painted a colour. Keys keep their assigned colour as a swatch;
// unassigned keys render dark.
#pragma once

#include <QColor>
#include <QHash>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QVector>
#include <QWidget>

class KeyboardWidget : public QWidget {
    Q_OBJECT
public:
    enum class LayoutKind {
        Alienware,
        LogitechG810Iso105,
        BeQuietLightMountAnsi,
    };

    explicit KeyboardWidget(QWidget* parent = nullptr);

    void setLayoutKind(LayoutKind kind);

    // Only keys that have an assigned (non-default) colour.
    QHash<QString, QColor> keyColors() const { return colors_; }
    void                   setKeyColors(const QHash<QString, QColor>& colors);

    // Restrict the drawn/selectable Alienware keys to those a model has (bit
    // from KeyboardModel::bit; 0xFF = all keys). Other layouts ignore it.
    void setModelBit(quint8 bit);

    QStringList selectedKeys() const;

public Q_SLOTS:
    void paintSelection(const QColor& color);  // colour the current selection
    void clearSelection();                     // off (black) the current selection
    void selectAll();
    void fillAll(const QColor& color);         // colour every key

Q_SIGNALS:
    void changed();                 // colours changed (re-apply to hardware)
    void selectionChanged(int count);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    QSize sizeHint() const override { return QSize(640, 200); }
    int   heightForWidth(int w) const override;
    bool  hasHeightForWidth() const override { return true; }

private:
    struct KeyRect {
        QString name;
        QString label;
        QRectF  cell;  // pixel rect, recomputed on resize
        bool    round = false;
    };

    void          recomputeLayout();
    double        layoutWidth() const;
    double        layoutHeight() const;
    const QString keyAt(const QPointF& p) const;

    QHash<QString, QColor> colors_;     // assigned colours only
    QSet<QString>          selected_;
    QVector<KeyRect>       rects_;
    QRectF                 chassisRect_;

    LayoutKind    layoutKind_ = LayoutKind::Alienware;
    quint8        modelBit_   = 0xFF;  // Alienware KeyboardModel::bit
    bool          dragging_   = false;
    bool          moved_      = false;
    QPointF       pressPos_;
    QRectF        rubber_;
    QSet<QString> baseSelection_;  // selection at drag start (for additive drags)
};
