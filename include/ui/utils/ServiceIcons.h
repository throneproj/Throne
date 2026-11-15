#pragma once

#include <QColor>
#include <QList>
#include <QMetaType>
#include <QPixmap>
#include <QRect>
#include <QString>
#include <QWidget>

class QPainter;

// Service Check logos, drawn from SVG path data, with a badge for the check's outcome.
namespace ServiceIcons {
    enum class State {
        Plain,    // the logo alone, as in the settings list
        Access,
        Partial,
        NoAccess,
        Unknown,  // inconclusive or failed: a greyed logo without a badge
    };

    // A stored check status (available, partial, restricted, blocked, unknown, failed) as an icon state.
    [[nodiscard]] State StateOf(const QString &status);

    // `id` is the check id; an id without a logo gets a letter tile from `name`.
    // `foreground` paints the monochrome logos (OpenAI, TikTok, Wikipedia, Steam); badges use the theme's tokens.
    [[nodiscard]] QPixmap Pixmap(const QString &id, const QString &name, State state, int size, qreal devicePixelRatio,
                                 const QColor &foreground);

    struct Item {
        QString id;
        QString name;
        State state = State::Unknown;
        QString toolTip;
    };

    constexpr int kSpacing = 4;

    [[nodiscard]] int StripWidth(qsizetype count, int size);

    // Where PaintStrip puts `count` icons, for hit tests without a painter.
    [[nodiscard]] QList<QRect> StripRects(const QRect &area, qsizetype count, int size);

    // Draws `items` left to right from area's left edge, vertically centered; returns each icon's rect, in order.
    QList<QRect> PaintStrip(QPainter *painter, const QRect &area, const QList<Item> &items, int size, const QColor &foreground);
} // namespace ServiceIcons

Q_DECLARE_METATYPE(QList<ServiceIcons::Item>)

// A row of service icons, each with its own tooltip.
class ServiceIconStrip : public QWidget {
    Q_OBJECT

public:
    explicit ServiceIconStrip(QWidget *parent = nullptr);

    void setItems(const QList<ServiceIcons::Item> &items);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    QList<ServiceIcons::Item> items_;
    QList<QRect> rects_;
};
