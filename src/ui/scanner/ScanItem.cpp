#include "include/ui/scanner/ScanItem.h"

#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "include/database/DatabaseManager.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Utils.hpp"
#include "include/scanner/ScanProfiles.h"
#include "include/ui/mainwindow.h"
#include "include/ui/scanner/dialog_scan_instance.h"
#include "include/ui/setting/ThemeManager.hpp"

namespace {
    constexpr int kScanItemPillPaddingX = 6;
    constexpr int kScanItemPillPaddingY = 1;
    constexpr int kScanItemPillBorder = 1;

    // UI thread only.
    QHash<int, int> scanItemWatchCounts;

    QIcon RecolorScanItemIcon(const QString &path, const QColor &color) {
        QPixmap pixmap(path);
        if (pixmap.isNull()) return QIcon(path);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        return QIcon(pixmap);
    }

    int scanItemPermille(quint64 tested, quint64 total) {
        if (total == 0) return 0;
        return static_cast<int>(qMin<quint64>(1000, tested * 1000 / total));
    }
}

namespace ScannerUi {
    void Watch(int scanId, bool watched) {
        auto *manager = Scanner::ScanManager::instance();
        int &count = scanItemWatchCounts[scanId];
        count = qMax(0, count + (watched ? 1 : -1));
        const bool now = count > 0;
        if (count == 0) scanItemWatchCounts.remove(scanId);
        if (manager->IsWatched(scanId) == now) return;
        manager->SetWatched(scanId, now);
        if (auto *window = GetMainWindow()) window->refreshScannerDataView(true);
    }

    bool IsResumable(const Configs::IpScan &scan) {
        if (scan.mode == Configs::IpScan::Mode::RescanResult && scan.rescan_snapshot_list_id >= 0 &&
            scan.rescan_total > 0 && scan.rescan_cursor < scan.rescan_total)
            return true;
        return scan.CanResumeInitial();
    }

    int ResultCount(const Configs::IpScan &scan) {
        if (scan.result_list_id < 0) return 0;
        return Configs::dataManager->ipListsRepo->EntryCount(scan.result_list_id);
    }

    bool ConfirmWipe(QWidget *parent, const Configs::IpScan &scan) {
        const int results = ResultCount(scan);
        const bool resumable = IsResumable(scan);
        if (results <= 0 && !resumable) return true;
        QString text;
        if (!resumable) {
            text = ScanItem::tr("Starting over clears the %n result(s) this scan has found. Continue?", nullptr, results);
        } else if (results > 0) {
            text = ScanItem::tr("This scan was paused partway. Starting over discards its progress and clears its %n result(s). Continue?",
                                nullptr, results);
        } else {
            text = ScanItem::tr("This scan was paused partway. Starting over discards its progress. Continue?");
        }
        return QMessageBox::question(parent, ScanItem::tr("IP Scanner"), text) == QMessageBox::Yes;
    }

    bool StartScan(QWidget *parent, int scanId, Scanner::StartMode mode) {
        auto *manager = Scanner::ScanManager::instance();
        auto result = manager->Start(scanId, mode);
        if (result.targetsChanged) {
            const auto scan = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
            const int results = scan != nullptr ? ResultCount(*scan) : 0;
            const QString text =
                results > 0 ? ScanItem::tr("The targets changed since this scan was paused, so it cannot resume. Start over from "
                                           "the beginning and clear its %n result(s)?",
                                           nullptr, results)
                            : ScanItem::tr("The targets changed since this scan was paused, so it cannot resume. Start over from "
                                           "the beginning?");
            if (QMessageBox::question(parent, ScanItem::tr("IP Scanner"), text) != QMessageBox::Yes) return false;
            result = manager->Start(scanId, Scanner::StartMode::FromInitial);
        }
        if (!result.ok()) {
            MessageBoxWarning(ScanItem::tr("IP Scanner"),
                              result.error.isEmpty() ? ScanItem::tr("The scan could not be started.") : result.error);
            return false;
        }
        return true;
    }

    Scanner::ScanLiveState RowState(const Configs::IpScan &scan) {
        Scanner::ScanLiveState state;
        state.tested = scan.ActiveCursor();
        state.total = scan.ActiveTotal();
        state.found = scan.found;
        state.removed = scan.removed;
        state.error = scan.last_error;
        return state;
    }

