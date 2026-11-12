#pragma once

#include <QHeaderView>
#include <QPainter>
#include <QStyle>

// A static triangle past a top-aligned label; Qt's own sort indicator widens every section and its look varies per style and theme.
namespace HeaderSortArrow {
    // Reserved on each side of the sorted section's label only, so the arrow stays off a narrow column's text.
    constexpr int kRoom = 14;

    inline void paint(const QHeaderView *header, QPainter *painter, const QRect &rect, int logicalIndex, bool descending) {
        const QFontMetrics metrics(header->font());
        const QString label = header->model() ? header->model()->headerData(logicalIndex, header->orientation()).toString() : QString();
        const qreal x = qMin(rect.center().x() + metrics.horizontalAdvance(label) / 2.0 + 10, rect.right() - 6.0);
        const qreal y = rect.top() + header->style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, header) + metrics.height() / 2.0;
        const qreal tip = descending ? 2.5 : -2.5;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(header->palette().color(QPalette::Highlight));
        painter->drawPolygon(QPolygonF{{x - 4, y - tip}, {x + 4, y - tip}, {x, y + tip}});
        painter->restore();
    }
}
