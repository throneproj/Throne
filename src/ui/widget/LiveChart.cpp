#include "include/ui/widget/LiveChart.h"

#include "include/global/Utils.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/widget/SeriesBuffer.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QRegion>
#include <QTextDocument>
#include <QTimer>
#include <QToolTip>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace {
    constexpr qreal kLiveChartSideLegendMinWidth = 700;
    constexpr qreal kLiveChartDimmedOpacity = 0.22;
    constexpr float kLiveChartNaN = std::numeric_limits<float>::quiet_NaN();

    enum LiveChartAlign { kLiveChartLeft, kLiveChartCenter, kLiveChartRight };

    double liveChartNiceStep(double x) {
        if (!(x > 0)) return 1;
        const double m = std::pow(10.0, std::floor(std::log10(x)));
        const double n = x / m;
        return (n <= 1 ? 1 : n <= 2 ? 2 : n <= 2.5 ? 2.5 : n <= 5 ? 5 : 10) * m;
    }

    QString liveChartTrimmed(double v, int decimals) {
        QString s = QString::number(v, 'f', decimals);
        if (s.contains(QLatin1Char('.'))) {
            while (s.endsWith(QLatin1Char('0'))) s.chop(1);
            if (s.endsWith(QLatin1Char('.'))) s.chop(1);
        }
        return s;
    }

    LiveChartAxis liveChartUnitAxis(double base, const QStringList &units, double mult, double floorValue, const QStringList &widest) {
        LiveChartAxis axis;
        axis.widest = widest;
        const int last = int(units.size()) - 1;
        axis.format = [=](double v) {
            double x = v * mult;
            int u = 0;
            while (x >= base && u < last) {
                x /= base;
                ++u;
            }
            const int decimals = u == 0 || x >= 100 ? 0 : x >= 10 ? 1 : 2;
            return QString::number(x, 'f', decimals) + QLatin1Char(' ') + units[u];
        };
        axis.scale = [=](double maxValue) {
            double v = std::max(maxValue * mult, floorValue);
            int u = 0;
            while (v >= base && u < last) {
                v /= base;
                ++u;
            }
            for (;;) {
                const double step = liveChartNiceStep(v / 4);
                const double top = std::ceil(v / step - 1e-9) * step;
                if (top >= 1000 && u < last) {
                    v /= base;
                    ++u;
                    continue;
                }
                const double f = std::pow(base, u) / mult;
                const QString unit = units[u];
                return LiveChartScale{top * f, step * f, [f, unit](double g) { return liveChartTrimmed(g / f, 2) + QLatin1Char(' ') + unit; }};
            }
        };
        return axis;
    }

    qint64 liveChartFloorDiv(qint64 a, qint64 b) {
        const qint64 q = a / b;
        return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
    }

    qreal liveChartClamp(qreal v, qreal lo, qreal hi) {
        return std::max(lo, std::min(hi, v));
    }

    QRect liveChartInflated(const QRectF &r) {
        return r.toAlignedRect().adjusted(-2, -2, 2, 2);
    }

    QColor liveChartAlpha(QColor color, int alpha) {
        color.setAlpha(alpha);
        return color;
    }

    QColor liveChartFaded(QColor color, qreal factor) {
        color.setAlphaF(color.alphaF() * factor);
        return color;
    }

    QString liveChartClock(qint64 wallMs, bool seconds) {
        return QDateTime::fromMSecsSinceEpoch(wallMs).toString(seconds ? QStringLiteral("HH:mm:ss") : QStringLiteral("HH:mm"));
    }

    void liveChartText(QPainter &p, const QFontMetricsF &fm, const QString &text, qreal x, qreal yMid, int align) {
        if (align != kLiveChartLeft) {
            const qreal w = fm.horizontalAdvance(text);
            x -= align == kLiveChartCenter ? w / 2 : w;
        }
        p.drawText(QPointF(x, yMid + (fm.ascent() - fm.descent()) / 2), text);
    }

    void liveChartHalo(QPainter &p, const QFontMetricsF &fm, const QString &text, qreal x, qreal yMid, const QColor &color, const QColor &halo, int align) {
        if (align != kLiveChartLeft) {
            const qreal w = fm.horizontalAdvance(text);
            x -= align == kLiveChartCenter ? w / 2 : w;
        }
        const QPointF at(x, yMid + (fm.ascent() - fm.descent()) / 2);
        p.setPen(halo);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx || dy) p.drawText(at + QPointF(dx, dy), text);
            }
        }
        p.setPen(color);
        p.drawText(at, text);
    }

    void liveChartDot(QPainter &p, const QPointF &center, qreal r, const QColor &color, const QColor &ring) {
        p.setPen(Qt::NoPen);
        p.setBrush(ring);
        p.drawEllipse(center, r + 2, r + 2);
        p.setBrush(color);
        p.drawEllipse(center, r, r);
    }

    void liveChartLegendDot(QPainter &p, const QPointF &center, const QColor &color, bool filled) {
        if (filled) {
            p.setPen(Qt::NoPen);
            p.setBrush(color);
        } else {
            p.setPen(QPen(color, 1.5));
            p.setBrush(Qt::NoBrush);
        }
        p.drawEllipse(center, 4.5, 4.5);
    }

    void liveChartLineKey(QPainter &p, qreal x0, qreal x1, qreal y, const QColor &color, bool dashed) {
        const qreal width = dashed ? 1.5 : 2;
        QPen pen(color, width);
        pen.setCapStyle(Qt::FlatCap);
        if (dashed) pen.setDashPattern({4 / width, 3 / width});
        p.setPen(pen);
        p.drawLine(QPointF(x0, y), QPointF(x1, y));
    }

    // Fritsch–Carlson monotone cubic: clamped tangents never overshoot a point, so the curve stays within each pair's range.
    void liveChartTracePath(QPainterPath &path, const std::vector<qreal> &xs, const std::vector<qreal> &ys, bool smooth,
                            std::vector<qreal> &d, std::vector<qreal> &m) {
        const size_t n = xs.size();
        path.moveTo(xs[0], ys[0]);
        if (n < 3 || !smooth) {
            for (size_t i = 1; i < n; ++i) path.lineTo(xs[i], ys[i]);
            return;
        }
        d.resize(n - 1);
        m.resize(n);
        for (size_t i = 0; i + 1 < n; ++i) {
            const qreal dx = xs[i + 1] - xs[i];
            d[i] = (ys[i + 1] - ys[i]) / (dx != 0 ? dx : 1);
        }
        m[0] = d[0];
        m[n - 1] = d[n - 2];
        for (size_t i = 1; i + 1 < n; ++i) m[i] = d[i - 1] * d[i] <= 0 ? 0 : (d[i - 1] + d[i]) / 2;
        for (size_t i = 0; i + 1 < n; ++i) {
            if (d[i] == 0) {
                m[i] = 0;
                m[i + 1] = 0;
                continue;
            }
            const qreal a = m[i] / d[i], b = m[i + 1] / d[i], s = a * a + b * b;
            if (s > 9) {
                const qreal t = 3 / std::sqrt(s);
                m[i] = t * a * d[i];
                m[i + 1] = t * b * d[i];
            }
        }
        for (size_t i = 0; i + 1 < n; ++i) {
            const qreal h = (xs[i + 1] - xs[i]) / 3;
            path.cubicTo(xs[i] + h, ys[i] + m[i] * h, xs[i + 1] - h, ys[i + 1] - m[i + 1] * h, xs[i + 1], ys[i + 1]);
        }
    }
} // namespace

LiveChartAxis LiveChartAxis::rateBytes() {
    return liveChartUnitAxis(1024, {QStringLiteral("B/s"), QStringLiteral("KiB/s"), QStringLiteral("MiB/s"), QStringLiteral("GiB/s")}, 1, 1024,
                             {QStringLiteral("1000 KiB/s"), QStringLiteral("8.88 MiB/s")});
}

LiveChartAxis LiveChartAxis::rateBits() {
    return liveChartUnitAxis(1000, {QStringLiteral("bps"), QStringLiteral("Kbps"), QStringLiteral("Mbps"), QStringLiteral("Gbps")}, 8, 1000,
                             {QStringLiteral("1000 Kbps"), QStringLiteral("8.88 Mbps")});
}

LiveChartAxis LiveChartAxis::bytes() {
    return liveChartUnitAxis(1024, {QStringLiteral("B"), QStringLiteral("KiB"), QStringLiteral("MiB"), QStringLiteral("GiB")}, 1, 1024 * 1024,
                             {QStringLiteral("1000 KiB"), QStringLiteral("888 MiB")});
}

LiveChartAxis LiveChartAxis::percent() {
    LiveChartAxis axis;
    axis.widest = {QStringLiteral("88.8%"), QStringLiteral("100%")};
    axis.format = [](double v) { return QString::number(v, 'f', v < 10 ? 1 : 0) + QLatin1Char('%'); };
    axis.scale = [](double maxValue) {
        const double v = std::max(maxValue, 5.0);
        const double step = liveChartNiceStep(v / 4);
        return LiveChartScale{std::min(100.0, std::ceil(v / step - 1e-9) * step), step,
                              [](double g) { return liveChartTrimmed(g, 1) + QLatin1Char('%'); }};
    };
    return axis;
}

LiveChartOptions LiveChartOptions::trafficGraph() {
    LiveChartOptions o;
    o.columns = {
        LiveChartColumn{QStringLiteral("d"), LiveChart::tr("Down"), QStringLiteral("↓"), false, true},
        LiveChartColumn{QStringLiteral("u"), LiveChart::tr("Up"), QStringLiteral("↑"), true, false},
    };
    o.legend = Legend::Side;
    o.ranges = {
        LiveChartRange{60, QStringLiteral("1m"), 15},
        LiveChartRange{120, QStringLiteral("2m"), 30},
        LiveChartRange{300, QStringLiteral("5m"), 60},
        LiveChartRange{600, QStringLiteral("10m"), 120},
    };
    o.windowSeconds = 120;
    o.pointIntervalMs = 1000;
    o.capacity = 600;
    o.crosshair = true;
    o.axisPills = true;
    o.markers = true;
    o.stoppedBands = true;
    o.timeAxis = true;
    o.totalInCard = true;
    o.contextMenu = true;
    o.axis = LiveChartAxis::rateBytes();
    o.emptyTitle = LiveChart::tr("Waiting for traffic");
    o.emptyBody = LiveChart::tr("The graph fills in once a second while a profile is running.");
    return o;
}

LiveChartOptions LiveChartOptions::sparkline(const QString &caption, LiveChartAxis axis) {
    LiveChartOptions o;
    o.columns = {LiveChartColumn{QStringLiteral("v"), {}, {}, false, true}};
    o.legend = Legend::None;
    o.windowSeconds = 60;
    o.pointIntervalMs = 1000;
    o.capacity = 64;
    o.crosshair = true;
    o.axisPills = false;
    o.markers = false;
    o.stoppedBands = false;
    o.timeAxis = false;
    o.totalInCard = false;
    o.contextMenu = false;
    o.compact = true;
    o.caption = caption;
    o.axis = std::move(axis);
    return o;
}

struct LiveChart::Impl {
    enum HitType { NoHit, RowHit, ColumnHit, RangeHit };
    enum MessageKind { NoMessage, BlockedMessage, EmptyMessage, HiddenMessage };

    struct Hit {
        HitType type = NoHit;
        int index = -1;
        QRectF rect;
    };

