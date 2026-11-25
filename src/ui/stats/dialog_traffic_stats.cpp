#include "include/ui/stats/dialog_traffic_stats.h"

#include "include/ui/stats/TrafficChartWidget.h"

#include "include/database/DatabaseManager.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/TrafficStatsRepo.h"
#include "include/database/entities/Profile.h"
#include "include/stats/traffic/TrafficStatsManager.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ItemViewFit.h"

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
    // Named rows per breakdown table; the rest is folded into one "Other" row.
    constexpr int kMaxBreakdownRows = 9;

    // Sorts on the raw byte value; the default compares the formatted text, ranking "900 MiB" above "1.00 GiB".
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

    // Box-layout stretch factors don't round-trip through uic, so they live here, not in the .ui.
    ui->verticalLayout->setStretch(1, 2); // chart
    ui->verticalLayout->setStretch(2, 3); // tabs

    for (auto *table : {ui->profileTable, ui->appTable}) table->setItemDelegate(new ItemViewFit::Delegate(table));
    ui->profileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->profileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    for (int c = 2; c <= 4; ++c)
        ui->profileTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);

    ui->appTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c <= 3; ++c)
        ui->appTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);

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
    return ui->periodCombo->currentIndex() == 0 ? 3600LL : 86400LL;
}

void DialogTrafficStats::refresh() {
    auto* repo = Configs::dataManager ? Configs::dataManager->trafficStatsRepo.get() : nullptr;
    if (!repo) return;

    // The in-progress minute lives in memory until flushed.
    Stats::trafficStatsManager->Flush();

    const long long now = QDateTime::currentSecsSinceEpoch();
    const long long window = selectedWindowSecs();
    const long long bucket = selectedBucketSecs();
    const long long from = now - window;
    // Buckets are stored on UTC boundaries; this shifts them onto the viewer's local calendar.
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

    // Must align with the same offset the query used, or a bar's key won't match a series point.
    const long long alignedFrom = ((from + tzOffset) / bucket) * bucket - tzOffset;
    QList<TrafficChartWidget::Bar> bars;
    for (long long b = alignedFrom; b < now; b += bucket) {
        TrafficChartWidget::Bar bar;
        bar.bucketStart = b;
        if (const auto it = byBucket.constFind(b); it != byBucket.constEnd()) {
            bar.down = it->down;
            bar.up = it->up;
        }
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

    // Sorted here, not by the table, so the top-N cut is by total even after the user re-sorts.
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
            } else if (const auto exit = Configs::ExitOfProfileID(u.profile_id); exit != Configs::ExitThrough::Server) {
                name = tr("%1 exit").arg(Configs::ExitName(exit));
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
