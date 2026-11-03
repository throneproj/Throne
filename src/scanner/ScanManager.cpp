#include "include/scanner/ScanManager.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTimer>

#include <utility>

#include "include/api/RPC.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/global/Configs.hpp"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanProfiles.h"
#include "include/scanner/ScanWorker.h"

namespace Scanner {
    namespace {
        constexpr int kScanManagerProgressIntervalMs = 250;

        // UI thread only.
        QSet<int> &scanManagerPendingProgress() {
            static QSet<int> pending;
            return pending;
        }

        // Detached lists stay unreachable while the background delete runs.
        void scanManagerDeleteDetached(const QList<int> &ids, bool detachFirst = false) {
            if (ids.isEmpty()) return;
            runOnNewThread([ids, detachFirst] {
                auto &lists = *Configs::dataManager->ipListsRepo;
                for (const int id : ids) {
                    if (detachFirst) lists.DetachIpList(id);
                    lists.DeleteIpList(id, true);
                }
                IpListUpdater::instance()->NotifyListsChanged();
            });
        }

        QPointer<QTimer> &scanManagerProgressTimer() {
            static QPointer<QTimer> timer;
            return timer;
        }

        void scanManagerQueueProgress(int scanId) {
            scanManagerPendingProgress().insert(scanId);
            if (auto *timer = scanManagerProgressTimer().data(); timer != nullptr && !timer->isActive()) timer->start();
        }

        void scanManagerSendStop(const QString &sessionId) {
            runOnNewThread([sessionId] {
                bool ok = false;
                if (API::defaultClient != nullptr) API::defaultClient->StopScan(&ok, sessionId);
            });
        }

        bool scanManagerRescanResumable(const Configs::IpScan &scan) {
            return scan.mode == Configs::IpScan::Mode::RescanResult && scan.rescan_snapshot_list_id >= 0 &&
                   scan.rescan_cursor < scan.rescan_total;
        }

        std::shared_ptr<Configs::IpList> scanManagerListHeader(int listId) {
            if (listId < 0) return nullptr;
            for (const auto &header : Configs::dataManager->ipListsRepo->GetAllIpLists(true)) {
                if (header->id == listId) return header;
            }
            return nullptr;
        }
    } // namespace

    ScanManager::ScanManager(QObject *parent) : QObject(parent) {
        scanManagerDeleteDetached(Configs::dataManager->ipListsRepo->OrphanedIpLists(), true);
        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->setInterval(kScanManagerProgressIntervalMs);
        connect(timer, &QTimer::timeout, this, [this] {
            const auto pending = std::exchange(scanManagerPendingProgress(), {});
            for (const int scanId : pending) emit progressChanged(scanId);
        });
        scanManagerProgressTimer() = timer;
    }

    ScanManager *ScanManager::instance() {
        static QPointer<ScanManager> self;
        if (!self) self = new ScanManager(QCoreApplication::instance());
        return self;
    }

    StartResult ScanManager::Start(int scanId, StartMode mode) {
        if (workers.contains(scanId)) return {tr("The scan is already running")};
        const auto scan = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
        if (scan == nullptr) return {tr("The scan no longer exists")};
        const auto &config = scan->config;
        const bool warp = scan->kind == Configs::IpScan::Kind::Warp;
        const bool resolver = scan->kind == Configs::IpScan::Kind::Resolver;

        if (!warp && !resolver && !config.icmp.enabled && !config.tcp.enabled && !config.http.enabled && !config.config.enabled)
            return {tr("Turn on at least one test phase")};
        if (!warp && !resolver && config.http.enabled && !config.http.tls) {
            if (config.http.method.compare(QStringLiteral("NONE"), Qt::CaseInsensitive) == 0)
                return {tr("A TLS handshake check needs TLS turned on")};
            if (config.http.httpVersion == QLatin1String("3")) return {tr("HTTP/3 needs TLS turned on")};
        }

        std::shared_ptr<Configs::Profile> base;
        if (warp || resolver || config.config.enabled) {
            QString error;
            base = ResolveScanBase(*scan, &error);
            if (base == nullptr) return {error.isEmpty() ? tr("The profile for the config test cannot be used") : error};
        }

        StartMode effective = mode;
        if (mode == StartMode::Resume && !scanManagerRescanResumable(*scan)) {
            if (!scan->CanResumeInitial()) {
                effective = StartMode::FromInitial;
            } else if (scan->spec_hash != config.TargetSpecHash(scan->base_list_id)) {
                StartResult result;
                result.targetsChanged = true;
                return result;
            }
        }
        if (effective == StartMode::FromInitial) {
            const auto baseList = scanManagerListHeader(scan->base_list_id);
            if (baseList == nullptr) return {tr("Choose the IP list to scan")};
            if (baseList->entryCount <= 0) return {tr("The IP list to scan is empty")};
        } else if (effective == StartMode::RescanResults) {
            const auto resultList = scanManagerListHeader(scan->result_list_id);
            if (resultList == nullptr || resultList->related_test_id != scanId || resultList->entryCount <= 0)
                return {tr("The result list is empty")};
        }

        persistDisabled = false;
        QPointer<ScanManager> self(this);
        ScanWorker::Hooks hooks;
        hooks.progress = [self, scanId] {
            runOnUiThread([self, scanId] {
                if (self != nullptr) scanManagerQueueProgress(scanId);
            });
        };
        hooks.results = [self, scanId] {
            runOnUiThread([self, scanId] {
                if (self != nullptr) emit self->resultsChanged(scanId);
            });
        };
        hooks.finished = [self, scanId] {
            runOnUiThread([self, scanId] {
                if (self != nullptr) self->onWorkerFinished(scanId);
            });
        };

        auto worker = std::make_shared<ScanWorker>(*scan, effective, base, std::move(hooks));
        Configs::dataManager->ipScansRepo->SaveStatus(scanId, Configs::IpScan::Status::Running, {});
        workers.insert(scanId, worker);
        emit statusChanged(scanId);
        worker->Start();
        return {};
    }