    QString StatusLine(const Configs::IpScan &scan, const Scanner::ScanLiveState &live, bool withStage, bool *isError) {
        using Status = Configs::IpScan::Status;
        QStringList parts;
        bool showCounts = true;
        if (live.running) {
            if (live.stopping) parts << ScanItem::tr("Pausing…");
            else parts << ScanItem::tr("Running");
            if (withStage && !live.stopping && !live.stage.isEmpty()) parts << live.stage;
        } else {
            switch (scan.status) {
                case Status::Idle:
                    parts << ScanItem::tr("Idle");
                    showCounts = false;
                    break;
                case Status::Running:
                case Status::Paused:
                    parts << ScanItem::tr("Paused");
                    break;
                case Status::Completed:
                    parts << ScanItem::tr("Completed");
                    break;
                case Status::Failed:
                    parts << ScanItem::tr("Failed");
                    break;
            }
        }

        if (showCounts) {
            const bool finished = !live.running && scan.status == Status::Completed;
            if (live.total > 0 && !finished) {
                parts << ScanItem::tr("%1 / %2 (%3%)")
                             .arg(live.tested)
                             .arg(live.total)
                             .arg(scanItemPermille(live.tested, live.total) / 10);
            }
            if (live.found > 0 || finished) parts << ScanItem::tr("%n found", nullptr, live.found);
            if (live.removed > 0) parts << ScanItem::tr("%n removed", nullptr, live.removed);
        }

        const bool error = !live.error.isEmpty();
        if (error) parts << live.error;
        if (isError != nullptr) *isError = error;
        return parts.join(QStringLiteral(" · "));
    }

    QString KindText(Configs::IpScan::Kind kind) {
        switch (kind) {
            case Configs::IpScan::Kind::Warp:
                return ScanItem::tr("WARP");
            case Configs::IpScan::Kind::Resolver:
                return ScanItem::tr("DNS resolvers");
            case Configs::IpScan::Kind::Generic:
                break;
        }
        return ScanItem::tr("Generic");
    }

    void FindResolvers(int profileId) {
        const auto profile = profileId >= 0 ? Configs::dataManager->profilesRepo->GetProfile(profileId) : nullptr;
        if (profile == nullptr || profile->outbound == nullptr || !Scanner::IsDnsTunnelBase(*profile)) {
            MessageBoxWarning(ScanItem::tr("IP Scanner"),
                              ScanItem::tr("Only a saved DNSTT, MasterDNS or XDNS profile can be used to find resolvers; save the profile first."));
            return;
        }
        int scanId = -1;
        for (const auto &scan : Configs::dataManager->ipScansRepo->GetAllIpScans()) {
            if (scan->kind == Configs::IpScan::Kind::Resolver && scan->config.resolver.profileId == profileId)
                scanId = qMax(scanId, scan->id);
        }
        if (scanId < 0) {
            auto scan = Configs::IpScansRepo::NewIpScan(Configs::IpScan::Kind::Resolver);
            scan->name = ScanItem::tr("Resolvers for %1").arg(profile->outbound->name);
            scan->config.resolver.profileId = profileId;
            if (!Configs::dataManager->ipScansRepo->AddIpScan(scan)) {
                MessageBoxWarning(ScanItem::tr("IP Scanner"), ScanItem::tr("Failed to store the scan."));
                return;
            }
            scanId = scan->id;
            Scanner::ScanManager::instance()->NotifyScansChanged();
        }
        DialogScanInstance::Open(scanId);
    }
} // namespace ScannerUi

ScanItem::ScanItem(QWidget *parent, std::shared_ptr<Configs::IpScan> scan_, QListWidgetItem *item_)
    : QWidget(parent), scan(std::move(scan_)), scanId(scan->id), item(item_) {
    setLayoutDirection(Qt::LeftToRight);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(6, 4, 6, 4);

    auto *text = new QVBoxLayout();
    text->setSpacing(2);

    nameLabel = new QLabel(this);
    nameLabel->setTextFormat(Qt::PlainText);
    nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    text->addWidget(nameLabel);

    statusLabel = new QLabel(this);
    statusLabel->setTextFormat(Qt::PlainText);
    statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    text->addWidget(statusLabel);
    row->addLayout(text, 1);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 1000);
    progressBar->setTextVisible(false);
    progressBar->setFixedSize(72, 6);
    auto barPolicy = progressBar->sizePolicy();
    barPolicy.setRetainSizeWhenHidden(true);
    progressBar->setSizePolicy(barPolicy);
    row->addWidget(progressBar, 0, Qt::AlignVCenter);

    kindLabel = new QLabel(this);
    kindLabel->setTextFormat(Qt::PlainText);
    kindLabel->setAlignment(Qt::AlignCenter);
    kindLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    row->addWidget(kindLabel, 0, Qt::AlignVCenter);

    actions = new QWidget(this);
    auto *actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(2);
    runButton = new QToolButton(actions);
    openButton = new QToolButton(actions);
    removeButton = new QToolButton(actions);
    openButton->setToolTip(tr("Open"));
    removeButton->setToolTip(tr("Delete"));
    for (auto *button : {runButton, openButton, removeButton}) {
        button->setAutoRaise(true);
        actionsLayout->addWidget(button);
    }
    auto actionsPolicy = actions->sizePolicy();
    actionsPolicy.setRetainSizeWhenHidden(true);
    actions->setSizePolicy(actionsPolicy);
    row->addWidget(actions, 0, Qt::AlignVCenter);

    // Presses must reach the list, which selects, opens and drags from them.
    for (QWidget *passive : {static_cast<QWidget *>(nameLabel), static_cast<QWidget *>(kindLabel),
                             static_cast<QWidget *>(statusLabel), static_cast<QWidget *>(progressBar)})
        passive->setAttribute(Qt::WA_TransparentForMouseEvents);

    connect(runButton, &QToolButton::clicked, this, [this] {
        if (running) emit pauseRequested();
        else emit startRequested();
    });
    connect(openButton, &QToolButton::clicked, this, [this] { emit openRequested(); });
    connect(removeButton, &QToolButton::clicked, this, [this] { emit deleteRequested(); });
    connect(themeManager(), &ThemeManager::themeChanged, this, [this] { applyStyle(); });

    applyIcons();
    SetActionsVisible(false);
    Refresh(false);

    adjustSize();
    if (item != nullptr) item->setSizeHint(sizeHint());
}

