#include "include/scanner/ScanManager.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <utility>

#include "include/api/RPC.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include "include/scanner/DefaultIpLists.h"
#include "include/scanner/IpListParse.h"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanProfiles.h"
#include "include/scanner/ScanWorker.h"
#include "include/scanner/WarpPresets.h"
#include "include/scanner/WarpWorker.h"

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

        // The carrier's default list, put back if it was deleted; the presets when it cannot be or holds nothing.
        QList<Configs::IpListEntry> scanManagerWarpSeeds(bool masque) {
            const auto &name = masque ? DefaultIpLists::kWarpMasqueName : DefaultIpLists::kWarpWireGuardName;
            const auto lists = Configs::dataManager->ipListsRepo->GetAllIpLists();
            const bool existed = std::any_of(lists.cbegin(), lists.cend(), [&name](const std::shared_ptr<Configs::IpList> &list) {
                return list->role == Configs::IpList::Role::User && list->name == name;
            });
            QList<Configs::IpListEntry> entries;
            if (const int listId = DefaultIpLists::Ensure(name); listId >= 0) {
                if (!existed) IpListUpdater::instance()->NotifyListsChanged();
                entries = Configs::dataManager->ipListsRepo->GetEntries(listId);
            }
            if (entries.isEmpty()) entries = EntriesFromCidrs(masque ? WarpPresets::kMasqueEndpoints : WarpPresets::kWireGuardRanges, 0);
            return entries;
        }

        // A resolver scan is reached only through its DNS tunnel profile, so once that is gone nothing can open it.
        QList<int> scanManagerProfilelessScans() {
            const auto scans = Configs::dataManager->ipScansRepo->GetAllIpScans();
            if (scans.isEmpty()) return {};
            QList<int> profileIds;
            for (const auto &scan : scans) {
                if (scan->config.resolver.profileId >= 0) profileIds << scan->config.resolver.profileId;
            }
            QSet<int> existing;
            for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileIDNameMappedBatch(profileIds))
                existing.insert(profile.first);
            QList<int> ids;
            for (const auto &scan : scans) {
                if (!existing.contains(scan->config.resolver.profileId)) ids << scan->id;
            }
            return ids;
        }
    } // namespace

    ScanManager::ScanManager(QObject *parent) : QObject(parent) {
        scanManagerDeleteDetached(Configs::dataManager->ipListsRepo->OrphanedIpLists(), true);
        for (const int scanId : scanManagerProfilelessScans()) DeleteScan(scanId, false);
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
        if (scan == nullptr || scan->kind != Configs::IpScan::Kind::Resolver) return {tr("The scan no longer exists")};
        const auto &config = scan->config;

        QString error;
        const auto base = ResolveScanBase(*scan, &error);
        if (base == nullptr) return {error.isEmpty() ? tr("The scan's profile cannot be used") : error};

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
        const auto worker = std::make_shared<ScanWorker>(*scan, effective, base, hooksFor(scanId));
        Configs::dataManager->ipScansRepo->SaveStatus(scanId, Configs::IpScan::Status::Running, {});
        workers.insert(scanId, worker);
        emit statusChanged(scanId);
        worker->Start();
        return {};
    }

    ScanRunHooks ScanManager::hooksFor(int scanId) {
        QPointer<ScanManager> self(this);
        ScanRunHooks hooks;
        hooks.progress = [self, scanId] {
            runOnUiThread([self, scanId] {
                if (self != nullptr && self->tracked(scanId)) scanManagerQueueProgress(scanId);
            });
        };
        hooks.results = [self, scanId] {
            runOnUiThread([self, scanId] {
                if (self != nullptr && self->tracked(scanId)) emit self->resultsChanged(scanId);
            });
        };
        hooks.finished = [self, scanId] {
            runOnUiThread([self, scanId] {
                if (self != nullptr) self->onWorkerFinished(scanId);
            });
        };
        return hooks;
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
            if (write && it.key() >= 0 && !worker->IsFinalized())
                Configs::dataManager->ipScansRepo->SaveStatus(it.key(), Configs::IpScan::Status::Paused, {});
        }
    }

    bool ScanManager::IsRunning(int scanId) const {
        return workers.contains(scanId);
    }

    QList<int> ScanManager::RunningScans() const {
        QList<int> ids;
        for (auto it = workers.cbegin(); it != workers.cend(); ++it) {
            if (it.key() >= 0) ids << it.key();
        }
        return ids;
    }

    ScanLiveState ScanManager::LiveState(int scanId) const {
        if (const auto it = workers.constFind(scanId); it != workers.constEnd()) return it.value()->LiveState();
        if (scanId < 0) return finders.value(scanId);
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
        if (scanId < 0) return;
        if (isWatched) watched.insert(scanId);
        else watched.remove(scanId);
    }

    bool ScanManager::IsWatched(int scanId) const {
        return watched.contains(scanId);
    }

    QString ScanManager::DeleteScan(int scanId, bool deleteResultList) {
        if (scanId < 0) return {};
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
        return scanName.trimmed();
    }

    void ScanManager::NotifyScansChanged() {
        emit scansChanged();
    }

    int ScanManager::StartWarpFinder(std::shared_ptr<Configs::Profile> base, QString *error) {
        auto copy = CloneWarpScanBase(base, error);
        if (copy == nullptr) return 0;
        auto seeds = scanManagerWarpSeeds(WarpCarrier(*copy) == QLatin1String("masque"));
        const int id = nextFinderId--;
        const auto worker = std::make_shared<WarpWorker>(id, std::move(copy), std::move(seeds), hooksFor(id));
        workers.insert(id, worker);
        emit statusChanged(id);
        worker->Start();
        return id;
    }

    void ScanManager::ForgetWarpFinder(int id) {
        if (id >= 0) return;
        finders.remove(id);
        scanManagerPendingProgress().remove(id);
        // The run winds down on its own threads, which hold it; nothing it reports reaches views any more.
        if (const auto worker = workers.take(id); worker != nullptr && !worker->IsStopping()) {
            worker->RequestStop();
            scanManagerSendStop(worker->SessionId());
        }
    }

    bool ScanManager::tracked(int scanId) const {
        return scanId >= 0 || workers.contains(scanId) || finders.contains(scanId);
    }

    void ScanManager::onWorkerFinished(int scanId) {
        scanManagerPendingProgress().remove(scanId);
        if (scanId < 0) {
            const auto it = workers.constFind(scanId);
            if (it == workers.constEnd()) return;
            auto state = it.value()->LiveState();
            state.running = false;
            state.stopping = false;
            finders.insert(scanId, state);
        }
        // Still registered, so views read the worker's final log and stage.
        emit progressChanged(scanId);
        workers.remove(scanId);
        emit statusChanged(scanId);
    }
} // namespace Scanner
