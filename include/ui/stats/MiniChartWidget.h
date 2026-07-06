#pragma once

#include <QWidget>
#include <QVector>
#include <QColor>

#include <functional>

// A minimal self-painted sparkline for the runtime panel: up to two overlaid
// series over a fixed-size rolling window, auto-scaled to a "nice" ceiling above
// the visible peak. All text is drawn *inside* the panel so the plot geometry is
// identical across charts (no external caption or variable-width gutter to knock
// them out of alignment): the scale ceiling sits top-left, 0 bottom-left, and an
// optional caption (e.g. "CPU"/"RAM") bottom-right, with a dot on each series'
// latest sample — so a value can actually be read off the chart. A formatter
// turns raw values into label text (e.g. "%", or a byte size). Thin antialiased
// lines with a faint fill under the primary series, on a subtle rounded backdrop.
// Theme-aware (colours default from the palette, overridable to match a legend).
// Qt Charts isn't a dependency, so this is drawn directly with QPainter (like
// TrafficChartWidget).
class MiniChartWidget : public QWidget {
    Q_OBJECT

public:
    explicit MiniChartWidget(QWidget* parent = nullptr);

    // Rolling-window capacity (older samples fall off the left). Default 60.
    void setCapacity(int n);
    // Series colours; pass an invalid QColor to keep the palette-derived default.
    void setColors(const QColor& primary, const QColor& secondary);
    // Fixed vertical max; <= 0 (default) auto-scales to a nice ceiling.
    void setMaxValue(double m);
    // Formats a raw value for the scale labels (default: plain number).
    void setFormatter(std::function<QString(double)> formatter);
    // Caption drawn inside the panel, bottom-right (e.g. "CPU", "RAM").
    void setCaption(const QString& caption);
    // Append one sample per series and repaint.
    void push(double primary, double secondary);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<double> a_;
    QVector<double> b_;
    int cap_ = 60;
    double fixedMax_ = -1.0;
    QColor primary_;
    QColor secondary_;
    std::function<QString(double)> formatter_;
    QString caption_;
};
