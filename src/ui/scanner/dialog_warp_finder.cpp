#include "include/ui/scanner/dialog_warp_finder.h"

#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <utility>

#include "include/configs/EndpointOverride.h"
#include "include/database/entities/Profile.h"
#include "include/global/Utils.hpp"
#include "include/scanner/ScanManager.h"
#include "include/ui/scanner/IpEntriesModel.h"
#include "include/ui/widget/ElidingLabel.hpp"

namespace {
    constexpr int kWarpFinderRefreshMs = 150;
    constexpr int kWarpFinderKeyRole = Qt::UserRole;

    enum WarpFinderColumn { kWarpFinderEndpointColumn, kWarpFinderLatencyColumn, kWarpFinderLossColumn, kWarpFinderColumnCount };

    // The theme's overlay sheet matches on the property, so a change needs a repolish.
    void warpFinderSetColorRole(QWidget *widget, const QString &role) {
        if (widget->property("colorRole").toString() == role) return;
        widget->setProperty("colorRole", role);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }

    QString warpFinderKey(const Configs::IpListEntry &entry) {
        return QStringLiteral("%1|%2").arg(entry.cidr).arg(entry.port);
    }

    bool warpFinderSameHits(const QList<Configs::IpListEntry> &a, const QList<Configs::IpListEntry> &b) {
        return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](const Configs::IpListEntry &x, const Configs::IpListEntry &y) {
            return x.cidr == y.cidr && x.port == y.port && x.latencyMs == y.latencyMs && x.lossPct == y.lossPct;
        });
    }
}