    struct RowEntry {
        LiveChartRow row;
        QString shortName;
        QColor color;
        QVector<int> columns;
        qreal nameWidth = 0;
        qreal kindWidth = 0;
        qreal cardNameWidth = 0;
        qreal cardKindWidth = 0;
        bool current = false;
    };

    struct Series {
        int reg = 0;
        int legendPos = 0;
        int column = 0;
        int bufferColumn = -1;
        bool on = false;
    };

    struct Bucket {
        qint64 key = 0;
        qint64 from = 0;
        qint64 to = 0;
        qint64 tx = 0;
        qreal x = 0;
        int n = 0;
        bool run = false;
        bool brk = false;
    };

    struct Anchor {
        bool set = false;
        qint64 t = 0;
        int reg = -1;
        int column = -1;

        bool operator==(const Anchor &) const = default;
    };

    struct SideRow {
        int reg = 0;
        QRectF rect;
        int values = 0;
    };

    struct GridLine {
        qreal y = 0;
        bool baseline = false;
        qreal labelMid = 0;
        QString label;
        QRectF labelRect;
    };

    struct Band {
        QRectF rect;
        bool labeled = false;
    };

    struct MarkerItem {
        qreal x = 0;
        bool flagged = false;
        QRectF flag;
        QString text;
    };

    struct AxisLabel {
        qreal x = 0;
        qreal width = 0;
        QString text;
        QRectF rect;
        bool base = false;
    };

    struct Dot {
        QPointF center;
        qreal r = 0;
        QColor color;
    };

    struct CardLine {
        enum Type { Row, Total, Note };
        Type type = Note;
        int reg = -1;
        bool focus = false;
        QString text;
        int values = -1;
    };

    struct Colors {
        QColor base, ink, ink2, muted, grid, axis, border, hairline, eventLine, stoppedFill, hatch, hover, selected, panel, label, caption;
    };

    struct View {
        bool panelValid = false;
        bool plotValid = false;
        bool valid = false;
        bool allHidden = false;
        MessageKind message = NoMessage;
        LiveChartOptions::Legend legend = LiveChartOptions::Legend::None;
        QRectF panel, plot, topStrip, bottomStrip, rangeRect;
        qreal sideX0 = 0, sideNameW = 0, sideColW = 0;
        qint64 start = 0, end = 0, span = 1, kms = 1, wallOffset = 0;
        double top = 1;
        QString ceiling;
        QString messageTitle;
    };

    struct Overlay {
        bool active = false;
        int focus = -1;
        qreal cx = 0, cy = 0, hy = 0;
        std::vector<Dot> dots;
        QString timeText, valueText;
        QRectF timePill, valuePill, card;
        QString head, sub;
        qreal headW = 0, nameW = 0, rowH = 0, padX = 0, headH = 0;
        std::vector<CardLine> lines;
        std::vector<QString> values;
        std::vector<int> columns;
        std::vector<qreal> rights;
        std::vector<char> labelVisible;
        std::vector<QRect> rects;
    };

    Impl(LiveChart *owner, LiveChartOptions options);

    bool canPaint() const;
    void requestRepaint();
    void requestRepaint(const QRegion &region);
    void invalidate();
    void notifyState();
    void animationTick();
    void watchWindow();

    void refreshFonts();
    void refreshRowMetrics();
    void refreshRowColors();
    void refreshColors();
    QColor rowColor(const LiveChartRow &row) const;
    QString seriesKey(const QString &row, const QString &column) const;
    QSet<QString> currentKeys() const;
    void resolveColumns();
    void resetPointerState();

    qint64 windowMs() const;
    qint64 tickMs() const;
    bool rowOn(int reg) const;
    bool columnOn(int column) const;
    qreal xOf(qint64 t) const;
    qreal yOf(double v) const;
    float valueAt(int bucket, int series) const;
    bool needs(const QRectF &r) const;

    void layout();
    bool layoutView();
    void layoutCompact(qreal W, qreal H);
    bool prepareData();
    double peakValue() const;
    void collectLegend(int from);
    bool rowHasData(int reg, int from) const;
    void buildBuckets(bool perPoint);
    qreal layoutSide(qreal W, qreal H);
    qreal layoutTop(qreal W);
    void addRangeHits();
    void layoutGrid(const LiveChartScale &scale);
    void layoutBands();
    void layoutMarkers();
    void layoutTimeAxis();
    void layoutMessage(MessageKind kind, const QString &title, const QString &body);
    void axisVisibility(qreal crossX, std::vector<char> &out) const;
    void computeOverlay();
    void computeCard(const Bucket &b, int focus, qreal cx, qreal anchorY);
    QRectF pillRect(const QString &text, qreal x, qreal y, bool centered) const;
    QRegion overlayRegion() const;

    void paint(QPainter &p);
    void paintCompact(QPainter &p);
    void paintSide(QPainter &p);
    void paintColumnHeader(QPainter &p, const Hit &hit);
    void paintSideRow(QPainter &p, const SideRow &row);
    void paintTop(QPainter &p);
    void paintChip(QPainter &p, const Hit &hit);
    void paintRange(QPainter &p);
    void paintGrid(QPainter &p);
    void paintBands(QPainter &p);
    void paintEventLines(QPainter &p);
    void paintFlags(QPainter &p);
    void paintSeries(QPainter &p);
    void paintSegment(QPainter &p, int j, int a, int b, const QPen &pen, const QColor &color, const QColor &washColor, bool wash, bool dashed);
    void traceRun(QPainter &p, int j, int a, int b, const QPen &pen, const QColor &washColor, bool wash);
    void paintAxis(QPainter &p);
    void paintOverlay(QPainter &p);
    void paintCard(QPainter &p);
    void paintPill(QPainter &p, const QString &text, const QRectF &rect);
    void paintMessage(QPainter &p);
    static QString markerText(const SeriesBuffer::Marker &marker, bool note);

    const Hit *hitAt(const QPointF &pos) const;
    int nearestByX(qreal x) const;
    int nearestByTime(qint64 t) const;
    int findSeries(int reg, int column) const;
    int pickSeries(int bucket, qreal y) const;
    void resolveAnchor();
    void hoverAt(const QPointF &pos, const QPoint &global);
    void dropPointer();
    void updateCursor(bool hand);
    void updateToolTip(const Hit *hit, const QPoint &global);
    void activate(const Hit &hit, bool alt);
    void soloRow(int reg);

    LiveChart *q;
    LiveChartOptions o;
    LiveChartState s;
    SeriesBuffer buf;

    std::vector<RowEntry> registry;
    int currentCount = 0;
    QString blockedTitle;
    QString blockedBody;
    QList<QPointer<QAction>> menuActions;

    QFont fontBase, fontSmall, fontMedium, fontBold, fontSmallMedium, fontSmallBold, fontTitle, fontLabel, fontLabelBold;
    QFontMetricsF fmBase{QFont()}, fmSmall{QFont()}, fmMedium{QFont()}, fmBold{QFont()}, fmSmallMedium{QFont()}, fmSmallBold{QFont()},
        fmTitle{QFont()}, fmLabel{QFont()}, fmLabelBold{QFont()};
    qreal legendRowH = 22, chipH = 24, headerH = 20, segW = 34, cardRowH = 19, compactRowH = 16, flagH = 17, pillH = 18, axisH = 24;
    qreal gridLabelGap = 9, messageLineH = 17, messageTitleH = 20, valueWidth = 0, cardTotalWidth = 0, stoppedWidth = 0;
    QString totalText;
    QString stoppedText;
    QStringList columnHeaders;
    std::vector<qreal> columnHeaderWidths;
    std::vector<qreal> columnArrowWidths;

    Colors c;
    QBrush hatchBrush;
    View view;
    Overlay ov;
    std::vector<int> legendRows;
    std::vector<Series> series;
    std::vector<Bucket> buckets;
    std::vector<float> bucketMax;
    std::vector<float> bucketTotal;
    std::vector<double> sampleSums;
    std::vector<int> drawOrder;
    std::vector<qreal> xs, ys, slopes, tangents;
    QPainterPath linePath, fillPath;
    std::vector<Hit> hits;
    std::vector<SideRow> sideRows;
    std::vector<QString> legendValues;
    std::vector<GridLine> gridLines;
    std::vector<Band> bands;
    std::vector<MarkerItem> markerItems;
    std::vector<AxisLabel> axisLabels;
    std::vector<char> axisScratch;
    QStringList messageLines;

    Hit hoverHit;
    Anchor anchor;
    int ap = -1;
    int apSeries = -1;
    QPointF pointer;
    bool pointerInside = false;
    bool tooltipShown = false;
    bool handCursor = false;

    bool layoutDirty = true;
    bool pendingRepaint = false;
    bool fullPaint = true;
    const QRegion *dirty = nullptr;
    double drawnTop = std::numeric_limits<double>::quiet_NaN();
    QElapsedTimer frameClock;
    QTimer animation;
    QPointer<QWindow> watchedWindow;
    QMetaObject::Connection windowConnection;
};

LiveChart::Impl::Impl(LiveChart *owner, LiveChartOptions options) : q(owner), o(std::move(options)), buf(o.capacity) {
    if (o.columns.isEmpty()) o.columns.append(LiveChartColumn{QStringLiteral("v"), {}, {}, false, true});
    if (!o.axis.format || !o.axis.scale) o.axis = LiveChartAxis::rateBytes();
    if (o.compact) {
        o.legend = LiveChartOptions::Legend::None;
        o.ranges.clear();
        o.timeAxis = false;
        o.axisPills = false;
        o.markers = false;
        o.stoppedBands = false;
        o.contextMenu = false;
    }
    if (!o.ranges.isEmpty()) s.range = std::clamp(s.range, 0, int(o.ranges.size()) - 1);
    animation.setInterval(16);
}

bool LiveChart::Impl::canPaint() const {
    // A collapsed QSplitter pane is moved off-screen rather than hidden, so isVisible() alone stays true.
    return q->isVisible() && !q->window()->isMinimized() && !q->visibleRegion().isEmpty();
}

void LiveChart::Impl::requestRepaint() {
    if (canPaint()) {
        q->update();
    } else {
        pendingRepaint = true;
    }
}

void LiveChart::Impl::requestRepaint(const QRegion &region) {
    if (canPaint()) {
        q->update(region);
    } else {
        pendingRepaint = true;
    }
}

void LiveChart::Impl::invalidate() {
    layoutDirty = true;
    requestRepaint();
}

void LiveChart::Impl::notifyState() {
    invalidate();
    emit q->stateChanged();
}

void LiveChart::Impl::animationTick() {
    layoutDirty = true;
    if (canPaint()) {
        q->update();
        return;
    }
    animation.stop();
    drawnTop = std::numeric_limits<double>::quiet_NaN();
    pendingRepaint = true;
}

void LiveChart::Impl::watchWindow() {
    QWindow *handle = q->window()->windowHandle();
    if (handle == watchedWindow) return;
    QObject::disconnect(windowConnection);
    watchedWindow = handle;
    // A repaint skipped while minimized leaves no dirty region, so the restore expose would only flush the stale backing store.
    if (handle) windowConnection = QObject::connect(handle, &QWindow::windowStateChanged, q, [this] {
        if (pendingRepaint) requestRepaint();
    });
}

