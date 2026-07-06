#include "include/ui/stats/MiniChartWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QFontMetricsF>

#include <cmath>

namespace {
    // Round a positive value up to a "nice" 1/2/5 * 10^n ceiling, so the scale
    // label is stable and readable instead of jittering with every sample.
    double niceCeil(double v) {
        if (v <= 0) return 1.0;
        const double mag = std::pow(10.0, std::floor(std::log10(v)));
        const double n = v / mag;
        double nice;
        if (n <= 1.0) nice = 1.0;
        else if (n <= 2.0) nice = 2.0;
        else if (n <= 5.0) nice = 5.0;
        else nice = 10.0;
        return nice * mag;
    }
}

MiniChartWidget::MiniChartWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(46);
    // Expand into whatever height the card gives it, so the process card fills
    // instead of leaving dead space.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void MiniChartWidget::setCapacity(int n) {
    cap_ = n > 1 ? n : 1;
    while (a_.size() > cap_) a_.removeFirst();
    while (b_.size() > cap_) b_.removeFirst();
    update();
}

void MiniChartWidget::setColors(const QColor& primary, const QColor& secondary) {
    primary_ = primary;
    secondary_ = secondary;
    update();
}

void MiniChartWidget::setMaxValue(double m) {
    fixedMax_ = m;
    update();
}

void MiniChartWidget::setFormatter(std::function<QString(double)> formatter) {
    formatter_ = std::move(formatter);
    update();
}

void MiniChartWidget::setCaption(const QString& caption) {
    caption_ = caption;
    update();
}

void MiniChartWidget::push(double primary, double secondary) {
    a_.append(primary);
    b_.append(secondary);
    while (a_.size() > cap_) a_.removeFirst();
    while (b_.size() > cap_) b_.removeFirst();
    update();
}

void MiniChartWidget::clear() {
    a_.clear();
    b_.clear();
    update();
}

void MiniChartWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor textColor = palette().color(QPalette::WindowText);
    const QColor primary = primary_.isValid() ? primary_ : palette().color(QPalette::Highlight);
    QColor secondary = secondary_;
    if (!secondary.isValid()) {
        secondary = textColor;
        secondary.setAlpha(120);
    }

    // Vertical scale: fixed if requested, otherwise a nice ceiling above the peak.
    double maxV = fixedMax_;
    if (maxV <= 0) {
        double peak = 0;
        for (const double v : a_) peak = qMax(peak, v);
        for (const double v : b_) peak = qMax(peak, v);
        maxV = niceCeil(peak);
    }
    if (maxV <= 0) maxV = 1.0;

    // The whole widget is the panel. Labels live in top/bottom strips *inside* it,
    // so the data band keeps a fixed geometry no matter how wide the labels are —
    // that's what keeps sibling charts aligned to the same length.
    const QRectF panel = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
    if (panel.width() <= 2 || panel.height() <= 2) return;

    QColor bg = textColor;
    bg.setAlpha(12);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(panel, 4, 4);

    QFont labelFont = font();
    labelFont.setPointSizeF(qMax(6.5, labelFont.pointSizeF() - 1.5));
    const QFontMetricsF fm(labelFont);
    const double strip = qMin(fm.height(), panel.height() / 3.0);
    const double padX = 5.0;

    const QRectF plot(panel.left() + padX, panel.top() + strip,
                      panel.width() - 2 * padX, panel.height() - 2 * strip);
    if (plot.width() <= 1 || plot.height() <= 1) return;

    const double stepX = plot.width() / static_cast<double>(cap_ - 1 > 0 ? cap_ - 1 : 1);

    auto drawSeries = [&](const QVector<double>& s, const QColor& color, bool fill) {
        if (s.isEmpty()) return;
        const int n = s.size();
        // Newest sample hugs the right edge; older samples extend left.
        const auto pointAt = [&](int i) {
            const double x = plot.right() - stepX * (n - 1 - i);
            const double y = plot.bottom() - qBound(0.0, s[i] / maxV, 1.0) * plot.height();
            return QPointF(x, y);
        };
        if (n >= 2) {
            QPainterPath line(pointAt(0));
            for (int i = 1; i < n; ++i) line.lineTo(pointAt(i));
            if (fill) {
                QPainterPath poly(line);
                poly.lineTo(pointAt(n - 1).x(), plot.bottom());
                poly.lineTo(pointAt(0).x(), plot.bottom());
                poly.closeSubpath();
                QColor fillColor = color;
                fillColor.setAlpha(36);
                p.setPen(Qt::NoPen);
                p.setBrush(fillColor);
                p.drawPath(poly);
            }
            QPen pen(color, 1.6);
            pen.setJoinStyle(Qt::RoundJoin);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(line);
        }
        // Dot on the latest sample so the current level is easy to spot.
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(pointAt(n - 1), 2.2, 2.2);
    };

    // Secondary first so the primary line stays on top and legible.
    drawSeries(b_, secondary, false);
    drawSeries(a_, primary, true);

    // Labels last, so they stay legible over the series: scale ceiling top-left,
    // 0 bottom-left, caption (metric name) bottom-right.
    p.setFont(labelFont);
    QColor scaleColor = textColor;
    scaleColor.setAlpha(150);
    p.setPen(scaleColor);
    const QRectF topStrip(panel.left() + padX, panel.top(), panel.width() - 2 * padX, strip);
    const QRectF botStrip(panel.left() + padX, panel.bottom() - strip, panel.width() - 2 * padX, strip);
    const QString topLabel = formatter_ ? formatter_(maxV) : QString::number(maxV);
    p.drawText(topStrip, Qt::AlignLeft | Qt::AlignVCenter, topLabel);
    p.drawText(botStrip, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("0"));

    if (!caption_.isEmpty()) {
        QColor capColor = textColor;
        capColor.setAlpha(190);
        p.setPen(capColor);
        QFont capFont = labelFont;
        capFont.setBold(true);
        p.setFont(capFont);
        p.drawText(botStrip, Qt::AlignRight | Qt::AlignVCenter, caption_);
    }
}
