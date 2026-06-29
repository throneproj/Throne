#include "include/ui/stats/dialog_traffic_stats.h"

#include "include/ui/stats/TrafficChartWidget.h"

#include "include/database/DatabaseManager.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/TrafficStatsRepo.h"
#include "include/database/entities/Profile.h"
#include "include/stats/traffic/TrafficStatsManager.hpp"
#include "include/global/Utils.hpp"

#include <QComboBox>
#include <QDateTime>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>

#include <algorithm>

#include "include/configs/generate.h"

namespace {
    // Cap on named rows in each breakdown table; usage past this is collapsed into
    // a single "Other" row so the table stays a readable top-N.
    constexpr int kMaxBreakdownRows = 9;

    // Table cell that sorts on its raw byte value rather than the formatted text,
    // so "1.00 GiB" ranks above "900 MiB". Right-aligned, like a figure column.
    class TrafficStatsSizeItem : public QTableWidgetItem {
    public:
        TrafficStatsSizeItem(const QString& text, long long value) : QTableWidgetItem(text) {
            QTableWidgetItem::setData(Qt::UserRole, QVariant::fromValue<qlonglong>(value));
            setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
        bool operator<(const QTableWidgetItem& other) const override {
            return data(Qt::UserRole).toLongLong() < other.data(Qt::UserRole).toLongLong();
        }
    };
}

DialogTrafficStats::DialogTrafficStats(QWidget* parent) : QDialog(parent), ui(new Ui::DialogTrafficStats) {
    ui->setupUi(this);

    // The chart and tables take the lion's share of the height; the top controls
    // stay at their natural size. (Box-layout stretch factors don't round-trip
    // cleanly through uic, so they're set here rather than in the .ui.)
    ui->verticalLayout->setStretch(1, 2); // chart
    ui->verticalLayout->setStretch(2, 3); // tabs

    // Section resize modes are per-column, so they stay in code rather than the
    // .ui: the name columns stretch to fill, the figure columns hug their content.
    ui->profileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->profileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c = 2; c <= 4; ++c)
        ui->profileTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    ui->appTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c <= 3; ++c)
        ui->appTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    // The chart and summary reflect the active tab's dimension, so refresh when
    // switching tabs, changing the period, or on explicit request.
    connect(ui->refreshBtn, &QPushButton::clicked, this, [this] { refresh(); });
    connect(ui->periodCombo, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });
    connect(ui->tabs, &QTabWidget::currentChanged, this, [this](int) { refresh(); });

    refresh();
}

DialogTrafficStats::~DialogTrafficStats() {
    delete ui;
}

long long DialogTrafficStats::selectedWindowSecs() const {
    switch (ui->periodCombo->currentIndex()) {
        case 1: return 7LL * 86400LL;
        case 2: return 30LL * 86400LL;
        case 3: return 90LL * 86400LL;
        case 0:
        default: return 24LL * 3600LL;
    }
}

long long DialogTrafficStats::selectedBucketSecs() const {
    // Hourly buckets for the 24h view, daily buckets for the longer ranges.
    return ui->periodCombo->currentIndex() == 0 ? 3600LL : 86400LL;
}

void DialogTrafficStats::refresh() {
    auto* repo = Configs::dataManager ? Configs::dataManager->trafficStatsRepo.get() : nullptr;
    if (!repo) return;

    // Persist the in-progress minute so the dashboard reflects up-to-the-moment
    // usage rather than only what has already rolled over to disk.
    Stats::trafficStatsManager->Flush();

    const long long now = QDateTime::currentSecsSinceEpoch();
    const long long window = selectedWindowSecs();
    const long long bucket = selectedBucketSecs();
    const long long from = now - window;
    // Buckets are stored on UTC boundaries; align them to the viewer's local
    // calendar so an "hour"/"day" bar starts on the local clock, not UTC's.
    const long long tzOffset = QDateTime::currentDateTime().offsetFromUtc();

    populateProfileTable(from, now);
    populateAppTable(from, now);

    const bool byApp = ui->tabs->currentIndex() == 1;
    const auto series = byApp ? repo->QueryAppSeries(from, now, bucket, tzOffset)
                              : repo->QueryConfigSeries(from, now, bucket, tzOffset);

    QHash<long long, Configs::TrafficSeriesPoint> byBucket;
    byBucket.reserve(series.size());
    long long totalUp = 0, totalDown = 0;
    for (const auto& pt : series) {
        byBucket.insert(pt.bucket_start, pt);
        totalUp += pt.up;
        totalDown += pt.down;
    }

    // Build a contiguous bar list (gaps filled with zeros) so the time axis is
    // continuous even when some buckets saw no traffic. Align to the local boundary
    // with the same offset the query used, so each bar's key matches a series point.
    const long long alignedFrom = ((from + tzOffset) / bucket) * bucket - tzOffset;
    QList<TrafficChartWidget::Bar> bars;
    for (long long b = alignedFrom; b < now; b += bucket) {
        TrafficChartWidget::Bar bar;
        bar.bucketStart = b;
        if (const auto it = byBucket.constFind(b); it != byBucket.constEnd()) {
            bar.down = it->down;
            bar.up = it->up;
        }
        // Label marks the bucket's start; the chart's tooltip shows the full range.
        bar.label = bucket >= 86400LL ? QDateTime::fromSecsSinceEpoch(b).toString("MM/dd")
                                      : QDateTime::fromSecsSinceEpoch(b).toString("HH:mm");
        bars.append(bar);
    }
    const int stride = qMax(1, (static_cast<int>(bars.size()) + 7) / 8);
    ui->chart->setData(bars, stride, bucket);

    ui->summaryLabel->setText(tr("Download: %1     Upload: %2     Total: %3")
                               .arg(ReadableSize(totalDown), ReadableSize(totalUp),
                                    ReadableSize(totalDown + totalUp)));
}