void LiveChart::Impl::refreshFonts() {
    fontBase = q->font();
    fontSmall = fontBase;
    fontLabel = fontBase;
    if (fontBase.pointSizeF() > 0) {
        fontSmall.setPointSizeF(std::max(7.0, fontBase.pointSizeF() - 1));
        fontLabel.setPointSizeF(std::max(6.5, fontBase.pointSizeF() - 1.5));
    } else if (fontBase.pixelSize() > 0) {
        fontSmall.setPixelSize(std::max(9, fontBase.pixelSize() - 1));
        fontLabel.setPixelSize(std::max(8, fontBase.pixelSize() - 2));
    }
    fontMedium = fontBase;
    fontMedium.setWeight(QFont::Medium);
    fontBold = fontBase;
    fontBold.setWeight(QFont::DemiBold);
    fontSmallMedium = fontSmall;
    fontSmallMedium.setWeight(QFont::Medium);
    fontSmallBold = fontSmall;
    fontSmallBold.setWeight(QFont::DemiBold);
    fontLabelBold = fontLabel;
    fontLabelBold.setBold(true);
    fontTitle = fontBold;
    if (fontBold.pointSizeF() > 0) fontTitle.setPointSizeF(fontBold.pointSizeF() + 1);
    else if (fontBold.pixelSize() > 0) fontTitle.setPixelSize(fontBold.pixelSize() + 1);

    fmBase = QFontMetricsF(fontBase, q);
    fmSmall = QFontMetricsF(fontSmall, q);
    fmMedium = QFontMetricsF(fontMedium, q);
    fmBold = QFontMetricsF(fontBold, q);
    fmSmallMedium = QFontMetricsF(fontSmallMedium, q);
    fmSmallBold = QFontMetricsF(fontSmallBold, q);
    fmTitle = QFontMetricsF(fontTitle, q);
    fmLabel = QFontMetricsF(fontLabel, q);
    fmLabelBold = QFontMetricsF(fontLabelBold, q);

    const qreal baseH = std::ceil(fmBase.height()), smallH = std::ceil(fmSmall.height());
    legendRowH = std::max<qreal>(22, baseH + 6);
    chipH = std::max<qreal>(24, baseH + 8);
    headerH = std::max<qreal>(20, smallH + 6);
    cardRowH = std::max<qreal>(19, baseH + 3);
    compactRowH = std::max<qreal>(16, smallH + 2);
    flagH = std::max<qreal>(17, smallH + 3);
    pillH = std::max<qreal>(18, std::ceil(fmSmallBold.height()) + 4);
    axisH = std::max<qreal>(24, pillH + 6);
    gridLabelGap = std::max<qreal>(9, std::ceil(smallH / 2) + 2);
    messageLineH = std::max<qreal>(17, baseH + 1);
    messageTitleH = std::max<qreal>(20, std::ceil(fmTitle.height()) + 3);

    valueWidth = 0;
    for (const auto &w : o.axis.widest) valueWidth = std::max(valueWidth, fmMedium.horizontalAdvance(w));
    valueWidth = std::ceil(valueWidth);
    segW = 34;
    for (const auto &range : o.ranges) segW = std::max(segW, std::ceil(fmBold.horizontalAdvance(range.label)) + 14);

    columnHeaders.clear();
    columnHeaderWidths.clear();
    columnArrowWidths.clear();
    for (const auto &column : o.columns) {
        const QString header = column.arrow.isEmpty() ? column.label : column.arrow + QLatin1Char(' ') + column.label;
        columnHeaders.append(header);
        columnHeaderWidths.push_back(fmSmallMedium.horizontalAdvance(header));
        columnArrowWidths.push_back(fmBase.horizontalAdvance(column.arrow));
    }
    totalText = LiveChart::tr("Total");
    cardTotalWidth = (o.compact ? fmSmall : fmBase).horizontalAdvance(totalText);
    stoppedText = LiveChart::tr("Stopped");
    stoppedWidth = fmSmallMedium.horizontalAdvance(stoppedText);
    refreshRowMetrics();
}

void LiveChart::Impl::refreshRowMetrics() {
    const QFontMetricsF &cardName = o.compact ? fmSmall : fmBase;
    const QFontMetricsF &cardKind = o.compact ? fmLabel : fmSmall;
    for (auto &entry : registry) {
        entry.shortName = ShortDisplayName(entry.row.label);
        entry.nameWidth = fmBase.horizontalAdvance(entry.shortName);
        entry.kindWidth = entry.row.kind.isEmpty() ? 0 : fmSmall.horizontalAdvance(entry.row.kind);
        entry.cardNameWidth = cardName.horizontalAdvance(entry.shortName);
        entry.cardKindWidth = entry.row.kind.isEmpty() ? 0 : cardKind.horizontalAdvance(entry.row.kind);
    }
}

void LiveChart::Impl::refreshRowColors() {
    for (auto &entry : registry) entry.color = rowColor(entry.row);
}

void LiveChart::Impl::refreshColors() {
    const QPalette &pal = q->palette();
    const QColor text = pal.color(o.compact ? QPalette::WindowText : QPalette::Text);
    const QColor highlight = pal.color(QPalette::Highlight);
    c.base = liveChartAlpha(pal.color(o.compact ? QPalette::Window : QPalette::Base), 255);
    c.ink = text;
    c.ink2 = liveChartAlpha(text, 205);
    c.muted = liveChartAlpha(text, 150);
    c.grid = liveChartAlpha(text, 24);
    c.axis = liveChartAlpha(text, 70);
    c.border = liveChartAlpha(text, 60);
    c.hairline = liveChartAlpha(text, 92);
    c.eventLine = liveChartAlpha(text, 92);
    c.stoppedFill = liveChartAlpha(text, 10);
    c.hatch = liveChartAlpha(text, 56);
    c.hover = liveChartAlpha(highlight, 30);
    c.selected = liveChartAlpha(highlight, 45);
    c.panel = liveChartAlpha(text, 12);
    c.label = liveChartAlpha(text, 150);
    c.caption = liveChartAlpha(text, 190);
    if (hatchBrush.color() != c.hatch || hatchBrush.style() != Qt::BDiagPattern) hatchBrush = QBrush(c.hatch, Qt::BDiagPattern);
}

QColor LiveChart::Impl::rowColor(const LiveChartRow &row) const {
    if (row.color.isValid()) return row.color;
    const ThemeTokens &tokens = themeManager()->tokens;
    const QColor color = row.paletteSlot < 0 ? tokens.chartContext : tokens.chartSeries[size_t(row.paletteSlot) % tokens.chartSeries.size()];
    return color.isValid() ? color : q->palette().color(QPalette::Highlight);
}

QString LiveChart::Impl::seriesKey(const QString &row, const QString &column) const {
    return row + QLatin1Char('|') + column;
}

QSet<QString> LiveChart::Impl::currentKeys() const {
    QSet<QString> keys;
    for (int i = 0; i < currentCount; ++i) {
        for (const auto &column : o.columns) keys.insert(seriesKey(registry[i].row.key, column.key));
    }
    return keys;
}

void LiveChart::Impl::resolveColumns() {
    const int nc = int(o.columns.size());
    for (auto &entry : registry) {
        entry.columns.resize(nc);
        for (int k = 0; k < nc; ++k) {
            const QString key = seriesKey(entry.row.key, o.columns[k].key);
            entry.columns[k] = entry.current ? buf.addColumn(key) : buf.findColumn(key);
        }
    }
    registry.erase(std::remove_if(registry.begin(), registry.end(),
                                  [](const RowEntry &e) {
                                      return !e.current && std::all_of(e.columns.begin(), e.columns.end(), [](int col) { return col < 0; });
                                  }),
                   registry.end());
}

void LiveChart::Impl::resetPointerState() {
    anchor = {};
    ap = -1;
    apSeries = -1;
    hoverHit = {};
    hits.clear();
    if (tooltipShown) {
        QToolTip::hideText();
        tooltipShown = false;
    }
    updateCursor(false);
}

qint64 LiveChart::Impl::windowMs() const {
    const int seconds = o.ranges.isEmpty() ? o.windowSeconds : o.ranges[s.range].seconds;
    return qint64(std::max(1, seconds)) * 1000;
}

qint64 LiveChart::Impl::tickMs() const {
    const int seconds = o.ranges.isEmpty() ? 30 : o.ranges[s.range].tickSeconds;
    return qint64(seconds > 0 ? seconds : 30) * 1000;
}

bool LiveChart::Impl::rowOn(int reg) const {
    return !s.hiddenRows.contains(registry[reg].row.key);
}

bool LiveChart::Impl::columnOn(int column) const {
    return !s.hiddenColumns.contains(o.columns[column].key);
}

qreal LiveChart::Impl::xOf(qint64 t) const {
    return view.plot.right() - double(view.end - t) / double(view.span) * view.plot.width();
}

qreal LiveChart::Impl::yOf(double v) const {
    double f = v / view.top;
    if (o.compact) f = std::clamp(f, 0.0, 1.0);
    return view.plot.bottom() - f * view.plot.height();
}

float LiveChart::Impl::valueAt(int bucket, int series) const {
    return bucketMax[size_t(bucket) * this->series.size() + size_t(series)];
}

bool LiveChart::Impl::needs(const QRectF &r) const {
    return fullPaint || !dirty || dirty->intersects(r.toAlignedRect());
}

void LiveChart::Impl::layout() {
    layoutDirty = false;
    if (layoutView()) {
        if (!animation.isActive()) animation.start();
    } else {
        animation.stop();
    }
    if (view.valid) {
        resolveAnchor();
    } else {
        ap = -1;
        apSeries = -1;
    }
    computeOverlay();
}

bool LiveChart::Impl::layoutView() {
    view = View{};
    hits.clear();
    sideRows.clear();
    legendValues.clear();
    gridLines.clear();
    bands.clear();
    markerItems.clear();
    axisLabels.clear();
    messageLines.clear();
    buckets.clear();
    bucketMax.clear();
    bucketTotal.clear();
    series.clear();
    refreshColors();

    const qreal W = q->width(), H = q->height();
    view.end = buf.isEmpty() ? MonotonicMs() : buf.latestTime();
    view.span = windowMs();
    view.start = view.end - view.span;
    collectLegend(std::max(0, buf.lowerBound(view.start) - 1));
    if (o.compact) {
        layoutCompact(W, H);
        return false;
    }

    const qreal axisSpace = o.timeAxis ? axisH : 0;
    view.legend = o.legend;
    if (view.legend == LiveChartOptions::Legend::Side && W < kLiveChartSideLegendMinWidth) view.legend = LiveChartOptions::Legend::Top;
    if (view.legend == LiveChartOptions::Legend::Side) {
        const qreal sideW = layoutSide(W, H);
        view.plot = QRectF(8, 12, W - sideW - 20, H - 12 - axisSpace);
    } else if (view.legend == LiveChartOptions::Legend::Top) {
        const qreal bottom = layoutTop(W);
        view.plot = QRectF(8, bottom + 10, W - 16, H - bottom - 10 - axisSpace);
    } else {
        view.plot = QRectF(8, 10, W - 16, H - 10 - axisSpace);
    }
    if (view.plot.width() < 40 || view.plot.height() < 24) return false;
    view.plotValid = true;
    if (!blockedTitle.isEmpty()) {
        layoutMessage(BlockedMessage, blockedTitle, blockedBody);
        return false;
    }
    if (!prepareData()) {
        layoutMessage(EmptyMessage, o.emptyTitle, o.emptyBody);
        return false;
    }

    const LiveChartScale scale = o.axis.scale(peakValue());
    const double target = scale.top > 0 ? scale.top : 1;
    qint64 dt = 16;
    if (frameClock.isValid()) dt = std::min<qint64>(100, frameClock.restart());
    else frameClock.start();
    bool animating = false;
    if (std::isnan(drawnTop)) {
        drawnTop = target;
    } else {
        const double diff = target - drawnTop;
        if (std::abs(diff) > target * 0.003) {
            drawnTop += diff * (1 - std::exp(-double(dt) / 50));
            animating = true;
        } else {
            drawnTop = target;
        }
    }
    view.top = drawnTop;

    layoutGrid(scale);
    if (o.stoppedBands) layoutBands();
    if (o.markers && s.markers) layoutMarkers();
    if (o.timeAxis) layoutTimeAxis();
    view.allHidden = std::none_of(series.begin(), series.end(), [](const Series &x) { return x.on; });
    if (view.allHidden) {
        layoutMessage(HiddenMessage, LiveChart::tr("All lines are hidden"), LiveChart::tr("Click an outbound or a direction in the legend to show it."));
    }
    view.valid = true;
    return animating;
}