void ScanItem::Refresh(bool reload) {
    if (reload) {
        if (auto fresh = Configs::dataManager->ipScansRepo->GetIpScan(scanId)) scan = fresh;
    }

    auto *manager = Scanner::ScanManager::instance();
    running = manager->IsRunning(scanId);
    const auto live = running ? manager->LiveState(scanId) : ScannerUi::RowState(*scan);

    kindLabel->setText(ScannerUi::KindText(scan->kind));
    bool error = false;
    statusText = ScannerUi::StatusLine(*scan, live, false, &error);
    statusError = error;
    statusLabel->setToolTip(statusText);
    nameLabel->setToolTip(scan->name);

    const bool partial = live.total > 0 && live.tested < live.total;
    progressBar->setVisible(running || (partial && scan->status != Configs::IpScan::Status::Completed));
    progressBar->setValue(scanItemPermille(live.tested, live.total));

    runButton->setEnabled(!live.stopping);
    removeButton->setEnabled(!running);
    updateRunButton();
    applyStyle();
    updateElidedLabels();
}

void ScanItem::SetActionsVisible(bool visible) {
    actions->setVisible(visible);
}

void ScanItem::applyStyle() const {
    const auto &tokens = themeManager()->tokens;
    const int pillHeight = kindLabel->fontMetrics().height() + 2 * (kScanItemPillPaddingY + kScanItemPillBorder);
    const QString pill = QStringLiteral("QLabel { color: %1; border: %2px solid %1; border-radius: %3px; "
                                        "padding: %4px %5px; background: transparent; }")
                             .arg(tokens.accent.name())
                             .arg(kScanItemPillBorder)
                             .arg(pillHeight / 2)
                             .arg(kScanItemPillPaddingY)
                             .arg(kScanItemPillPaddingX);
    if (kindLabel->styleSheet() != pill) kindLabel->setStyleSheet(pill);
    kindLabel->setFixedHeight(pillHeight);

    const auto &color = statusError ? tokens.danger : tokens.muted;
    const QString status = QStringLiteral("color: %1;").arg(color.name());
    // setStyleSheet repolishes even when the sheet is unchanged.
    if (statusLabel->styleSheet() != status) statusLabel->setStyleSheet(status);
}

void ScanItem::updateRunButton() const {
    runButton->setIcon(style()->standardIcon(running ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
    runButton->setToolTip(running ? tr("Pause") : (ScannerUi::IsResumable(*scan) ? tr("Resume") : tr("Start")));
}

void ScanItem::applyIcons() const {
    const auto color = palette().color(QPalette::ButtonText);
    updateRunButton();
    openButton->setIcon(RecolorScanItemIcon(QStringLiteral(":/icon/material/pencil-outline.png"), color));
    removeButton->setIcon(RecolorScanItemIcon(QStringLiteral(":/icon/material/delete.png"), color));
}

void ScanItem::updateElidedLabels() const {
    // QLabel clips rather than elides.
    nameLabel->setText(nameLabel->fontMetrics().elidedText(scan->name, Qt::ElideRight, nameLabel->width()));
    statusLabel->setText(statusLabel->fontMetrics().elidedText(statusText, Qt::ElideRight, statusLabel->width()));
}

void ScanItem::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) applyIcons();
    if (event->type() == QEvent::FontChange) applyStyle();
    QWidget::changeEvent(event);
}

void ScanItem::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateElidedLabels();
}