void DialogTrafficStats::populateProfileTable(long long fromSecs, long long toSecs) {
    auto* repo = Configs::dataManager->trafficStatsRepo.get();
    auto usage = repo->QueryConfigUsage(fromSecs, toSecs);
    QHash<int, Configs::ConfigMetaRow> meta;
    for (const auto& m : repo->GetAllConfigMeta()) meta.insert(m.profile_id, m);

    // Rank by total, show the busiest few, and fold any remainder into one "Other"
    // row. The data is sorted here (not just left to the table) so the cut is by
    // total even if the user later re-sorts the visible rows by another column.
    std::sort(usage.begin(), usage.end(), [](const Configs::ConfigUsage& a, const Configs::ConfigUsage& b) {
        return (a.down + a.up) > (b.down + b.up);
    });
    const int count = static_cast<int>(usage.size());
    const int shown = qMin(count, kMaxBreakdownRows);
    const bool hasOther = count > kMaxBreakdownRows;
    long long otherDown = 0, otherUp = 0;
    for (int i = shown; i < count; ++i) {
        otherDown += usage[i].down;
        otherUp += usage[i].up;
    }

    ui->profileTable->setSortingEnabled(false);
    ui->profileTable->setRowCount(shown + (hasOther ? 1 : 0));
    for (int i = 0; i < shown; ++i) {
        const auto& u = usage[i];
        QString name, group;
        if (const auto it = meta.constFind(u.profile_id); it != meta.constEnd()) {
            name = it->name;
            group = it->group_name;
        }
        if (name.isEmpty()) {
            if (u.profile_id == Stats::DIRECT_STAT_PROFILE_ID) {
                name = tr("Direct");
            } else if (const auto prof = Configs::dataManager->profilesRepo->GetProfile(u.profile_id)) {
                name = prof->name;
            } else if (u.profile_id == Configs::warpProfileID) {
                name = "built-in warp";
            } else {
                name = tr("Profile #%1 (deleted)").arg(u.profile_id);
            }
        }
        ui->profileTable->setItem(i, 0, new QTableWidgetItem(name));
        ui->profileTable->setItem(i, 1, new QTableWidgetItem(group));
        ui->profileTable->setItem(i, 2, new TrafficStatsSizeItem(ReadableSize(u.down), u.down));
        ui->profileTable->setItem(i, 3, new TrafficStatsSizeItem(ReadableSize(u.up), u.up));
        ui->profileTable->setItem(i, 4, new TrafficStatsSizeItem(ReadableSize(u.down + u.up), u.down + u.up));
    }
    if (hasOther) {
        ui->profileTable->setItem(shown, 0, new QTableWidgetItem(tr("Other")));
        ui->profileTable->setItem(shown, 1, new QTableWidgetItem(QString()));
        ui->profileTable->setItem(shown, 2, new TrafficStatsSizeItem(ReadableSize(otherDown), otherDown));
        ui->profileTable->setItem(shown, 3, new TrafficStatsSizeItem(ReadableSize(otherUp), otherUp));
        ui->profileTable->setItem(shown, 4, new TrafficStatsSizeItem(ReadableSize(otherDown + otherUp), otherDown + otherUp));
    }
    ui->profileTable->setSortingEnabled(true);
    ui->profileTable->sortItems(4, Qt::DescendingOrder);
}

void DialogTrafficStats::populateAppTable(long long fromSecs, long long toSecs) {
    auto* repo = Configs::dataManager->trafficStatsRepo.get();
    auto usage = repo->QueryAppUsage(fromSecs, toSecs);

    // Same top-N + "Other" treatment as the profile table.
    std::sort(usage.begin(), usage.end(), [](const Configs::AppUsage& a, const Configs::AppUsage& b) {
        return (a.down + a.up) > (b.down + b.up);
    });
    const int count = static_cast<int>(usage.size());
    const int shown = qMin(count, kMaxBreakdownRows);
    const bool hasOther = count > kMaxBreakdownRows;
    long long otherDown = 0, otherUp = 0;
    for (int i = shown; i < count; ++i) {
        otherDown += usage[i].down;
        otherUp += usage[i].up;
    }

    ui->appTable->setSortingEnabled(false);
    ui->appTable->setRowCount(shown + (hasOther ? 1 : 0));
    for (int i = 0; i < shown; ++i) {
        const auto& u = usage[i];
        QString name = u.process_name.isEmpty() ? tr("Unknown") : u.process_name;
        ui->appTable->setItem(i, 0, new QTableWidgetItem(name));
        ui->appTable->setItem(i, 1, new TrafficStatsSizeItem(ReadableSize(u.down), u.down));
        ui->appTable->setItem(i, 2, new TrafficStatsSizeItem(ReadableSize(u.up), u.up));
        ui->appTable->setItem(i, 3, new TrafficStatsSizeItem(ReadableSize(u.down + u.up), u.down + u.up));
    }
    if (hasOther) {
        ui->appTable->setItem(shown, 0, new QTableWidgetItem(tr("Other")));
        ui->appTable->setItem(shown, 1, new TrafficStatsSizeItem(ReadableSize(otherDown), otherDown));
        ui->appTable->setItem(shown, 2, new TrafficStatsSizeItem(ReadableSize(otherUp), otherUp));
        ui->appTable->setItem(shown, 3, new TrafficStatsSizeItem(ReadableSize(otherDown + otherUp), otherDown + otherUp));
    }
    ui->appTable->setSortingEnabled(true);
    ui->appTable->sortItems(3, Qt::DescendingOrder);
}