void LiveChart::Impl::layoutCompact(qreal W, qreal H) {
    view.panel = QRectF(0, 0, W, H).adjusted(1.5, 1.5, -1.5, -1.5);
    if (view.panel.width() <= 2 || view.panel.height() <= 2) return;
    view.panelValid = true;
    const qreal strip = std::min(fmLabel.height(), view.panel.height() / 3), padX = 5;
    const QRectF &panel = view.panel;
    view.plot = QRectF(panel.left() + padX, panel.top() + strip, panel.width() - 2 * padX, panel.height() - 2 * strip);
    view.topStrip = QRectF(panel.left() + padX, panel.top(), panel.width() - 2 * padX, strip);
    view.bottomStrip = QRectF(panel.left() + padX, panel.bottom() - strip, panel.width() - 2 * padX, strip);
    if (view.plot.width() <= 1 || view.plot.height() <= 1) return;
    view.plotValid = true;
    const bool hasData = prepareData();
    const LiveChartScale scale = o.axis.scale(peakValue());
    view.top = drawnTop = scale.top > 0 ? scale.top : 1;
    view.ceiling = scale.label ? scale.label(scale.top) : o.axis.format(scale.top);
    view.allHidden = std::none_of(series.begin(), series.end(), [](const Series &x) { return x.on; });
    view.valid = hasData;
}

bool LiveChart::Impl::prepareData() {
    const qint64 interval = std::max(1, o.pointIntervalMs);
    const qint64 perBucket = std::max<qint64>(1, qint64(std::ceil(double(view.span) / double(interval) / (view.plot.width() / 2))));
    view.kms = perBucket * interval;
    view.wallOffset = QDateTime::currentMSecsSinceEpoch() - MonotonicMs();
    const int nc = int(o.columns.size());
    for (int pos = 0; pos < int(legendRows.size()); ++pos) {
        const int reg = legendRows[pos];
        const bool shown = rowOn(reg);
        for (int k = 0; k < nc; ++k) series.push_back(Series{reg, pos, k, registry[reg].columns[k], shown && columnOn(k)});
    }
    buildBuckets(perBucket == 1);
    return !buckets.empty();
}

double LiveChart::Impl::peakValue() const {
    const size_t ns = series.size();
    double peak = 0;
    for (size_t j = 0; j < ns; ++j) {
        if (!series[j].on) continue;
        for (size_t b = 0; b < buckets.size(); ++b) {
            const float v = bucketMax[b * ns + j];
            if (v > peak) peak = v;
        }
    }
    return peak;
}

void LiveChart::Impl::collectLegend(int from) {
    legendRows.clear();
    for (int reg = 0; reg < int(registry.size()); ++reg) {
        if (registry[reg].current || rowHasData(reg, from)) legendRows.push_back(reg);
    }
}

bool LiveChart::Impl::rowHasData(int reg, int from) const {
    for (const int column : registry[reg].columns) {
        if (column < 0) continue;
        for (int i = from; i < buf.size(); ++i) {
            if (!std::isnan(buf.value(column, i))) return true;
        }
    }
    return false;
}

void LiveChart::Impl::buildBuckets(bool perPoint) {
    const size_t ns = series.size(), nc = size_t(o.columns.size());
    const qint64 gapMs = qint64(o.pointIntervalMs * 2.5);
    sampleSums.resize(nc);
    int cur = -1;
    qint64 prevT = 0;
    for (int i = std::max(0, buf.lowerBound(view.start - view.kms) - 1); i < buf.size(); ++i) {
        const qint64 t = buf.time(i);
        if (t > view.end) break;
        // Keys are absolute time, not buffer positions, so a bucket keeps its points (and the + its place) as new ones arrive.
        const qint64 key = perPoint ? t : liveChartFloorDiv(t, view.kms);
        const bool gap = cur >= 0 && t - prevT > gapMs;
        if (cur < 0 || gap || buckets[cur].key != key) {
            Bucket bucket;
            bucket.key = key;
            bucket.from = t;
            bucket.brk = gap;
            buckets.push_back(bucket);
            bucketMax.resize(bucketMax.size() + ns, kLiveChartNaN);
            bucketTotal.resize(bucketTotal.size() + nc, kLiveChartNaN);
            cur = int(buckets.size()) - 1;
        }
        Bucket &b = buckets[cur];
        b.to = t;
        ++b.n;
        if (buf.running(i)) b.run = true;
        float *mx = bucketMax.data() + size_t(cur) * ns;
        float *tot = bucketTotal.data() + size_t(cur) * nc;
        std::fill(sampleSums.begin(), sampleSums.end(), std::numeric_limits<double>::quiet_NaN());
        for (size_t j = 0; j < ns; ++j) {
            if (series[j].bufferColumn < 0) continue;
            const float v = buf.value(series[j].bufferColumn, i);
            if (std::isnan(v)) continue;
            if (!(mx[j] >= v)) mx[j] = v;
            double &sum = sampleSums[size_t(series[j].column)];
            sum = std::isnan(sum) ? v : sum + v;
        }
        for (size_t k = 0; k < nc; ++k) {
            if (!std::isnan(sampleSums[k]) && !(tot[k] >= sampleSums[k])) tot[k] = float(sampleSums[k]);
        }
        prevT = t;
    }
    for (auto &b : buckets) {
        b.tx = b.from + (b.to - b.from) / 2;
        b.x = xOf(b.tx);
    }
}

qreal LiveChart::Impl::layoutSide(qreal W, qreal H) {
    qreal nameW = 56;
    for (const int reg : legendRows) {
        const auto &entry = registry[reg];
        nameW = std::max(nameW, entry.nameWidth + (entry.row.kind.isEmpty() ? 0 : 6 + entry.kindWidth));
    }
    nameW = std::ceil(nameW);
    const int nc = int(o.columns.size());
    const qreal colW = valueWidth + 12;
    const qreal sideW = 30 + nameW + nc * colW + 10;
    const qreal x0 = W - sideW;
    view.sideX0 = x0;
    view.sideNameW = nameW;
    view.sideColW = colW;

    qreal y = 8;
    if (!o.ranges.isEmpty()) {
        view.rangeRect = QRectF(W - 8 - segW * o.ranges.size(), y, segW * o.ranges.size(), chipH);
        addRangeHits();
        y += chipH + 10;
    }
    if (nc > 1) {
        for (int k = 0; k < nc; ++k) {
            const qreal right = x0 + 30 + nameW + (k + 1) * colW, lw = columnHeaderWidths[k];
            hits.push_back(Hit{ColumnHit, k, QRectF(right - lw - 24, y, lw + 28, headerH)});
        }
        y += headerH + 4;
    }
    const int last = buf.size() - 1;
    const bool blocked = !blockedTitle.isEmpty();
    for (const int reg : legendRows) {
        const qreal h = legendRowH;
        if (y + h > H - 4) break;
        const QRectF rect(x0 + 4, y, sideW - 8, h);
        sideRows.push_back(SideRow{reg, rect, int(legendValues.size())});
        for (int k = 0; k < nc; ++k) {
            const int column = registry[reg].columns[k];
            const float v = blocked || last < 0 || column < 0 ? kLiveChartNaN : buf.value(column, last);
            legendValues.push_back(std::isnan(v) ? QStringLiteral("—") : o.axis.format(v));
        }
        hits.push_back(Hit{RowHit, reg, rect});
        y += h;
    }
    return sideW;
}

qreal LiveChart::Impl::layoutTop(qreal W) {
    const qreal pad = 8, gap = 6, top = 8, h = chipH;
    const int nc = int(o.columns.size());
    const qreal rangeW = o.ranges.isEmpty() ? 0 : segW * o.ranges.size();
    const qreal limit = W - pad - (rangeW > 0 ? rangeW + 12 : 0);
    qreal x = pad, y = top;
    const auto place = [&](HitType type, int index, qreal w) {
        if (x > pad && x + w > limit) {
            x = pad;
            y += h + 6;
        }
        hits.push_back(Hit{type, index, QRectF(x, y, w, h)});
        x += w + gap;
    };
    for (const int reg : legendRows) place(RowHit, reg, 30 + std::ceil(registry[reg].nameWidth) + 10);
    if (nc > 1) {
        for (int k = 0; k < nc; ++k) place(ColumnHit, k, 30 + std::ceil(columnArrowWidths[k]) + 10);
    }
    if (rangeW > 0) {
        view.rangeRect = QRectF(W - pad - rangeW, top, rangeW, h);
        addRangeHits();
    }
    return y + h;
}

void LiveChart::Impl::addRangeHits() {
    const QRectF &r = view.rangeRect;
    for (int i = 0; i < int(o.ranges.size()); ++i) hits.push_back(Hit{RangeHit, i, QRectF(r.left() + i * segW, r.top(), segW, r.height())});
}

void LiveChart::Impl::layoutGrid(const LiveChartScale &scale) {
    const QRectF &r = view.plot;
    const qreal lineH = fmSmall.height();
    for (int i = 0; i < 24; ++i) {
        if (i > 0 && !(scale.step > 0)) break;
        const double g = i * scale.step;
        const qreal yy = yOf(g);
        if (yy < r.top() - 0.5) break;
        GridLine line;
        line.y = std::round(yy);
        line.baseline = i == 0;
        if (i > 0 && scale.label) {
            const qreal mid = line.y + 0.5;
            line.label = scale.label(g);
            line.labelMid = mid - gridLabelGap < r.top() ? mid + gridLabelGap : mid - gridLabelGap;
            line.labelRect = QRectF(r.left() + 2, line.labelMid - lineH / 2 - 1, fmSmall.horizontalAdvance(line.label) + 2, lineH + 2);
        }
        gridLines.push_back(std::move(line));
    }
}

void LiveChart::Impl::layoutBands() {
    const QRectF &r = view.plot;
    const qreal half = std::max(1.0, double(view.kms) / double(view.span) * r.width() / 2);
    for (size_t i = 0; i < buckets.size();) {
        if (buckets[i].run) {
            ++i;
            continue;
        }
        size_t j = i;
        while (j + 1 < buckets.size() && !buckets[j + 1].run) ++j;
        const qreal x0 = std::max(r.left(), buckets[i].x - half), x1 = std::min(r.right(), buckets[j].x + half);
        if (x1 > x0) bands.push_back(Band{QRectF(x0, r.top(), x1 - x0, r.height()), x1 - x0 > stoppedWidth + 16});
        i = j + 1;
    }
}

