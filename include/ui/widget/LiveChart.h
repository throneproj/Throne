#pragma once

#include <QColor>
#include <QList>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>
#include <memory>

class QAction;

// A row is a measured thing (an outbound, a process), a column a measure (down/up, one value); each line is one row × column.
struct LiveChartRow {
    QString key;
    QString label;
    QString kind;
    // Invalid: resolved from ThemeTokens, chartSeries[paletteSlot] for 0..4, chartContext for -1.
    QColor color;
    int paletteSlot = 0;
    bool wash = false;
};

struct LiveChartColumn {
    QString key;
    QString label;
    QString arrow;
    bool dashed = false;
    bool wash = false;
};

struct LiveChartRange {
    int seconds = 60;
    QString label;
    int tickSeconds = 15;
};

struct LiveChartScale {
    double top = 1;
    double step = 1;
    std::function<QString(double)> label;
};

struct LiveChartAxis {
    std::function<QString(double)> format;
    std::function<LiveChartScale(double maxValue)> scale;
    // Widest strings format() can return, so value columns can reserve a fixed width.
    QStringList widest;

    static LiveChartAxis rateBytes();
    static LiveChartAxis rateBits();
    static LiveChartAxis bytes();
    static LiveChartAxis percent();
};

struct LiveChartOptions {
    enum class Legend { None, Side, Top };

    QList<LiveChartColumn> columns;
    Legend legend = Legend::None;
    // Empty: a fixed windowSeconds and no window control.
    QList<LiveChartRange> ranges;
    int windowSeconds = 120;
    int pointIntervalMs = 1000;
    int capacity = 600;
    bool crosshair = true;
    bool axisPills = true;
    bool markers = false;
    bool stoppedBands = false;
    bool timeAxis = true;
    bool totalInCard = true;
    bool contextMenu = false;
    // Transparent panel with the scale and caption inside it, so sibling charts keep identical plot geometry.
    bool compact = false;
    QString caption;
    LiveChartAxis axis;
    QString emptyTitle;
    QString emptyBody;

    static LiveChartOptions trafficGraph();
    static LiveChartOptions sparkline(const QString &caption, LiveChartAxis axis);
};

struct LiveChartState {
    QStringList hiddenRows;
    QStringList hiddenColumns;
    int range = 1;
    bool smooth = true;
    bool markers = true;

    bool operator==(const LiveChartState &) const = default;
};

// Timestamps are MonotonicMs() (include/global/Utils.hpp); labels convert them to wall time when painted.
class LiveChart : public QWidget {
    Q_OBJECT

public:
    enum class Marker { Started, Switched, Stopped };

    explicit LiveChart(LiveChartOptions options, QWidget *parent = nullptr);
    ~LiveChart() override;

    // Rows of the current source. Rows from earlier calls stay in the legend while they have data in the window.
    void setRows(const QList<LiveChartRow> &rows);
    // One value per current row × column, row-major; NaN where a line has no value. running=false draws a stopped band.
    void addPoint(qint64 monoMs, bool running, const QVector<double> &values);
    void addMarker(qint64 monoMs, Marker kind, const QString &text);
    // A non-empty title replaces the plot with the message; the buffer is kept.
    void setBlocked(const QString &title, const QString &body = {});
    void clearHistory();
    // Appended to the context menu after the built-in entries; the chart does not take ownership.
    void addMenuAction(QAction *action);

    LiveChartState state() const;
    void setState(const LiveChartState &state);

signals:
    void stateChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
