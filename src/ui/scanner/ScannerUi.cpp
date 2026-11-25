#include "include/ui/scanner/ScannerUi.h"

#include <QCoreApplication>
#include <QHash>
#include <QMessageBox>
#include <QStringList>

#include "include/database/DatabaseManager.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Utils.hpp"
#include "include/scanner/ScanProfiles.h"
#include "include/ui/mainwindow.h"
#include "include/ui/scanner/dialog_scan_instance.h"

namespace {
    struct ScannerUiTr {
        Q_DECLARE_TR_FUNCTIONS(ScannerUi)
    };

    // UI thread only.
    QHash<int, int> scannerUiWatchCounts;

    int scannerUiPermille(quint64 tested, quint64 total) {
        if (total == 0) return 0;
        return static_cast<int>(qMin<quint64>(1000, tested * 1000 / total));
    }
}

namespace ScannerUi {
    void Watch(int scanId, bool watched) {
        auto *manager = Scanner::ScanManager::instance();
        int &count = scannerUiWatchCounts[scanId];
        count = qMax(0, count + (watched ? 1 : -1));
        const bool now = count > 0;
        if (count == 0) scannerUiWatchCounts.remove(scanId);
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

    bool ConfirmStartOver(QWidget *parent, const Configs::IpScan &scan) {
        const int results = ResultCount(scan);
        QString text;
        if (IsResumable(scan)) {
            text = ScannerUiTr::tr("This scan stopped partway. Starting over discards its progress. Continue?");
        } else if (results > 0) {
            text = ScannerUiTr::tr("Starting over keeps the %n current result(s) until the first new one replaces them. Continue?",
                                   nullptr, results);
        } else {
            return true;
        }
        return QMessageBox::question(parent, ScannerUiTr::tr("Find resolvers"), text) == QMessageBox::Yes;
    }

    bool StartScan(QWidget *parent, int scanId, Scanner::StartMode mode) {
        auto *manager = Scanner::ScanManager::instance();
        auto result = manager->Start(scanId, mode);
        if (result.targetsChanged) {
            const auto text = ScannerUiTr::tr("The targets changed since this scan was paused, so it cannot resume. Start over from "
                                              "the beginning?");
            if (QMessageBox::question(parent, ScannerUiTr::tr("Find resolvers"), text) != QMessageBox::Yes) return false;
            result = manager->Start(scanId, Scanner::StartMode::FromInitial);
        }
        if (!result.ok()) {
            MessageBoxWarning(ScannerUiTr::tr("Find resolvers"),
                              result.error.isEmpty() ? ScannerUiTr::tr("The scan could not be started.") : result.error);
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
            if (live.stopping) parts << ScannerUiTr::tr("Pausing…");
            else parts << ScannerUiTr::tr("Running");
            if (withStage && !live.stopping && !live.stage.isEmpty()) parts << live.stage;
        } else {
            switch (scan.status) {
                case Status::Idle:
                    parts << ScannerUiTr::tr("Idle");
                    showCounts = false;
                    break;
                case Status::Running:
                case Status::Paused:
                    parts << ScannerUiTr::tr("Paused");
                    break;
                case Status::Completed:
                    parts << ScannerUiTr::tr("Completed");
                    break;
                case Status::Failed:
                    parts << ScannerUiTr::tr("Failed");
                    break;
            }
        }

        if (showCounts) {
            const bool finished = !live.running && scan.status == Status::Completed;
            if (live.total > 0 && !finished) {
                parts << ScannerUiTr::tr("%1 / %2 (%3%)")
                             .arg(live.tested)
                             .arg(live.total)
                             .arg(scannerUiPermille(live.tested, live.total) / 10);
            }
            if (live.found > 0 || finished) parts << ScannerUiTr::tr("%n found", nullptr, live.found);
            if (live.removed > 0) parts << ScannerUiTr::tr("%n removed", nullptr, live.removed);
        }

        const bool error = !live.error.isEmpty();
        if (error) parts << live.error;
        if (isError != nullptr) *isError = error;
        return parts.join(QStringLiteral(" · "));
    }

    void FindResolvers(int profileId) {
        const auto profile = profileId >= 0 ? Configs::dataManager->profilesRepo->GetProfile(profileId) : nullptr;
        if (profile == nullptr || profile->outbound == nullptr || !Scanner::IsDnsTunnelBase(*profile)) {
            MessageBoxWarning(ScannerUiTr::tr("Find resolvers"),
                              ScannerUiTr::tr("Only a saved DNSTT, MasterDNS or XDNS profile can be used to find resolvers; save the profile first."));
            return;
        }
        auto *manager = Scanner::ScanManager::instance();
        const QString name = ScannerUiTr::tr("Resolvers for %1").arg(profile->outbound->name).trimmed();
        std::shared_ptr<Configs::IpScan> newest;
        for (const auto &scan : Configs::dataManager->ipScansRepo->GetAllIpScans()) {
            if (scan->kind == Configs::IpScan::Kind::Resolver && scan->config.resolver.profileId == profileId &&
                (newest == nullptr || scan->id > newest->id))
                newest = scan;
        }
        int scanId = -1;
        if (newest == nullptr) {
            auto scan = Configs::IpScansRepo::NewIpScan(Configs::IpScan::Kind::Resolver);
            scan->name = name;
            scan->config.resolver.profileId = profileId;
            if (!Configs::dataManager->ipScansRepo->AddIpScan(scan)) {
                MessageBoxWarning(ScannerUiTr::tr("Find resolvers"), ScannerUiTr::tr("Failed to store the scan."));
                return;
            }
            scanId = scan->id;
            manager->NotifyScansChanged();
        } else {
            scanId = newest->id;
            const auto result = newest->result_list_id >= 0 ? Configs::dataManager->ipListsRepo->GetIpListHeader(newest->result_list_id) : nullptr;
            const bool stale = newest->name != name || (result != nullptr && result->name != Scanner::ScanManager::ResultListName(name));
            // Renaming saves the whole row, which would race a running scan's progress writes; the next open catches up.
            if (stale && !manager->IsRunning(scanId)) manager->RenameScan(scanId, name);
        }
        DialogScanInstance::Open(scanId);
    }
} // namespace ScannerUi