void LiveChart::Impl::layoutMarkers() {
    const QRectF &r = view.plot;
    qreal lastRight = -std::numeric_limits<qreal>::infinity();
    for (const auto &marker : buf.markers()) {
        if (marker.t < view.start || marker.t > view.end) continue;
        const qreal x = xOf(marker.t);
        MarkerItem item;
        item.x = std::round(x);
        item.text = markerText(marker, false);
        const qreal w = std::ceil(fmSmall.horizontalAdvance(item.text)) + 12;
        qreal fx = x + 3;
        if (fx + w > r.right()) fx = x - 3 - w;
        if (fx >= r.left() && fx >= lastRight + 4) {
            item.flagged = true;
            item.flag = QRectF(fx, r.top() + 2, w, flagH);
            lastRight = fx + w;
        }
        markerItems.push_back(std::move(item));
    }
}

void LiveChart::Impl::layoutTimeAxis() {
    const QRectF &r = view.plot;
    const qint64 tick = tickMs();
    const qint64 wallStart = view.start + view.wallOffset, wallEnd = view.end + view.wallOffset;
    const qint64 tz = qint64(QDateTime::fromMSecsSinceEpoch(wallEnd).offsetFromUtc()) * 1000;
    const bool seconds = tick < 60000;
    const qreal labelY = r.bottom() + 4 + pillH / 2, lineH = fmSmall.height();
    // Ticks sit on round local clock times: ceil to a multiple of tick in local time, then shift back to UTC.
    for (qint64 tw = -liveChartFloorDiv(-(wallStart + tz), tick) * tick - tz; tw <= wallEnd; tw += tick) {
        const qreal x = xOf(tw - view.wallOffset);
        if (x < r.left() - 0.5) continue;
        AxisLabel label;
        label.x = x;
        label.text = liveChartClock(tw, seconds);
        label.width = fmSmall.horizontalAdvance(label.text);
        label.rect = QRectF(x - label.width / 2 - 1, labelY - lineH / 2 - 1, label.width + 2, lineH + 2);
        axisLabels.push_back(std::move(label));
    }
    axisVisibility(std::numeric_limits<qreal>::quiet_NaN(), axisScratch);
    for (size_t i = 0; i < axisLabels.size(); ++i) axisLabels[i].base = axisScratch[i];
}

void LiveChart::Impl::layoutMessage(MessageKind kind, const QString &title, const QString &body) {
    view.message = kind;
    view.messageTitle = title;
    const qreal maxW = std::max<qreal>(120, view.plot.width() - 32);
    QString line;
    for (const auto &word : body.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const QString next = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
        if (!line.isEmpty() && fmBase.horizontalAdvance(next) > maxW) {
            messageLines.append(line);
            line = word;
        } else {
            line = next;
        }
    }
    if (!line.isEmpty()) messageLines.append(line);
}

void LiveChart::Impl::axisVisibility(qreal crossX, std::vector<char> &out) const {
    const QRectF &r = view.plot;
    out.assign(axisLabels.size(), 0);
    qreal lastRight = -std::numeric_limits<qreal>::infinity();
    for (size_t i = 0; i < axisLabels.size(); ++i) {
        const auto &label = axisLabels[i];
        const qreal half = label.width / 2;
        if (label.x - half < 2 || label.x + half > r.right() + 6 || label.x - half < lastRight + 14) continue;
        if (!std::isnan(crossX) && std::abs(label.x - crossX) < half + 40) continue;
        out[i] = 1;
        lastRight = label.x + half;
    }
}

void LiveChart::Impl::computeOverlay() {
    ov.active = false;
    ov.focus = -1;
    ov.dots.clear();
    ov.rects.clear();
    ov.timeText.clear();
    ov.valueText.clear();
    ov.lines.clear();
    ov.values.clear();
    const bool cross = view.valid && o.crosshair && ap >= 0 && !view.allHidden;
    const qreal crossX = cross ? buckets[ap].x : std::numeric_limits<qreal>::quiet_NaN();
    axisVisibility(crossX, ov.labelVisible);
    for (size_t i = 0; i < axisLabels.size(); ++i) {
        if (bool(ov.labelVisible[i]) != axisLabels[i].base) ov.rects.push_back(liveChartInflated(axisLabels[i].rect));
    }
    if (!cross) return;

    ov.active = true;
    const QRectF &r = view.plot;
    const size_t ns = series.size();
    const Bucket &b = buckets[ap];
    const int focus = b.run ? apSeries : -1;
    const float focusValue = focus >= 0 ? valueAt(ap, focus) : kLiveChartNaN;
    ov.focus = focus;
    ov.cx = std::round(b.x);
    ov.cy = focus >= 0 ? yOf(focusValue) : r.center().y();
    ov.hy = liveChartClamp(ov.cy, r.top(), r.bottom());
    ov.rects.push_back(liveChartInflated(QRectF(ov.cx, r.top(), 1, r.height())));
    if (focus >= 0) ov.rects.push_back(liveChartInflated(QRectF(r.left(), std::round(ov.hy), r.width(), 1)));
    if (b.run) {
        const qreal other = o.compact ? 2.5 : 3, focused = o.compact ? 3.5 : 5;
        for (size_t j = 0; j < ns; ++j) {
            if (!series[j].on || int(j) == focus) continue;
            const float v = valueAt(ap, int(j));
            if (!std::isnan(v)) ov.dots.push_back(Dot{QPointF(b.x, yOf(v)), other, registry[series[j].reg].color});
        }
        if (focus >= 0) ov.dots.push_back(Dot{QPointF(b.x, ov.cy), focused, registry[series[focus].reg].color});
        for (const auto &dot : ov.dots) {
            const qreal outer = dot.r + 2;
            ov.rects.push_back(liveChartInflated(QRectF(dot.center.x() - outer, dot.center.y() - outer, 2 * outer, 2 * outer)));
        }
    }
    if (o.axisPills) {
        if (o.timeAxis) {
            ov.timeText = liveChartClock(b.to + view.wallOffset, true);
            ov.timePill = pillRect(ov.timeText, ov.cx + 0.5, r.bottom() + 3, true);
            ov.rects.push_back(liveChartInflated(ov.timePill));
        }
        if (focus >= 0) {
            ov.valueText = o.axis.format(focusValue);
            ov.valuePill = pillRect(ov.valueText, r.left(), ov.hy, false);
            ov.rects.push_back(liveChartInflated(ov.valuePill));
        }
    }
    computeCard(b, focus, ov.cx + 0.5, ov.cy);
    ov.rects.push_back(liveChartInflated(ov.card));
}

void LiveChart::Impl::computeCard(const Bucket &b, int focus, qreal cx, qreal anchorY) {
    const QRectF &plot = view.plot;
    const qreal W = q->width(), H = q->height();
    const size_t nc = size_t(o.columns.size());
    const bool multi = nc > 1;
    const QFontMetricsF &fm = o.compact ? fmSmall : fmBase;
    const QFontMetricsF &fmValue = o.compact ? fmSmallBold : fmBold;
    const QFontMetricsF &fmSub = o.compact ? fmLabel : fmSmall;
    ov.rowH = o.compact ? compactRowH : cardRowH;
    ov.padX = o.compact ? 8 : 10;
    ov.headH = ov.rowH + 2;

    ov.columns.clear();
    for (size_t k = 0; k < nc; ++k) {
        if (columnOn(int(k))) ov.columns.push_back(int(k));
    }
    ov.head = b.n > 1 ? liveChartClock(b.from + view.wallOffset, true) + QStringLiteral(" – ") + liveChartClock(b.to + view.wallOffset, true)
                      : liveChartClock(b.to + view.wallOffset, true);
    ov.sub = b.n > 1 ? LiveChart::tr("peak of %1 s").arg(qRound(b.n * o.pointIntervalMs / 1000.0)) : QString();

    if (!b.run) {
        ov.lines.push_back(CardLine{CardLine::Note, -1, false, LiveChart::tr("Not running"), -1});
    } else {
        int rows = 0;
        const float *mx = bucketMax.data() + size_t(ap) * series.size();
        for (size_t pos = 0; pos < legendRows.size(); ++pos) {
            const int reg = legendRows[pos];
            if (!rowOn(reg)) continue;
            const bool any = std::any_of(ov.columns.begin(), ov.columns.end(), [&](int k) { return !std::isnan(mx[pos * nc + size_t(k)]); });
            if (!any) continue;
            const int offset = int(ov.values.size());
            for (const int k : ov.columns) {
                const float v = mx[pos * nc + size_t(k)];
                ov.values.push_back(std::isnan(v) ? QStringLiteral("—") : o.axis.format(v));
            }
            ov.lines.push_back(CardLine{CardLine::Row, reg, focus >= 0 && series[focus].reg == reg, {}, offset});
            ++rows;
        }
        if (o.totalInCard && rows > 1) {
            const int offset = int(ov.values.size());
            const float *tot = bucketTotal.data() + size_t(ap) * nc;
            for (const int k : ov.columns) ov.values.push_back(o.axis.format(std::isnan(tot[k]) ? 0.0 : double(tot[k])));
            ov.lines.push_back(CardLine{CardLine::Total, -1, false, {}, offset});
        }
    }
    if (o.markers) {
        const qint64 reach = view.kms / 2 + o.pointIntervalMs / 2;
        for (const auto &marker : buf.markers()) {
            if (std::abs(marker.t - b.tx) <= reach) ov.lines.push_back(CardLine{CardLine::Note, -1, false, markerText(marker, true), -1});
        }
    }

    qreal nameW = cardTotalWidth, noteW = 0;
    for (const auto &line : ov.lines) {
        if (line.type == CardLine::Note) {
            noteW = std::max(noteW, fm.horizontalAdvance(line.text));
        } else if (line.type == CardLine::Row) {
            const auto &entry = registry[line.reg];
            nameW = std::max(nameW, entry.cardNameWidth + (entry.row.kind.isEmpty() ? 0 : 6 + entry.cardKindWidth));
        }
    }
    const size_t vc = ov.columns.size();
    ov.rights.assign(vc, 0);
    qreal colsTotal = 0;
    for (size_t k = 0; k < vc; ++k) {
        qreal w = multi ? fmValue.horizontalAdvance(o.columns[ov.columns[k]].arrow) : 0;
        for (const auto &line : ov.lines) {
            if (line.values >= 0) w = std::max(w, fmValue.horizontalAdvance(ov.values[size_t(line.values) + k]));
        }
        ov.rights[k] = w;
        colsTotal += 14 + w;
    }
    ov.nameW = nameW;
    ov.headW = fmValue.horizontalAdvance(ov.head);
    const qreal subW = ov.sub.isEmpty() ? 0 : 8 + fmSub.horizontalAdvance(ov.sub);
    const qreal w = std::ceil(std::max({14 + nameW + colsTotal, noteW, ov.headW + subW + 16 + (multi ? colsTotal : 0)})) + ov.padX * 2;
    const qreal h = 8 + ov.headH + 6 + qreal(ov.lines.size()) * ov.rowH + 6;
    qreal x = cx + 16;
    if (x + w > W - 4) x = cx - 16 - w;
    x = std::max<qreal>(o.compact ? 2 : 4, x);
    const qreal lo = o.compact ? 2 : std::max<qreal>(4, plot.top());
    const qreal hi = std::max(lo, o.compact ? H - h - 2 : std::min(H - h - 4, plot.bottom() - h));
    ov.card = QRectF(x, liveChartClamp(anchorY - h / 2, lo, hi), w, h);

    qreal acc = x + ov.padX + 14 + nameW;
    for (auto &right : ov.rights) {
        acc += 14 + right;
        right = acc;
    }
}

QRectF LiveChart::Impl::pillRect(const QString &text, qreal x, qreal y, bool centered) const {
    const qreal w = std::ceil(fmSmallBold.horizontalAdvance(text)) + 12, h = pillH;
    const qreal bx = std::max<qreal>(2, std::min(q->width() - w - 2, centered ? x - w / 2 : x));
    return QRectF(bx, centered ? y : y - h / 2, w, h);
}

