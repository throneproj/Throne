#include "include/ui/widget/ElidingLabel.hpp"

#include <QHelpEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>
#include <QTextDocument>
#include <QToolTip>

QSize ElidingLabel::minimumSizeHint() const {
    QSize size = QLabel::minimumSizeHint();
    const QMargins margins = contentsMargins();
    const int ellipsisWidth = fontMetrics().horizontalAdvance(QChar(0x2026)) + margins.left() + margins.right() + 2 * margin();
    size.setWidth(qMin(size.width(), ellipsisWidth));
    return size;
}

QRect ElidingLabel::textRect() const {
    return contentsRect().adjusted(margin(), margin(), -margin(), -margin());
}

QString ElidingLabel::fittedText(bool *elided) const {
    *elided = false;
    const bool plain = textFormat() == Qt::PlainText || (textFormat() == Qt::AutoText && !Qt::mightBeRichText(text()));
    if (!plain) return text();
    const int width = textRect().width();
    const QFontMetrics metrics = fontMetrics();
    QStringList lines = text().split(QLatin1Char('\n'));
    for (auto &line : lines) {
        const QString fitted = metrics.elidedText(line, Qt::ElideRight, width);
        if (fitted != line) {
            line = fitted;
            *elided = true;
        }
    }
    return lines.join(QLatin1Char('\n'));
}

bool ElidingLabel::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip && toolTip().isEmpty()) {
        bool elided = false;
        fittedText(&elided);
        if (elided) {
            QToolTip::showText(static_cast<QHelpEvent *>(event)->globalPos(), text(), this);
            return true;
        }
    }
    return QLabel::event(event);
}

void ElidingLabel::paintEvent(QPaintEvent *event) {
    bool elided = false;
    const QString shown = fittedText(&elided);
    if (!elided) {
        QLabel::paintEvent(event);
        return;
    }
    QPainter painter(this);
    drawFrame(&painter);
    QStyleOption option;
    option.initFrom(this);
    const auto direction = text().isRightToLeft() ? Qt::RightToLeft : Qt::LeftToRight;
    const int flags = QStyle::visualAlignment(direction, alignment())
                      | (direction == Qt::LeftToRight ? Qt::TextForceLeftToRight : Qt::TextForceRightToLeft);
    style()->drawItemText(&painter, textRect(), flags, option.palette, isEnabled(), shown, foregroundRole());
}