DialogWarpFinder::DialogWarpFinder(std::shared_ptr<Configs::Profile> base_, const QString &overrideOrigin_, QWidget *parent)
    : QDialog(parent), base(std::move(base_)), overrideOrigin(overrideOrigin_) {
    setWindowTitle(tr("Find WARP endpoint"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);

    auto *layout = new QVBoxLayout(this);

    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    statusLabel->setTextFormat(Qt::PlainText);
    layout->addWidget(statusLabel);

    detailLabel = new ElidingLabel(this);
    detailLabel->setProperty("colorRole", QStringLiteral("muted"));
    detailLabel->hide();
    layout->addWidget(detailLabel);

    progressRow = new QWidget(this);
    auto *progressLayout = new QHBoxLayout(progressRow);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressBar = new QProgressBar(progressRow);
    progressBar->setTextVisible(true);
    foundLabel = new QLabel(progressRow);
    progressLayout->addWidget(progressBar, 1);
    progressLayout->addWidget(foundLabel);
    layout->addWidget(progressRow);

    noticeLabel = new QLabel(this);
    noticeLabel->setWordWrap(true);
    noticeLabel->setTextFormat(Qt::PlainText);
    noticeLabel->hide();
    layout->addWidget(noticeLabel);

    resultsView = new QTreeWidget(this);
    resultsView->setColumnCount(kWarpFinderColumnCount);
    resultsView->setHeaderLabels({tr("Endpoint"), IpEntriesModel::tr("Latency"), IpEntriesModel::tr("Loss")});
    resultsView->setRootIsDecorated(false);
    resultsView->setUniformRowHeights(true);
    resultsView->setSelectionMode(QAbstractItemView::SingleSelection);
    resultsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *header = resultsView->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(kWarpFinderEndpointColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(kWarpFinderLatencyColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(kWarpFinderLossColumn, QHeaderView::ResizeToContents);
    layout->addWidget(resultsView, 1);

    placeholderLabel = new QLabel(tr("Working endpoints appear here"), resultsView->viewport());
    placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLabel->setWordWrap(true);
    placeholderLabel->setProperty("colorRole", QStringLiteral("muted"));
    auto *placeholderLayout = new QVBoxLayout(resultsView->viewport());
    placeholderLayout->addWidget(placeholderLabel);

    auto *buttonRow = new QHBoxLayout();
    runButton = new QPushButton(this);
    runButton->setAutoDefault(false);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setAutoDefault(false);
    useButton = buttons->addButton(tr("Use endpoint"), QDialogButtonBox::AcceptRole);
    useButton->setDefault(true);
    useButton->setEnabled(false);
    buttonRow->addWidget(runButton);
    buttonRow->addWidget(buttons, 1);
    layout->addLayout(buttonRow);

    refreshTimer = new QTimer(this);
    refreshTimer->setSingleShot(true);
    refreshTimer->setInterval(kWarpFinderRefreshMs);
    connect(refreshTimer, &QTimer::timeout, this, [this] { refresh(); });

    connect(runButton, &QPushButton::clicked, this, [this] { runClicked(); });
    connect(useButton, &QPushButton::clicked, this, [this] { useSelected(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(resultsView, &QTreeWidget::itemDoubleClicked, this, [this] { useSelected(); });
    connect(resultsView, &QTreeWidget::itemSelectionChanged, this, [this] {
        const bool any = !resultsView->selectedItems().isEmpty();
        if (!populating) selectionByUser = any;
        useButton->setEnabled(any);
    });

    auto *manager = Scanner::ScanManager::instance();
    const auto onRunChanged = [this](int id) {
        if (runId < 0 && id == runId) scheduleRefresh();
    };
    connect(manager, &Scanner::ScanManager::progressChanged, this, onRunChanged);
    connect(manager, &Scanner::ScanManager::resultsChanged, this, onRunChanged);
    connect(manager, &Scanner::ScanManager::statusChanged, this, onRunChanged);

    start();

    const auto *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const int line = fontMetrics().height();
    const QSize wanted = sizeHint().expandedTo(QSize(line * 30, line * 22));
    resize(scr != nullptr ? wanted.boundedTo(scr->availableGeometry().size()) : wanted);
}

DialogWarpFinder::~DialogWarpFinder() {
    forgetRun();
}

QString DialogWarpFinder::OverrideOrigin(const std::shared_ptr<Configs::Profile> &profile) {
    if (profile == nullptr || !Configs::EndpointOverrideBlocker(profile).isEmpty()) return {};
    const auto endpoint = Configs::ResolveEndpointSource(Configs::EffectiveEndpointSource(*profile));
    return endpoint.address.isEmpty() ? QString() : endpoint.origin;
}

void DialogWarpFinder::done(int result) {
    forgetRun();
    QDialog::done(result);
}

void DialogWarpFinder::start() {
    forgetRun();
    startError.clear();
    selectionByUser = false;
    showHits({});
    clock.start();
    QString error;
    if (const int id = Scanner::ScanManager::instance()->StartWarpFinder(base, &error); id < 0) {
        runId = id;
    } else {
        startError = error.isEmpty() ? tr("The search could not be started") : error;
    }
    refresh();
}

void DialogWarpFinder::forgetRun() {
    refreshTimer->stop();
    if (runId >= 0) return;
    Scanner::ScanManager::instance()->ForgetWarpFinder(std::exchange(runId, 0));
}

void DialogWarpFinder::scheduleRefresh() {
    if (!refreshTimer->isActive()) refreshTimer->start();
}

void DialogWarpFinder::refresh() {
    refreshTimer->stop();
    auto *manager = Scanner::ScanManager::instance();
    const bool started = runId < 0;
    const bool running = started && manager->IsRunning(runId);
    const auto live = started ? manager->LiveState(runId) : Scanner::ScanLiveState{};
    const int found = static_cast<int>(live.hits.size());

    showHits(live.hits);

    QString status;
    QString role;
    if (!started) {
        status = startError;
        role = QStringLiteral("danger");
    } else if (running) {
        if (live.stopping) status = tr("Stopping");
        else status = live.stage.isEmpty() ? Scanner::ScanManager::tr("Preparing") : live.stage;
    } else if (found > 0) {
        const qint64 seconds = live.total > 0 ? static_cast<qint64>(qMin(live.tested, live.total)) : clock.elapsed() / 1000;
        status = tr("Found %n endpoint(s) in %1 s", nullptr, found).arg(seconds);
        role = QStringLiteral("success");
    } else if (!live.error.isEmpty()) {
        status = live.error;
        role = QStringLiteral("danger");
    } else {
        status = live.log.isEmpty() ? tr("No working endpoint found") : live.log.constLast();
    }
    statusLabel->setText(status);
    warpFinderSetColorRole(statusLabel, role);

    const QString detail = running && !live.stopping && !live.log.isEmpty() ? live.log.constLast() : QString();
    detailLabel->setText(detail);
    detailLabel->setVisible(!detail.isEmpty() && detail != status);

    progressRow->setVisible(running);
    if (running) {
        if (live.total == 0) {
            progressBar->setRange(0, 0);
        } else {
            const quint64 total = qMin<quint64>(live.total, std::numeric_limits<int>::max());
            const quint64 tested = qMin(live.tested, total);
            progressBar->setRange(0, static_cast<int>(total));
            progressBar->setValue(static_cast<int>(tested));
            progressBar->setFormat(tr("%1 s of %2 s").arg(tested).arg(total));
        }
        foundLabel->setText(tr("Found %n", nullptr, found));
    }

    QStringList notices;
    if (live.warpJunk) {
        notices << tr("Plain WireGuard is blocked on this network, but it gets through with AmneziaWG junk packets. Using an endpoint "
                      "turns them on.");
    }
    if (live.warpHttp2) {
        notices << tr("HTTP/3 is blocked on this network, but HTTP/2 gets through. Using an endpoint turns on the fallback to HTTP/2.");
    }
    if (!overrideOrigin.isEmpty()) {
        notices << tr("This profile dials %1 instead of its own address. Using an endpoint switches it back to its own address.")
                       .arg(overrideOrigin);
    }
    noticeLabel->setText(notices.join(QLatin1Char('\n')));
    noticeLabel->setVisible(!notices.isEmpty());

    placeholderLabel->setVisible(running && found == 0);
    runButton->setText(running ? tr("Stop") : tr("Search again"));
    runButton->setEnabled(!running || !live.stopping);
}

void DialogWarpFinder::showHits(const QList<Configs::IpListEntry> &hits) {
    if (warpFinderSameHits(hits, shownHits)) return;
    QString keep;
    if (const auto selected = resultsView->selectedItems(); selectionByUser && !selected.isEmpty()) {
        keep = selected.first()->data(kWarpFinderEndpointColumn, kWarpFinderKeyRole).toString();
    }

    const QScopedValueRollback guard(populating, true);
    resultsView->clear();
    const QString unknown(QChar(0x2014));
    QTreeWidgetItem *select = nullptr;
    for (const auto &hit : hits) {
        auto *item = new QTreeWidgetItem(resultsView);
        item->setText(kWarpFinderEndpointColumn, hit.port > 0 ? DisplayAddress(hit.cidr, hit.port) : hit.cidr);
        item->setText(kWarpFinderLatencyColumn, hit.latencyMs > 0 ? IpEntriesModel::tr("%1 ms").arg(hit.latencyMs) : unknown);
        item->setText(kWarpFinderLossColumn, hit.lossPct >= 0 ? IpEntriesModel::tr("%1%").arg(hit.lossPct) : unknown);
        for (const int column : {kWarpFinderLatencyColumn, kWarpFinderLossColumn}) {
            item->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter);
        }
        const QString key = warpFinderKey(hit);
        item->setData(kWarpFinderEndpointColumn, kWarpFinderKeyRole, key);
        if (select == nullptr && !keep.isEmpty() && key == keep) select = item;
    }
    shownHits = hits;

    // Until the user picks a row, the selection follows the best endpoint.
    if (select == nullptr) {
        selectionByUser = false;
        select = resultsView->topLevelItem(0);
    }
    if (select != nullptr) resultsView->setCurrentItem(select);
    useButton->setEnabled(!resultsView->selectedItems().isEmpty());
}

void DialogWarpFinder::runClicked() {
    auto *manager = Scanner::ScanManager::instance();
    if (runId < 0 && manager->IsRunning(runId)) {
        manager->Pause(runId);
        refresh();
        return;
    }
    start();
}

void DialogWarpFinder::useSelected() {
    const auto selected = resultsView->selectedItems();
    if (selected.isEmpty()) return;
    const int row = resultsView->indexOfTopLevelItem(selected.first());
    if (row < 0 || row >= shownHits.size()) return;
    const auto entry = shownHits.at(row);
    const auto live = runId < 0 ? Scanner::ScanManager::instance()->LiveState(runId) : Scanner::ScanLiveState{};
    forgetRun();
    emit endpointChosen(entry, live.warpJunk, live.warpHttp2);
    accept();
}