QRegion LiveChart::Impl::overlayRegion() const {
    QRegion region;
    for (const auto &rect : ov.rects) region += rect;
    return region;
}

void LiveChart::Impl::paint(QPainter &p) {
    if (o.compact) {
        paintCompact(p);
        return;
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(q->rect(), c.base);
    if (view.legend == LiveChartOptions::Legend::Side) paintSide(p);
    else if (view.legend == LiveChartOptions::Legend::Top) paintTop(p);
    if (!view.plotValid) return;
    if (view.message == BlockedMessage || view.message == EmptyMessage) {
        paintMessage(p);
        return;
    }
    paintGrid(p);
    if (!bands.empty()) paintBands(p);
    if (!markerItems.empty()) paintEventLines(p);
    paintSeries(p);
    if (!markerItems.empty()) paintFlags(p);
    if (o.timeAxis) paintAxis(p);
    if (view.message == HiddenMessage) {
        paintMessage(p);
        return;
    }
    paintOverlay(p);
}

void LiveChart::Impl::paintCompact(QPainter &p) {
    if (!view.panelValid) return;
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(c.panel);
    p.drawRoundedRect(view.panel, 4, 4);
    if (!view.plotValid) return;
    if (view.valid && !view.allHidden) paintSeries(p);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setFont(fontLabel);
    p.setPen(c.label);
    if (needs(view.topStrip)) liveChartText(p, fmLabel, view.ceiling, view.topStrip.left(), view.topStrip.center().y(), kLiveChartLeft);
    if (needs(view.bottomStrip)) {
        liveChartText(p, fmLabel, QStringLiteral("0"), view.bottomStrip.left(), view.bottomStrip.center().y(), kLiveChartLeft);
        if (!o.caption.isEmpty()) {
            p.setFont(fontLabelBold);
            p.setPen(c.caption);
            liveChartText(p, fmLabelBold, o.caption, view.bottomStrip.right(), view.bottomStrip.center().y(), kLiveChartRight);
        }
    }
    paintOverlay(p);
}

void LiveChart::Impl::paintSide(QPainter &p) {
    p.fillRect(QRectF(std::floor(view.sideX0), 8, 1, q->height() - 16), c.grid);
    if (!o.ranges.isEmpty() && needs(view.rangeRect)) paintRange(p);
    for (const auto &hit : hits) {
        if (hit.type == ColumnHit && needs(hit.rect)) paintColumnHeader(p, hit);
    }
    for (const auto &row : sideRows) {
        if (needs(row.rect)) paintSideRow(p, row);
    }
}

void LiveChart::Impl::paintColumnHeader(QPainter &p, const Hit &hit) {
    const int k = hit.index;
    const bool on = columnOn(k);
    const qreal right = hit.rect.right() - 4, lw = columnHeaderWidths[k], my = hit.rect.center().y();
    p.setRenderHint(QPainter::Antialiasing, true);
    if (hoverHit.type == ColumnHit && hoverHit.index == k) {
        p.setPen(Qt::NoPen);
        p.setBrush(c.hover);
        p.drawRoundedRect(hit.rect, 4, 4);
    }
    liveChartLineKey(p, right - lw - 19, right - lw - 6, my, on ? c.ink2 : liveChartFaded(c.muted, 0.5), o.columns[k].dashed);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setFont(fontSmallMedium);
    p.setPen(on ? c.ink2 : c.muted);
    liveChartText(p, fmSmallMedium, columnHeaders[k], right, my, kLiveChartRight);
}

void LiveChart::Impl::paintSideRow(QPainter &p, const SideRow &row) {
    const auto &entry = registry[row.reg];
    const bool on = rowOn(row.reg);
    const qreal x0 = view.sideX0, my = row.rect.center().y();
    p.setRenderHint(QPainter::Antialiasing, true);
    if (hoverHit.type == RowHit && hoverHit.index == row.reg) {
        p.setPen(Qt::NoPen);
        p.setBrush(c.hover);
        p.drawRoundedRect(row.rect, 4, 4);
    }
    liveChartLegendDot(p, QPointF(x0 + 17, my), entry.color, on);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setFont(fontBase);
    p.setPen(on ? c.ink : c.muted);
    liveChartText(p, fmBase, entry.shortName, x0 + 30, my, kLiveChartLeft);
    if (!entry.row.kind.isEmpty()) {
        p.setFont(fontSmall);
        p.setPen(c.muted);
        liveChartText(p, fmSmall, entry.row.kind, x0 + 30 + entry.nameWidth + 6, my, kLiveChartLeft);
    }
    p.setFont(fontMedium);
    for (int k = 0; k < int(o.columns.size()); ++k) {
        p.setPen(on && columnOn(k) ? c.ink : c.muted);
        liveChartText(p, fmMedium, legendValues[size_t(row.values + k)], x0 + 30 + view.sideNameW + (k + 1) * view.sideColW, my, kLiveChartRight);
    }
}

void LiveChart::Impl::paintTop(QPainter &p) {
    for (const auto &hit : hits) {
        if ((hit.type == RowHit || hit.type == ColumnHit) && needs(hit.rect)) paintChip(p, hit);
    }
    if (!o.ranges.isEmpty() && needs(view.rangeRect)) paintRange(p);
}

void LiveChart::Impl::paintChip(QPainter &p, const Hit &hit) {
    const bool row = hit.type == RowHit;
    const bool on = row ? rowOn(hit.index) : columnOn(hit.index);
    const bool hovered = hoverHit.type == hit.type && hoverHit.index == hit.index;
    const QRectF &r = hit.rect;
    const qreal my = r.center().y();
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen border(on ? c.grid : liveChartFaded(c.axis, 0.6), 1);
    if (!on) border.setDashPattern({3, 2});
    p.setPen(border);
    p.setBrush(hovered ? QBrush(c.hover) : QBrush());
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
    if (row) liveChartLegendDot(p, QPointF(r.left() + 15, my), registry[hit.index].color, on);
    else liveChartLineKey(p, r.left() + 9, r.left() + 23, my, on ? c.ink2 : liveChartFaded(c.muted, 0.5), o.columns[hit.index].dashed);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setFont(fontBase);
    p.setPen(on ? c.ink2 : c.muted);
    liveChartText(p, fmBase, row ? registry[hit.index].shortName : o.columns[hit.index].arrow, r.left() + 30, my, kLiveChartLeft);
}

void LiveChart::Impl::paintRange(QPainter &p) {
    const QRectF &r = view.rangeRect;
    const int n = int(o.ranges.size());
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c.axis, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < n; ++i) {
        const bool selected = i == s.range;
        if (selected || (hoverHit.type == RangeHit && hoverHit.index == i)) {
            p.setBrush(selected ? c.selected : c.hover);
            p.drawRoundedRect(QRectF(r.left() + i * segW + 2, r.top() + 2, segW - 4, r.height() - 4), 4, 4);
        }
    }
    p.setRenderHint(QPainter::Antialiasing, false);
    for (int i = 0; i < n; ++i) {
        const bool selected = i == s.range;
        p.setFont(selected ? fontBold : fontBase);
        p.setPen(selected ? c.ink : c.ink2);
        liveChartText(p, selected ? fmBold : fmBase, o.ranges[i].label, r.left() + i * segW + segW / 2, r.center().y() + 0.5, kLiveChartCenter);
    }
}

void LiveChart::Impl::paintGrid(QPainter &p) {
    const QRectF &r = view.plot;
    p.setFont(fontSmall);
    for (const auto &line : gridLines) {
        p.fillRect(QRectF(r.left(), line.y, r.width(), 1), line.baseline ? c.axis : c.grid);
        if (!line.label.isEmpty() && needs(line.labelRect)) {
            liveChartHalo(p, fmSmall, line.label, r.left() + 3, line.labelMid, c.muted, c.base, kLiveChartLeft);
        }
    }
}

void LiveChart::Impl::paintBands(QPainter &p) {
    const qreal labelH = fmSmallMedium.height();
    p.setFont(fontSmallMedium);
    for (const auto &band : bands) {
        p.fillRect(band.rect, c.stoppedFill);
        p.fillRect(band.rect, hatchBrush);
        if (!band.labeled) continue;
        const QPointF center = band.rect.center();
        if (needs(QRectF(center.x() - stoppedWidth / 2 - 2, center.y() - labelH / 2 - 2, stoppedWidth + 4, labelH + 4))) {
            liveChartHalo(p, fmSmallMedium, stoppedText, center.x(), center.y(), c.ink2, c.base, kLiveChartCenter);
        }
    }
}

void LiveChart::Impl::paintEventLines(QPainter &p) {
    const QRectF &r = view.plot;
    QPen pen(c.eventLine, 1);
    pen.setCapStyle(Qt::FlatCap);
    pen.setDashPattern({2, 3});
    p.setPen(pen);
    for (const auto &item : markerItems) p.drawLine(QPointF(item.x, r.top()), QPointF(item.x, r.bottom()));
}

void LiveChart::Impl::paintFlags(QPainter &p) {
    p.setFont(fontSmall);
    for (const auto &item : markerItems) {
        if (!item.flagged || !needs(item.flag)) continue;
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(c.axis, 1));
        p.setBrush(liveChartFaded(c.base, 0.94));
        p.drawRoundedRect(item.flag.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(c.ink2);
        liveChartText(p, fmSmall, item.text, item.flag.left() + 6, item.flag.center().y() + 0.5, kLiveChartLeft);
    }
}

void LiveChart::Impl::paintSeries(QPainter &p) {
    const QRectF &r = view.plot;
    const size_t ns = series.size();
    const int hoveredRow = hoverHit.type == RowHit ? hoverHit.index : -1;
    drawOrder.clear();
    for (size_t j = 0; j < ns; ++j) {
        if (series[j].on) drawOrder.push_back(int(j));
    }
    std::sort(drawOrder.begin(), drawOrder.end(), [this](int a, int b) {
        const Series &x = series[a], &y = series[b];
        if (x.legendPos != y.legendPos) return x.legendPos > y.legendPos;
        const bool dx = o.columns[x.column].dashed, dy = o.columns[y.column].dashed;
        if (dx != dy) return dx;
        return a < b;
    });

    p.save();
    p.setClipRect(o.compact ? view.panel : QRectF(r.left(), r.top() - 8, r.width() + 8, r.height() + 9));
    p.setRenderHint(QPainter::Antialiasing, true);
    const int nb = int(buckets.size());
    for (const int j : drawOrder) {
        const Series &sr = series[j];
        const RowEntry &entry = registry[sr.reg];
        const LiveChartColumn &column = o.columns[sr.column];
        const bool dashed = !o.compact && column.dashed;
        const qreal width = o.compact ? 1.6 : dashed ? 1.5 : 2;
        QPen pen(entry.color, width);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        if (dashed) pen.setDashPattern({5 / width, 4 / width});
        const bool wash = entry.row.wash && column.wash;
        const QColor washColor = o.compact ? liveChartAlpha(entry.color, 36) : liveChartFaded(entry.color, 0.1);
        p.setOpacity(hoveredRow >= 0 && hoveredRow != sr.reg ? kLiveChartDimmedOpacity : 1);
        int segmentStart = -1;
        for (int i = 0; i <= nb; ++i) {
            const bool valid = i < nb && buckets[i].run && !std::isnan(valueAt(i, j));
            if (segmentStart >= 0 && (!valid || buckets[i].brk)) {
                paintSegment(p, j, segmentStart, i - 1, pen, entry.color, washColor, wash, dashed);
                segmentStart = -1;
            }
            if (valid && segmentStart < 0) segmentStart = i;
        }
        const float lastValue = valueAt(nb - 1, j);
        if (!buckets[nb - 1].run || std::isnan(lastValue)) continue;
        const QPointF end(buckets[nb - 1].x, yOf(lastValue));
        if (!needs(QRectF(end.x() - 6, end.y() - 6, 12, 12))) continue;
        if (o.compact) {
            p.setPen(Qt::NoPen);
            p.setBrush(entry.color);
            p.drawEllipse(end, 2.2, 2.2);
        } else {
            liveChartDot(p, end, 3.5, entry.color, c.base);
        }
    }
    p.restore();
}