    void ScanManager::Pause(int scanId) {
        const auto it = workers.constFind(scanId);
        if (it == workers.constEnd() || it.value()->IsStopping()) return;
        it.value()->RequestStop();
        scanManagerSendStop(it.value()->SessionId());
        emit statusChanged(scanId);
    }

    void ScanManager::StopAll(bool persist) {
        if (!persist) persistDisabled = true;
        // After a restore, a stopped run still in the map must not touch the restored rows.
        const bool write = persist && !persistDisabled;
        for (auto it = workers.cbegin(); it != workers.cend(); ++it) {
            const auto &worker = it.value();
            if (!persist) worker->DisablePersist();
            worker->RequestStop();
            scanManagerSendStop(worker->SessionId());
            if (write && !worker->IsFinalized())
                Configs::dataManager->ipScansRepo->SaveStatus(it.key(), Configs::IpScan::Status::Paused, {});
        }
    }

    bool ScanManager::IsRunning(int scanId) const {
        return workers.contains(scanId);
    }

    QList<int> ScanManager::RunningScans() const {
        return workers.keys();
    }

    ScanLiveState ScanManager::LiveState(int scanId) const {
        if (const auto it = workers.constFind(scanId); it != workers.constEnd()) return it.value()->LiveState();
        ScanLiveState state;
        const auto scan = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
        if (scan == nullptr) return state;
        state.tested = scan->ActiveCursor();
        state.total = scan->ActiveTotal();
        state.found = scan->found;
        state.removed = scan->removed;
        state.error = scan->last_error;
        return state;
    }

    void ScanManager::SetWatched(int scanId, bool isWatched) {
        if (isWatched) watched.insert(scanId);
        else watched.remove(scanId);
    }

    bool ScanManager::IsWatched(int scanId) const {
        return watched.contains(scanId);
    }

    QString ScanManager::DeleteScan(int scanId, bool deleteResultList) {
        if (workers.contains(scanId)) return tr("Pause the scan first");
        auto &lists = *Configs::dataManager->ipListsRepo;
        const auto scan = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
        const int snapshotId = scan != nullptr ? scan->snapshot_list_id : -1;
        const int rescanSnapshotId = scan != nullptr ? scan->rescan_snapshot_list_id : -1;
        QList<int> doomed;
        for (const auto &header : lists.GetAllIpLists(true)) {
            if (header->role == Configs::IpList::Role::ScanSnapshot &&
                (header->related_test_id == scanId || header->id == snapshotId || header->id == rescanSnapshotId)) {
                doomed.append(header->id);
            } else if (header->role == Configs::IpList::Role::ScanResult && header->related_test_id == scanId) {
                if (deleteResultList) {
                    doomed.append(header->id);
                } else {
                    header->role = Configs::IpList::Role::User;
                    header->related_test_id = -1;
                    lists.SaveHeader(header);
                }
            }
        }
        for (const int id : doomed) lists.DetachIpList(id);
        Configs::dataManager->ipScansRepo->DeleteIpScan(scanId);
        watched.remove(scanId);
        emit scansChanged();
        scanManagerDeleteDetached(doomed);
        IpListUpdater::instance()->NotifyListsChanged();
        return {};
    }

    void ScanManager::RenameScan(int scanId, const QString &name) {
        const auto trimmed = name.trimmed();
        if (trimmed.isEmpty()) return;
        auto &scans = *Configs::dataManager->ipScansRepo;
        const auto scan = scans.GetIpScan(scanId);
        if (scan == nullptr) return;
        if (scan->name != trimmed) {
            scan->name = trimmed;
            scans.Save(scan);
        }
        if (const auto result = scanManagerListHeader(scan->result_list_id);
            result != nullptr && result->role == Configs::IpList::Role::ScanResult && result->related_test_id == scanId) {
            const auto resultName = ResultListName(trimmed);
            if (result->name != resultName) {
                result->name = resultName;
                Configs::dataManager->ipListsRepo->SaveHeader(result);
            }
        }
        emit scansChanged();
        IpListUpdater::instance()->NotifyListsChanged();
    }

    int ScanManager::EnsureResultList(int scanId) {
        const auto scan = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
        if (scan == nullptr) return -1;
        // A running scan's worker owns its row and creates the list itself; a full Save here would race it.
        if (workers.contains(scanId)) return scan->result_list_id;
        const int previous = scan->result_list_id;
        const int listId = EnsureScanResultList(*scan);
        if (listId >= 0 && listId != previous) {
            Configs::dataManager->ipScansRepo->Save(scan);
            IpListUpdater::instance()->NotifyListsChanged();
        }
        return listId;
    }

    QString ScanManager::ResultListName(const QString &scanName) {
        return tr("Scan %1 Result").arg(scanName);
    }

    void ScanManager::NotifyScansChanged() {
        emit scansChanged();
    }

    void ScanManager::onWorkerFinished(int scanId) {
        scanManagerPendingProgress().remove(scanId);
        // Still registered, so views read the worker's final log and stage.
        emit progressChanged(scanId);
        workers.remove(scanId);
        emit statusChanged(scanId);
    }
} // namespace Scanner