void LiveChart::Impl::paintSegment(QPainter &p, int j, int a, int b, const QPen &pen, const QColor &color, const QColor &washColor, bool wash, bool dashed) {
    if (a == b) {
        const QPointF point(buckets[a].x, yOf(valueAt(a, j)));
        if (!needs(QRectF(point.x() - 3, point.y() - 3, 6, 6))) return;
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(point, 1.5, 1.5);
        return;
    }
    // Dash phase follows the path start, so a dashed line cut into runs would shift its dashes.
    if (fullPaint || dashed) {
        traceRun(p, j, a, b, pen, washColor, wash);
        return;
    }
    // Monotone tangents depend on nearby buckets only, so a three-bucket margin keeps the clipped part matching the full curve.
    int runStart = -1, runEnd = -1;
    for (int k = a; k < b; ++k) {
        const qreal y0 = yOf(valueAt(k, j)), y1 = yOf(valueAt(k + 1, j));
        const qreal top = std::min(y0, y1), bottom = wash ? view.plot.bottom() : std::max(y0, y1);
        if (!needs(QRectF(buckets[k].x - 4, top - 4, buckets[k + 1].x - buckets[k].x + 8, bottom - top + 8))) continue;
        const int from = std::max(a, k - 3), to = std::min(b, k + 4);
        if (runStart >= 0 && from <= runEnd) {
            runEnd = std::max(runEnd, to);
            continue;
        }
        if (runStart >= 0) traceRun(p, j, runStart, runEnd, pen, washColor, wash);
        runStart = from;
        runEnd = to;
    }
    if (runStart >= 0) traceRun(p, j, runStart, runEnd, pen, washColor, wash);
}

void LiveChart::Impl::traceRun(QPainter &p, int j, int a, int b, const QPen &pen, const QColor &washColor, bool wash) {
    xs.clear();
    ys.clear();
    for (int i = a; i <= b; ++i) {
        xs.push_back(buckets[i].x);
        ys.push_back(yOf(valueAt(i, j)));
    }
    if (wash) {
        fillPath.clear();
        liveChartTracePath(fillPath, xs, ys, s.smooth, slopes, tangents);
        fillPath.lineTo(xs.back(), view.plot.bottom());
        fillPath.lineTo(xs.front(), view.plot.bottom());
        fillPath.closeSubpath();
        p.fillPath(fillPath, washColor);
    }
    linePath.clear();
    liveChartTracePath(linePath, xs, ys, s.smooth, slopes, tangents);
    p.strokePath(linePath, pen);
}

void LiveChart::Impl::paintAxis(QPainter &p) {
    const qreal y = std::round(view.plot.bottom());
    for (const auto &label : axisLabels) p.fillRect(QRectF(std::round(label.x), y, 1, 4), c.axis);
    p.setFont(fontSmall);
    p.setPen(c.muted);
    for (size_t i = 0; i < axisLabels.size(); ++i) {
        const auto &label = axisLabels[i];
        if (i >= ov.labelVisible.size() || !ov.labelVisible[i] || !needs(label.rect)) continue;
        liveChartText(p, fmSmall, label.text, label.x - label.width / 2, label.rect.center().y(), kLiveChartLeft);
    }
}

void LiveChart::Impl::paintOverlay(QPainter &p) {
    if (!ov.active) return;
    const QRectF &r = view.plot;
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(QRectF(ov.cx, r.top(), 1, r.height()), c.hairline);
    if (ov.focus >= 0) p.fillRect(QRectF(r.left(), std::round(ov.hy), r.width(), 1), c.hairline);
    p.setRenderHint(QPainter::Antialiasing, true);
    for (const auto &dot : ov.dots) liveChartDot(p, dot.center, dot.r, dot.color, c.base);
    if (!ov.timeText.isEmpty()) paintPill(p, ov.timeText, ov.timePill);
    if (!ov.valueText.isEmpty()) paintPill(p, ov.valueText, ov.valuePill);
    if (needs(ov.card)) paintCard(p);
}

void LiveChart::Impl::paintCard(QPainter &p) {
    const QFont &font = o.compact ? fontSmall : fontBase;
    const QFont &valueFont = o.compact ? fontSmallBold : fontBold;
    const QFont &subFont = o.compact ? fontLabel : fontSmall;
    const QFontMetricsF &fm = o.compact ? fmSmall : fmBase;
    const QFontMetricsF &fmValue = o.compact ? fmSmallBold : fmBold;
    const QFontMetricsF &fmSub = o.compact ? fmLabel : fmSmall;
    const QRectF &card = ov.card;
    const qreal x = card.left(), y = card.top(), w = card.width(), rowH = ov.rowH;

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(c.border, 1));
    p.setBrush(c.base);
    p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), 7, 7);

    qreal ly = y + 8 + ov.headH / 2;
    p.setFont(valueFont);
    p.setPen(c.ink);
    liveChartText(p, fmValue, ov.head, x + ov.padX, ly, kLiveChartLeft);
    if (!ov.sub.isEmpty()) {
        p.setFont(subFont);
        p.setPen(c.muted);
        liveChartText(p, fmSub, ov.sub, x + ov.padX + ov.headW + 8, ly, kLiveChartLeft);
    }
    if (o.columns.size() > 1) {
        p.setFont(valueFont);
        p.setPen(c.muted);
        for (size_t k = 0; k < ov.columns.size(); ++k) liveChartText(p, fmValue, o.columns[ov.columns[k]].arrow, ov.rights[k], ly, kLiveChartRight);
    }
    const qreal separator = std::round(y + 8 + ov.headH + 2);
    p.fillRect(QRectF(x + 1, separator, w - 2, 1), c.grid);
    ly = separator + 0.5 + 4 + rowH / 2;
    for (const auto &line : ov.lines) {
        const qreal lx = x + ov.padX;
        if (line.type == CardLine::Row) {
            const auto &entry = registry[line.reg];
            if (line.focus) p.fillRect(QRectF(x + 3, ly - rowH / 2, w - 6, rowH), c.hover);
            p.setPen(Qt::NoPen);
            p.setBrush(entry.color);
            p.drawEllipse(QPointF(lx + 4, ly), 3.5, 3.5);
            p.setFont(font);
            p.setPen(c.ink);
            liveChartText(p, fm, entry.shortName, lx + 14, ly, kLiveChartLeft);
            if (!entry.row.kind.isEmpty()) {
                p.setFont(subFont);
                p.setPen(c.muted);
                liveChartText(p, fmSub, entry.row.kind, lx + 14 + entry.cardNameWidth + 6, ly, kLiveChartLeft);
            }
        } else {
            p.setFont(font);
            p.setPen(c.ink2);
            if (line.type == CardLine::Total) liveChartText(p, fm, totalText, lx + 14, ly, kLiveChartLeft);
            else liveChartText(p, fm, line.text, lx, ly, kLiveChartLeft);
        }
        if (line.values >= 0) {
            p.setFont(valueFont);
            p.setPen(c.ink);
            for (size_t k = 0; k < ov.columns.size(); ++k) liveChartText(p, fmValue, ov.values[size_t(line.values) + k], ov.rights[k], ly, kLiveChartRight);
        }
        ly += rowH;
    }
}

void LiveChart::Impl::paintPill(QPainter &p, const QString &text, const QRectF &rect) {
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(c.ink);
    p.drawRoundedRect(rect, 4, 4);
    p.setFont(fontSmallBold);
    p.setPen(c.base);
    liveChartText(p, fmSmallBold, text, rect.center().x(), rect.center().y() + 0.5, kLiveChartCenter);
}

void LiveChart::Impl::paintMessage(QPainter &p) {
    const QRectF &r = view.plot;
    if (!needs(r)) return;
    const qreal cx = r.center().x();
    qreal y = r.center().y() - (messageTitleH + messageLines.size() * messageLineH) / 2 + messageTitleH * 0.4;
    p.setFont(fontTitle);
    liveChartHalo(p, fmTitle, view.messageTitle, cx, y, c.ink, c.base, kLiveChartCenter);
    p.setFont(fontBase);
    for (const auto &text : messageLines) {
        y += messageLineH;
        liveChartHalo(p, fmBase, text, cx, y + 3, c.ink2, c.base, kLiveChartCenter);
    }
}

QString LiveChart::Impl::markerText(const SeriesBuffer::Marker &marker, bool note) {
    switch (static_cast<LiveChart::Marker>(marker.kind)) {
        case LiveChart::Marker::Started:
            return QStringLiteral("▶ ") + (note ? LiveChart::tr("Started %1").arg(marker.text) : ShortDisplayName(marker.text));
        case LiveChart::Marker::Switched:
            return QStringLiteral("⇄ ") + (note ? LiveChart::tr("Switched to %1").arg(marker.text) : ShortDisplayName(marker.text));
        case LiveChart::Marker::Stopped:
            break;
    }
    return QStringLiteral("■ ") + LiveChart::tr("Stopped");
}

const LiveChart::Impl::Hit *LiveChart::Impl::hitAt(const QPointF &pos) const {
    for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
        if (it->rect.contains(pos)) return &*it;
    }
    return nullptr;
}

int LiveChart::Impl::nearestByX(qreal x) const {
    int lo = 0, hi = int(buckets.size()) - 1;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (buckets[mid].x < x) lo = mid + 1;
        else hi = mid;
    }
    if (lo > 0 && std::abs(buckets[lo - 1].x - x) <= std::abs(buckets[lo].x - x)) --lo;
    return lo;
}

int LiveChart::Impl::nearestByTime(qint64 t) const {
    int lo = 0, hi = int(buckets.size()) - 1;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (buckets[mid].tx < t) lo = mid + 1;
        else hi = mid;
    }
    if (lo > 0 && std::abs(buckets[lo - 1].tx - t) <= std::abs(buckets[lo].tx - t)) --lo;
    return lo;
}

int LiveChart::Impl::findSeries(int reg, int column) const {
    for (size_t j = 0; j < series.size(); ++j) {
        if (series[j].reg == reg && series[j].column == column) return int(j);
    }
    return -1;
}

int LiveChart::Impl::pickSeries(int bucket, qreal y) const {
    int best = -1;
    qreal bestDistance = std::numeric_limits<qreal>::infinity();
    for (size_t j = 0; j < series.size(); ++j) {
        if (!series[j].on) continue;
        const float v = valueAt(bucket, int(j));
        if (std::isnan(v)) continue;
        const qreal distance = std::abs(yOf(v) - y);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = int(j);
        }
    }
    return best;
}

void LiveChart::Impl::resolveAnchor() {
    ap = -1;
    apSeries = -1;
    if (!anchor.set) return;
    const int index = nearestByTime(anchor.t);
    const Bucket &b = buckets[index];
    if (std::abs(b.tx - anchor.t) > view.kms || b.x < view.plot.left()) {
        anchor = {};
        return;
    }
    if (b.run) {
        int j = findSeries(anchor.reg, anchor.column);
        if (j < 0 || !series[j].on || std::isnan(valueAt(index, j))) {
            j = pickSeries(index, pointerInside ? pointer.y() : view.plot.center().y());
            if (j < 0) return;
            anchor.reg = series[j].reg;
            anchor.column = series[j].column;
        }
        apSeries = j;
    }
    ap = index;
}

void LiveChart::Impl::hoverAt(const QPointF &pos, const QPoint &global) {
    const Hit *hit = hitAt(pos);
    const HitType type = hit ? hit->type : NoHit;
    const int index = hit ? hit->index : -1;
    const bool hoverChanged = type != hoverHit.type || index != hoverHit.index;
    hoverHit = hit ? *hit : Hit{};

    Anchor next;
    int nextAp = -1, nextSeries = -1;
    const QRectF &zone = o.compact ? view.panel : view.plot;
    if (!hit && view.valid && o.crosshair && !view.allHidden && zone.contains(pos)) {
        const int bi = nearestByX(pos.x());
        const Bucket &b = buckets[bi];
        if (b.x >= view.plot.left()) {
            const int j = b.run ? pickSeries(bi, pos.y()) : -1;
            if (!b.run || j >= 0) {
                next.set = true;
                next.t = b.tx;
                if (j >= 0) {
                    next.reg = series[j].reg;
                    next.column = series[j].column;
                }
                nextAp = bi;
                nextSeries = j;
            }
        }
    }
    const bool anchorChanged = !(next == anchor);
    if (!hoverChanged && !anchorChanged) return;
    const bool partial = !hoverChanged && !layoutDirty;
    QRegion region;
    if (partial) region = overlayRegion();
    anchor = next;
    ap = nextAp;
    apSeries = nextSeries;
    if (!layoutDirty) computeOverlay();
    if (hoverChanged) {
        updateCursor(hit != nullptr);
        updateToolTip(hit, global);
    }
    if (partial) {
        region += overlayRegion();
        requestRepaint(region);
    } else {
        requestRepaint();
    }
}

void LiveChart::Impl::dropPointer() {
    pointerInside = false;
    if (tooltipShown) {
        QToolTip::hideText();
        tooltipShown = false;
    }
    updateCursor(false);
    const bool hovered = hoverHit.type != NoHit;
    if (!hovered && !anchor.set) return;
    const bool partial = !hovered && !layoutDirty;
    QRegion region;
    if (partial) region = overlayRegion();
    anchor = {};
    ap = -1;
    apSeries = -1;
    hoverHit = {};
    if (!layoutDirty) computeOverlay();
    if (partial) {
        region += overlayRegion();
        requestRepaint(region);
    } else {
        requestRepaint();
    }
}

void LiveChart::Impl::updateCursor(bool hand) {
    if (hand == handCursor) return;
    handCursor = hand;
    if (hand) q->setCursor(Qt::PointingHandCursor);
    else q->unsetCursor();
}

void LiveChart::Impl::updateToolTip(const Hit *hit, const QPoint &global) {
    if (tooltipShown) {
        QToolTip::hideText();
        tooltipShown = false;
    }
    if (!hit || hit->type != RowHit) return;
    const auto &entry = registry[hit->index];
    if (entry.shortName == entry.row.label) return;
    const QString text = entry.row.kind.isEmpty() ? entry.row.label : entry.row.label + QStringLiteral(" (") + entry.row.kind + QLatin1Char(')');
    // Profile names are untrusted: QToolTip renders anything that looks like markup as rich text.
    QToolTip::showText(global, Qt::convertFromPlainText(text, Qt::WhiteSpaceNormal), q, hit->rect.toAlignedRect());
    tooltipShown = true;
}

void LiveChart::Impl::activate(const Hit &hit, bool alt) {
    switch (hit.type) {
        case RowHit:
            if (alt) {
                soloRow(hit.index);
            } else if (s.hiddenRows.removeAll(registry[hit.index].row.key) == 0) {
                s.hiddenRows.append(registry[hit.index].row.key);
            }
            break;
        case ColumnHit:
            if (s.hiddenColumns.removeAll(o.columns[hit.index].key) == 0) s.hiddenColumns.append(o.columns[hit.index].key);
            break;
        case RangeHit:
            if (s.range == hit.index) return;
            s.range = hit.index;
            break;
        case NoHit:
            return;
    }
    notifyState();
}

void LiveChart::Impl::soloRow(int reg) {
    const QString key = registry[reg].row.key;
    const bool already = std::all_of(legendRows.begin(), legendRows.end(), [&](int r) {
        const QString &k = registry[r].row.key;
        return s.hiddenRows.contains(k) == (k != key);
    });
    // Only legend rows change: hidden keys of rows not shown now must survive in the saved state.
    for (const int r : legendRows) {
        const QString &k = registry[r].row.key;
        s.hiddenRows.removeAll(k);
        if (!already && k != key) s.hiddenRows.append(k);
    }
}

LiveChart::LiveChart(LiveChartOptions options, QWidget *parent) : QWidget(parent), d(std::make_unique<Impl>(this, std::move(options))) {
    setLayoutDirection(Qt::LeftToRight);
    setMouseTracking(true);
    if (!d->o.compact) setAttribute(Qt::WA_OpaquePaintEvent);
    connect(&d->animation, &QTimer::timeout, this, [this] { d->animationTick(); });
    connect(themeManager(), &ThemeManager::themeChanged, this, [this] {
        d->refreshRowColors();
        d->invalidate();
    });
    d->refreshFonts();
}

LiveChart::~LiveChart() {
    QObject::disconnect(d->windowConnection);
}

void LiveChart::setRows(const QList<LiveChartRow> &rows) {
    std::vector<Impl::RowEntry> next;
    next.reserve(size_t(rows.size()) + d->registry.size());
    for (const auto &row : rows) {
        Impl::RowEntry entry;
        entry.row = row;
        entry.current = true;
        next.push_back(std::move(entry));
    }
    for (auto &old : d->registry) {
        const bool redefined = std::any_of(rows.begin(), rows.end(), [&](const LiveChartRow &row) { return row.key == old.row.key; });
        if (redefined) continue;
        old.current = false;
        next.push_back(std::move(old));
    }
    d->registry = std::move(next);
    d->currentCount = int(rows.size());
    d->buf.releaseDeadColumns(d->currentKeys());
    d->resolveColumns();
    d->refreshRowMetrics();
    d->refreshRowColors();
    d->resetPointerState();
    d->invalidate();
}

void LiveChart::addPoint(qint64 monoMs, bool running, const QVector<double> &values) {
    if (!d->buf.isEmpty() && monoMs <= d->buf.latestTime()) monoMs = d->buf.latestTime() + 1;
    d->buf.push(monoMs, running);
    const int nc = int(d->o.columns.size());
    const int count = int(std::min<qsizetype>(values.size(), qsizetype(d->currentCount) * nc));
    for (int i = 0; i < count; ++i) {
        const double v = values[i];
        if (!std::isnan(v)) d->buf.setLatest(d->registry[size_t(i / nc)].columns[i % nc], float(v));
    }
    d->invalidate();
}

void LiveChart::addMarker(qint64 monoMs, Marker kind, const QString &text) {
    d->buf.addMarker(monoMs, int(kind), text);
    if (d->o.markers) d->invalidate();
}

void LiveChart::setBlocked(const QString &title, const QString &body) {
    if (title == d->blockedTitle && body == d->blockedBody) return;
    d->blockedTitle = title;
    d->blockedBody = body;
    d->anchor = {};
    d->ap = -1;
    d->apSeries = -1;
    d->invalidate();
}

void LiveChart::clearHistory() {
    d->buf.clear();
    d->buf.releaseDeadColumns(d->currentKeys());
    d->resolveColumns();
    d->drawnTop = std::numeric_limits<double>::quiet_NaN();
    d->resetPointerState();
    d->invalidate();
}

void LiveChart::addMenuAction(QAction *action) {
    d->menuActions.append(action);
}

LiveChartState LiveChart::state() const {
    return d->s;
}

void LiveChart::setState(const LiveChartState &state) {
    d->s = state;
    if (!d->o.ranges.isEmpty()) d->s.range = std::clamp(d->s.range, 0, int(d->o.ranges.size()) - 1);
    d->invalidate();
}

void LiveChart::paintEvent(QPaintEvent *event) {
    const QRegion &region = event->region();
    const bool full = region.rectCount() == 1 && event->rect().contains(rect());
    // A partial expose after a layout change would leave the rest of the widget showing the old layout.
    if (!full && (d->layoutDirty || d->pendingRepaint)) update();
    if (full) d->pendingRepaint = false;
    if (d->layoutDirty) d->layout();
    QPainter p(this);
    d->fullPaint = full;
    d->dirty = &region;
    d->paint(p);
    d->dirty = nullptr;
}

void LiveChart::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    d->layoutDirty = true;
    if (d->pendingRepaint) d->requestRepaint();
}

void LiveChart::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
    if (d->pendingRepaint) d->requestRepaint();
}

void LiveChart::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    d->watchWindow();
    if (d->pendingRepaint) update();
}

void LiveChart::changeEvent(QEvent *event) {
    switch (event->type()) {
        case QEvent::FontChange:
        case QEvent::LanguageChange:
            d->refreshFonts();
            d->invalidate();
            break;
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            d->refreshRowColors();
            d->invalidate();
            break;
        default:
            break;
    }
    QWidget::changeEvent(event);
}

void LiveChart::mouseMoveEvent(QMouseEvent *event) {
    d->pointerInside = true;
    d->pointer = event->position();
    d->hoverAt(event->position(), event->globalPosition().toPoint());
    QWidget::mouseMoveEvent(event);
}

void LiveChart::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (const auto *hit = d->hitAt(event->position())) {
            d->activate(*hit, event->modifiers().testFlag(Qt::AltModifier));
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}


void LiveChart::leaveEvent(QEvent *event) {
    d->dropPointer();
    QWidget::leaveEvent(event);
}

void LiveChart::contextMenuEvent(QContextMenuEvent *event) {
    if (!d->o.contextMenu) {
        QWidget::contextMenuEvent(event);
        return;
    }
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction *smooth = menu->addAction(tr("Smooth lines"));
    smooth->setCheckable(true);
    smooth->setChecked(d->s.smooth);
    connect(smooth, &QAction::toggled, this, [this](bool on) {
        d->s.smooth = on;
        d->notifyState();
    });
    if (d->o.markers) {
        QAction *markers = menu->addAction(tr("Profile events"));
        markers->setCheckable(true);
        markers->setChecked(d->s.markers);
        connect(markers, &QAction::toggled, this, [this](bool on) {
            d->s.markers = on;
            d->notifyState();
        });
    }
    menu->addSeparator();
    connect(menu->addAction(tr("Show all lines")), &QAction::triggered, this, [this] {
        if (d->s.hiddenRows.isEmpty() && d->s.hiddenColumns.isEmpty()) return;
        d->s.hiddenRows.clear();
        d->s.hiddenColumns.clear();
        d->notifyState();
    });
    connect(menu->addAction(tr("Clear history")), &QAction::triggered, this, &LiveChart::clearHistory);
    bool separated = false;
    for (const auto &action : std::as_const(d->menuActions)) {
        if (!action) continue;
        if (!separated) {
            menu->addSeparator();
            separated = true;
        }
        menu->addAction(action.data());
    }
    menu->popup(event->globalPos());
    event->accept();
}
