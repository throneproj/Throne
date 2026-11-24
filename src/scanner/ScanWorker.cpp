#include "include/scanner/ScanWorker.h"

#include <QDateTime>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "include/api/RPC.h"
#include "include/configs/common/DnsTunnel.h"
#include "include/configs/common/ResolverSource.h"
#include "include/configs/common/xrayStreamSetting.h"
#include "include/configs/generate.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/global/Configs.hpp"
#include "include/scanner/IpListParse.h"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanProfiles.h"

namespace Scanner {
    namespace {
        constexpr int kScanWorkerPollIntervalMs = 250;
        constexpr int kScanWorkerCountTimeoutMs = 60000;
        constexpr int kScanWorkerNetworkCheckTimeoutMs = 3000;
        // The core bounds each resolver target at 10 s.
        constexpr qint64 kScanWorkerTargetBoundMs = 10000;
        // Networks that reset a share of exchanges at random need several tries before a live entry is ruled dead.
        constexpr int kScanWorkerRescanRetries = 5;
        constexpr int kScanWorkerRetryDelayMs = 1000;
        constexpr qint64 kScanWorkerRpcSlackMs = 30000;
        // System resolvers plus the profile's own, probed before the list.
        constexpr int kScanWorkerPreflightCap = 20;
        constexpr int kScanWorkerHeavyLossPct = 50;
        // TEST-NET-1 runs no resolver: an answer from it means the network answers DNS for any address.
        constexpr auto kScanWorkerInterceptionProbe = "192.0.2.1";

        std::atomic<quint32> scanWorkerSessionCounter{0};

        QString scanWorkerText(const std::string &text) {
            return QString::fromStdString(text);
        }

        int scanWorkerClampTimeout(qint64 ms) {
            return static_cast<int>(std::min<qint64>(ms, std::numeric_limits<int>::max()));
        }

        bool scanWorkerIsStopped(const QString &error) {
            return error.contains(QStringLiteral("scan stopped"));
        }

        QString scanWorkerPhaseName(const QString &phase) {
            if (phase == QLatin1String("dns")) return QStringLiteral("DNS");
            if (phase == QLatin1String("warp")) return QStringLiteral("WARP");
            if (phase == QLatin1String("url")) return ScanManager::tr("data check");
            return phase;
        }

        QString scanWorkerFormatEvent(const libcore::ScanEvent &event) {
            const auto kind = scanWorkerText(event.kind.value());
            const auto phase = scanWorkerText(event.phase.value());
            const auto target = scanWorkerText(event.target.value());
            const int ms = event.latency_ms.value();
            if (kind == QLatin1String("start")) return ScanManager::tr("Testing %1 (%2)").arg(target, scanWorkerPhaseName(phase));
            if (kind == QLatin1String("fail"))
                return ScanManager::tr("%1 failed %2: %3").arg(target, scanWorkerPhaseName(phase), scanWorkerText(event.error.value()));
            if (kind != QLatin1String("ok")) return {};
            if (phase == QLatin1String("dns")) return ScanManager::tr("%1 relays the DNS tunnel (%2 ms)").arg(target).arg(ms);
            return {};
        }

        libcore::ScanTargetSpec scanWorkerBuildSpec(const QList<Configs::IpListEntry> &entries, const QList<int> &ports,
                                                    const QString &portMode, bool shuffle, quint64 seed) {
            libcore::ScanTargetSpec spec;
            spec.entries.reserve(static_cast<size_t>(entries.size()));
            for (const auto &entry : entries) {
                libcore::ScanEntry item;
                item.cidr = entry.cidr.toStdString();
                item.port = entry.port;
                spec.entries.push_back(std::move(item));
            }
            for (const int port : ports) spec.default_ports.push_back(port);
            spec.port_mode = portMode.toStdString();
            spec.shuffle = shuffle;
            spec.seed = seed;
            return spec;
        }

        Configs::IpListEntry scanWorkerEntry(const QString &address, int port, int latencyMs) {
            const auto canonical = NormalizeCidr(address);
            return {canonical.isEmpty() ? address : canonical, port, latencyMs};
        }

        QString scanWorkerKey(const Configs::IpListEntry &entry) {
            return entry.cidr + QLatin1Char(' ') + QString::number(entry.port);
        }

        bool scanWorkerIsV6(const Configs::IpListEntry &entry) {
            return entry.cidr.contains(QLatin1Char(':'));
        }

        bool scanWorkerAnyOf(const QList<Configs::IpListEntry> &entries, bool v6) {
            return std::any_of(entries.cbegin(), entries.cend(), [v6](const Configs::IpListEntry &entry) { return scanWorkerIsV6(entry) == v6; });
        }

        int scanWorkerLossPct(int sent, int answered) {
            if (sent <= 0) return -1;
            return static_cast<int>(std::lround(100.0 * std::max(0, sent - answered) / sent));
        }

        // Single addresses only, in order; ranges and anything unparsable are dropped.
        QList<Configs::IpListEntry> scanWorkerHostEntries(const QStringList &lines) {
            QList<Configs::IpListEntry> out;
            if (lines.isEmpty()) return out;
            for (const auto &entry : ParseIpListText(lines.join(QLatin1Char('\n')).toUtf8()).entries) {
                if (IsHostCidr(entry.cidr)) out << entry;
            }
            return out;
        }

        // Port 0 is probed on 53, so it dedupes against an explicit 53.
        QList<Configs::IpListEntry> scanWorkerUniqueTargets(const QList<Configs::IpListEntry> &entries, int cap) {
            QList<Configs::IpListEntry> out;
            QSet<QString> seen;
            for (const auto &entry : entries) {
                if (out.size() >= cap) break;
                const auto key = entry.cidr + QLatin1Char(' ') + QString::number(entry.port > 0 ? entry.port : 53);
                if (seen.contains(key)) continue;
                seen.insert(key);
                out << entry;
            }
            return out;
        }

        // The plain UDP resolvers the profile would use now; DoT/DoH/TCP lines and "system" are left out.
        QList<Configs::IpListEntry> scanWorkerProfileResolvers(const Configs::ResolverSource &source, const QString &protocol,
                                                               const QString &owner) {
            const auto flavor = protocol == QLatin1String("dnstt")       ? Configs::ResolverFlavor::Dnstt
                                : protocol == QLatin1String("masterdns") ? Configs::ResolverFlavor::MasterDns
                                                                         : Configs::ResolverFlavor::Xdns;
            const auto built = Configs::BuildResolvers(source, flavor, owner);
            QStringList lines;
            for (const auto &value : built.resolvers) {
                if (value.isObject()) {
                    const auto object = value.toObject();
                    if (object.value(QLatin1String("type")).toString() == QLatin1String("udp"))
                        lines << FormatTarget(object.value(QLatin1String("server")).toString(),
                                              object.value(QLatin1String("server_port")).toInt());
                    continue;
                }
                auto text = value.toString().trimmed();
                if (text.startsWith(QLatin1String("udp://"), Qt::CaseInsensitive)) text = text.mid(6);
                else if (text.contains(QLatin1String("://"))) continue;
                lines << text;
            }
            return scanWorkerHostEntries(lines);
        }

        // Exact keys suffice only on a finalmask UpgradeXrayFinalmask has canonicalized.
        QJsonObject scanWorkerXdnsSettings(const QJsonObject &finalmask) {
            for (const auto &mask : finalmask.value(QLatin1String("udp")).toArray()) {
                const auto object = mask.toObject();
                if (object.value(QLatin1String("type")).toString() == QLatin1String("xdns"))
                    return object.value(QLatin1String("settings")).toObject();
            }
            return {};
        }
    } // namespace

    int EnsureScanResultList(Configs::IpScan &scan) {
        auto &lists = *Configs::dataManager->ipListsRepo;
        if (scan.result_list_id >= 0) {
            for (const auto &header : lists.GetAllIpLists(true)) {
                if (header->id != scan.result_list_id) continue;
                if (header->role == Configs::IpList::Role::ScanResult && header->related_test_id == scan.id) return header->id;
                break;
            }
        }
        auto list = Configs::IpListsRepo::NewIpList();
        list->name = ScanManager::ResultListName(scan.name);
        list->role = Configs::IpList::Role::ScanResult;
        list->related_test_id = scan.id;
        if (!lists.AddIpList(list)) return -1;
        scan.result_list_id = list->id;
        return list->id;
    }

    bool RescanRemovals::WouldRemove(const QList<Configs::IpListEntry> &hits, const QList<Configs::IpListEntry> &failures,
                                     bool v6) const {
        const auto &family = v6 ? v6_ : v4_;
        const bool passes = family.passed || scanWorkerAnyOf(hits, v6);
        return passes && (!family.pending.isEmpty() || scanWorkerAnyOf(failures, v6));
    }

    void RescanRemovals::Add(const QList<Configs::IpListEntry> &hits, const QList<Configs::IpListEntry> &failures,
                             quint64 chunkStart) {
        for (const auto &hit : hits) (scanWorkerIsV6(hit) ? v6_ : v4_).passed = true;
        for (const auto &failure : failures) {
            auto &family = scanWorkerIsV6(failure) ? v6_ : v4_;
            family.pending << failure;
            if (!family.passed) family.unresolvedFrom = std::min(family.unresolvedFrom, chunkStart);
        }
    }

    QList<Configs::IpListEntry> RescanRemovals::TakeRemovable() {
        QList<Configs::IpListEntry> removable;
        for (auto *family : {&v4_, &v6_}) {
            if (!family->passed) continue;
            removable << std::exchange(family->pending, {});
            family->unresolvedFrom = std::numeric_limits<quint64>::max();
        }
        return removable;
    }

    quint64 RescanRemovals::UnresolvedFrom() const {
        return std::min(v4_.unresolvedFrom, v6_.unresolvedFrom);
    }

    QStringList RescanRemovals::KeptLines() const {
        QStringList lines;
        if (!AnyPassed()) return lines;
        for (const auto *family : {&v4_, &v6_}) {
            if (family->passed || family->pending.isEmpty()) continue;
            const auto name = family == &v6_ ? QStringLiteral("IPv6") : QStringLiteral("IPv4");
            lines << ScanManager::tr("No %1 result answered, so %n failed %1 result(s) were kept", nullptr,
                                     static_cast<int>(family->pending.size()))
                         .arg(name);
        }
        return lines;
    }

    // A snapshot's last_update has no other use: set while the old results wait for the pass's first hit, then 0.
    bool ReplacePending(const Configs::IpScan &scan) {
        if (scan.snapshot_list_id < 0) return false;
        const auto header = Configs::dataManager->ipListsRepo->GetIpListHeader(scan.snapshot_list_id);
        return header != nullptr && header->last_update > 0;
    }

    void ClearReplacePending(const Configs::IpScan &scan) {
        if (scan.snapshot_list_id < 0) return;
        auto &lists = *Configs::dataManager->ipListsRepo;
        const auto header = lists.GetIpListHeader(scan.snapshot_list_id);
        if (header == nullptr || header->last_update == 0) return;
        header->last_update = 0;
        lists.SaveHeader(header);
    }

    ScanWorker::ScanWorker(const Configs::IpScan &scan, StartMode mode, std::shared_ptr<Configs::Profile> base, Hooks hooks)
        : scanId_(scan.id),
          sessionId_(QStringLiteral("scan-%1-%2-%3")
                         .arg(scan.id)
                         .arg(QDateTime::currentMSecsSinceEpoch())
                         .arg(scanWorkerSessionCounter.fetch_add(1))),
          startMode_(mode),
          base_(std::move(base)),
          hooks_(std::move(hooks)),
          initialStatus_(scan.status),
          scan_(scan) {
        scan_.status = Configs::IpScan::Status::Running;
        scan_.last_error.clear();
        live_.running = true;
        live_.stage = ScanManager::tr("Preparing");
        live_.found = scan.found;
        if (mode == StartMode::Resume) {
            live_.tested = scan.ActiveCursor();
            live_.total = scan.ActiveTotal();
            live_.removed = scan.removed;
        }
    }

    void ScanWorker::Start() {
        const auto self = shared_from_this();
        runOnNewThread([self] { self->run(); });
        runOnNewThread([self] { self->pollLoop(); });
    }

    void ScanWorker::RequestStop() {
        stop_.store(true);
        QMutexLocker locker(&mutex_);
        live_.stopping = true;
    }

    void ScanWorker::DisablePersist() {
        persist_.store(false);
    }

    ScanLiveState ScanWorker::LiveState() const {
        QMutexLocker locker(&mutex_);
        auto state = live_;
        state.stopping = stop_.load();
        return state;
    }

    void ScanWorker::run() {
        QString error;
        bool completed = false;
        try {
            error = execute(completed);
        } catch (const std::exception &ex) {
            error = QString::fromUtf8(ex.what());
        } catch (...) {
            error = ScanManager::tr("The scan stopped on an unexpected error");
        }
        // A pause cuts RPCs short, so their errors are artifacts.
        if (stop_.load() && !completed) error.clear();
        try {
            finishPass(completed, error);
        } catch (...) {
            MW_show_log(ScanManager::tr("Scan %1: saving its final state failed").arg(scanId_));
        }
        pollStop_.store(true);
        {
            QMutexLocker locker(&mutex_);
            chunkActive_ = false;
        }
        if (hooks_.finished) hooks_.finished();
        bool ok = false;
        API::defaultClient->StopScan(&ok, sessionId_);
    }

    QString ScanWorker::execute(bool &completed) {
        setStage(Stage::Preparing);
        pushLog(ScanManager::tr("Preparing targets…"));
        if (auto error = prepareResolverProbe(); !error.isEmpty()) return error;
        if (stop_) return {};
        // Before the pass changes anything, so a dead network leaves the scan as it was.
        QString detail;
        QStringList system;
        if (!networkUp(&detail, &system)) {
            if (stop_) return {};
            if (!detail.isEmpty()) MW_show_log(ScanManager::tr("Scan %1: network check failed: %2").arg(scan_.name, detail));
            return ScanManager::tr("Network appears to be down");
        }
        systemResolvers_ = scanWorkerHostEntries(system);
        if (stop_) return {};
        if (auto error = preparePass(); !error.isEmpty()) return error;
        if (std::exchange(listsTouched_, false)) IpListUpdater::instance()->NotifyListsChanged();
        if (stop_) return {};
        if (auto error = loadSpec(); !error.isEmpty()) return error;
        if (!rescanPass()) {
            if (auto error = preflight(completed); !error.isEmpty() || completed || stop_) return error;
        }
        return runChunks(completed);
    }

    QString ScanWorker::preparePass() {
        switch (startMode_) {
            case StartMode::FromInitial:
                return prepareInitial();
            case StartMode::RescanResults:
                return prepareRescan();
            case StartMode::Resume:
                break;
        }
        const bool rescanResumable =
            rescanPass() && scan_.rescan_snapshot_list_id >= 0 && scan_.rescan_cursor < scan_.rescan_total;
        if (!rescanResumable && !scan_.CanResumeInitial()) return prepareInitial();
        if (!rescanResumable) {
            scan_.mode = Configs::IpScan::Mode::Initial;
            replacePending_ = ReplacePending(scan_);
        }
        if (!persist_ || stop_) return {};
        const int previous = scan_.result_list_id;
        if (EnsureScanResultList(scan_) < 0) return ScanManager::tr("The result list could not be created");
        if (scan_.result_list_id != previous) {
            listsTouched_ = true;
            persistPassState();
        }
        return {};
    }

    QString ScanWorker::prepareInitial() {
        auto &lists = *Configs::dataManager->ipListsRepo;
        const auto base = lists.GetIpList(scan_.base_list_id);
        if (base == nullptr) return ScanManager::tr("The base IP list no longer exists");
        if (base->entries.isEmpty()) return ScanManager::tr("The base IP list is empty");
        if (!persist_ || stop_) return {};

        const int previous = scan_.result_list_id;
        const int resultId = EnsureScanResultList(scan_);
        if (resultId < 0) return ScanManager::tr("The result list could not be created");
        if (resultId != previous) listsTouched_ = true;
        // The old results stay until the pass's first hit replaces them.
        replacePending_ = true;
        deleteList(scan_.snapshot_list_id);
        deleteList(scan_.rescan_snapshot_list_id);
        scan_.snapshot_list_id = -1;
        scan_.rescan_snapshot_list_id = -1;
        scan_.rescan_cursor = 0;
        scan_.rescan_total = 0;
        scan_.mode = Configs::IpScan::Mode::Initial;
        scan_.cursor = 0;
        scan_.total = 0;
        scan_.found = lists.EntryCount(resultId);
        scan_.removed = 0;
        scan_.started_at = QDateTime::currentSecsSinceEpoch();
        scan_.finished_at = 0;
        scan_.last_error.clear();
        {
            QMutexLocker locker(&mutex_);
            live_.found = scan_.found;
            live_.removed = 0;
        }

        auto snapshot = Configs::IpListsRepo::NewIpList();
        snapshot->name = ScanManager::tr("%1 snapshot").arg(scan_.name);
        snapshot->role = Configs::IpList::Role::ScanSnapshot;
        snapshot->related_test_id = scan_.id;
        snapshot->last_update = QDateTime::currentSecsSinceEpoch();
        snapshot->entries = base->entries;
        snapshot->entryCount = static_cast<int>(base->entries.size());
        snapshot->entriesLoaded = true;
        if (!persist_) return {};
        if (!lists.AddIpList(snapshot)) return ScanManager::tr("The base IP list could not be copied");
        listsTouched_ = true;
        scan_.snapshot_list_id = snapshot->id;
        scan_.seed = QRandomGenerator::system()->generate64();
        scan_.spec_hash = scan_.config.TargetSpecHash(scan_.base_list_id);
        spec_ = scanWorkerBuildSpec(base->entries, scan_.config.ports, scan_.config.PortModeKey(), scan_.config.shuffle, scan_.seed);

        QString error;
        if (!countTargets(spec_, &scan_.total, &error)) return error;
        persistPassState();
        if (scan_.total == 0) return ScanManager::tr("The base IP list holds no valid target");
        setTested(0, scan_.total);
        return {};
    }

    QString ScanWorker::prepareRescan() {
        auto &lists = *Configs::dataManager->ipListsRepo;
        if (!persist_ || stop_) return {};
        deleteList(scan_.rescan_snapshot_list_id);
        scan_.rescan_snapshot_list_id = -1;
        scan_.rescan_cursor = 0;
        scan_.rescan_total = 0;

        const int previousResultId = scan_.result_list_id;
        const int resultId = EnsureScanResultList(scan_);
        if (resultId < 0) return ScanManager::tr("The result list could not be created");
        if (resultId != previousResultId) listsTouched_ = true;
        const auto entries = lists.GetEntries(resultId);
        if (entries.isEmpty()) {
            persistPassState();
            return ScanManager::tr("The result list is empty");
        }
        auto snapshot = Configs::IpListsRepo::NewIpList();
        snapshot->name = ScanManager::tr("%1 rescan snapshot").arg(scan_.name);
        snapshot->role = Configs::IpList::Role::ScanSnapshot;
        snapshot->related_test_id = scan_.id;
        snapshot->entries = entries;
        snapshot->entryCount = static_cast<int>(entries.size());
        snapshot->entriesLoaded = true;
        if (!persist_) return {};
        if (!lists.AddIpList(snapshot)) return ScanManager::tr("The result list could not be copied");
        listsTouched_ = true;
        scan_.rescan_snapshot_list_id = snapshot->id;
        scan_.mode = Configs::IpScan::Mode::RescanResult;
        scan_.removed = 0;
        scan_.last_error.clear();
        {
            QMutexLocker locker(&mutex_);
            live_.removed = 0;
        }
        spec_ = scanWorkerBuildSpec(entries, {}, QStringLiteral("list"), false, 0);

        QString error;
        if (!countTargets(spec_, &scan_.rescan_total, &error)) return error;
        persistPassState();
        if (scan_.rescan_total == 0) return ScanManager::tr("The result list holds no valid target");
        setTested(0, scan_.rescan_total);
        return {};
    }

    QString ScanWorker::loadSpec() {
        if (!spec_.entries.empty()) return {};
        auto &lists = *Configs::dataManager->ipListsRepo;
        if (rescanPass()) {
            const auto entries = lists.GetEntries(scan_.rescan_snapshot_list_id);
            if (entries.isEmpty()) {
                scan_.rescan_snapshot_list_id = -1;
                return ScanManager::tr("The copy of the result list is gone; rescan the results again");
            }
            spec_ = scanWorkerBuildSpec(entries, {}, QStringLiteral("list"), false, 0);
            return {};
        }
        const auto entries = lists.GetEntries(scan_.snapshot_list_id);
        if (entries.isEmpty()) {
            scan_.snapshot_list_id = -1;
            return ScanManager::tr("The copy of the base IP list is gone; restart the scan");
        }
        spec_ = scanWorkerBuildSpec(entries, scan_.config.ports, scan_.config.PortModeKey(), scan_.config.shuffle, scan_.seed);
        return {};
    }

    QString ScanWorker::prepareResolverProbe() {
        const auto unusable = ScanManager::tr("The DNS tunnel profile cannot be used to find resolvers");
        const auto protocol = base_ == nullptr ? QString() : DnsTunnelProtocol(*base_);
        if (protocol.isEmpty()) return unusable;
        auto *source = Configs::ProfileResolverSource(*base_);
        if (source != nullptr) profileResolvers_ = scanWorkerProfileResolvers(*source, protocol, base_->name);
        QJsonObject config;
        if (protocol == QLatin1String("xdns")) {
            auto finalmask = base_->outbound->GetXrayStream()->finalmask;
            Configs::UpgradeXrayFinalmask(finalmask);
            config = scanWorkerXdnsSettings(finalmask);
        } else if (source != nullptr) {
            // The core replaces the resolvers per target; a placeholder keeps Build() off the IP lists.
            const auto saved = std::exchange(*source, Configs::ResolverSource{{QStringLiteral("127.0.0.1")}, -1});
            const auto built = base_->outbound->Build();
            *source = saved;
            if (!built.error.isEmpty()) return built.error;
            config = built.object;
        }
        if (config.isEmpty()) return unusable;
        dns_ = {};
        dns_.enabled = true;
        dns_.protocol = protocol.toStdString();
        dns_.config_json = QJsonObject2QString(config, true).toStdString();
        return {};
    }

    QString ScanWorker::preflight(bool &completed) {
        setStage(Stage::Probing);
        pushLog(ScanManager::tr("Checking for DNS interception"));
        const auto interception = runTargets({Configs::IpListEntry{QString::fromLatin1(kScanWorkerInterceptionProbe), 53}});
        if (stop_) return {};
        const bool intercepted = interception.error.isEmpty() && !interception.hits.isEmpty();
        auto targets = systemResolvers_;
        if (intercepted) {
            const auto line = ScanManager::tr("This network answers DNS for any address; probing only the system resolvers");
            pushLog(line);
            MW_show_log(ScanManager::tr("Scan %1: %2").arg(scan_.name, line));
        } else {
            targets << profileResolvers_;
        }
        targets = scanWorkerUniqueTargets(targets, kScanWorkerPreflightCap);

        if (!targets.isEmpty()) {
            pushLog(ScanManager::tr("Probing %n system or profile resolver(s)", nullptr, static_cast<int>(targets.size())));
            const auto result = runTargets(targets);
            if (stop_) return {};
            if (!result.error.isEmpty()) return result.error;
            if (!result.replied && scanWorkerAnyOf(result.localFailures, false)) return ScanManager::tr("Network appears to be down; scan paused");
            tally(result);
            if (storeHits(result)) {
                scan_.found = Configs::dataManager->ipListsRepo->EntryCount(scan_.result_list_id);
                {
                    QMutexLocker locker(&mutex_);
                    live_.found = scan_.found;
                }
                saveProgress();
                if (hooks_.results) hooks_.results();
            }
            if (result.hits.isEmpty() && !intercepted)
                pushLog(ScanManager::tr("The tunnel answered through none of the system or profile resolvers; check the profile or the server"));
        }
        if (intercepted) completed = true;
        return {};
    }

    QString ScanWorker::runChunks(bool &completed) {
        auto &lists = *Configs::dataManager->ipListsRepo;
        const int resultId = scan_.result_list_id;
        const int chunkSize = std::clamp(scan_.config.concurrency * 4, 32, 2048);
        if (persist_ && resultId >= 0) {
            scan_.found = lists.EntryCount(resultId);
            QMutexLocker locker(&mutex_);
            live_.found = scan_.found;
        }
        setTested(scan_.ActiveCursor(), scan_.ActiveTotal());

        for (;;) {
            const bool rescan = rescanPass();
            quint64 &cursor = rescan ? scan_.rescan_cursor : scan_.cursor;
            const quint64 total = rescan ? scan_.rescan_total : scan_.total;
            if (!rescan && stopAfterReached()) {
                pushLog(ScanManager::tr("Found %n new result(s); stopping as configured", nullptr, runFound_));
                stoppedByLimit_ = true;
                completed = true;
                return {};
            }
            if (cursor >= total) {
                completed = true;
                return {};
            }
            if (stop_) return {};

            const quint64 chunkStart = cursor;
            beginChunk(chunkStart, static_cast<int>(std::min<quint64>(chunkSize, total - chunkStart)));
            quint64 next = chunkStart;
            auto result = runChunk(spec_, chunkStart, chunkSize, &next);
            if (rescan && result.error.isEmpty() && !result.aborted && !result.failures.isEmpty())
                confirmFailures(result, chunkStart, next);

            bool unverified = false;
            QString networkError;
            if (result.error.isEmpty()) {
                // Any reply proves the network up; a silent chunk asks the system resolvers.
                const bool silent = !result.replied && !result.failures.isEmpty();
                // An outage can begin mid-chunk or during the retries, so a removal needs a fresh check.
                const bool removes = rescan && (removals_.WouldRemove(result.hits, result.failures, false) ||
                                                removals_.WouldRemove(result.hits, result.failures, true));
                if (!result.replied && scanWorkerAnyOf(result.localFailures, false)) {
                    // Without a default route every target fails locally, and the DNS check reports up when it knows no resolver.
                    unverified = true;
                    networkError = ScanManager::tr("Network appears to be down; scan paused");
                } else if (rescan && result.aborted && !result.failures.isEmpty()) {
                    // Cut short before its failures were retried, so they prove nothing.
                    unverified = true;
                } else if (silent || removes) {
                    QString detail;
                    if (stop_) {
                        unverified = true;
                    } else if (!networkUp(&detail, nullptr)) {
                        unverified = true;
                        networkError = ScanManager::tr("Network appears to be down; scan paused");
                        if (!detail.isEmpty()) MW_show_log(ScanManager::tr("Scan %1: network check failed: %2").arg(scan_.name, detail));
                    }
                }
            }
            const bool settled = result.error.isEmpty() && !unverified;
            if (settled) tally(result);

            bool resultsChanged = false;
            if (persist_ && resultId >= 0) {
                resultsChanged = storeHits(result);
                if (rescan && settled) {
                    removals_.Add(result.hits, result.failures, chunkStart);
                    if (const auto dead = removals_.TakeRemovable(); !dead.isEmpty()) {
                        if (const int removed = lists.RemoveEntries(resultId, dead); removed > 0) {
                            scan_.removed += removed;
                            resultsChanged = true;
                        }
                    }
                }
                scan_.found = lists.EntryCount(resultId);
            }

            if (!result.error.isEmpty() || unverified) cursor = chunkStart;
            else cursor = next;
            cursor = std::min(cursor, total);

            {
                QMutexLocker locker(&mutex_);
                chunkActive_ = false;
                live_.found = scan_.found;
                live_.removed = scan_.removed;
            }
            saveProgress();
            setTested(cursor, total);
            if (resultsChanged && hooks_.results) hooks_.results();

            if (!result.error.isEmpty()) return result.error;
            if (!networkError.isEmpty()) return networkError;
            if (unverified) return stop_ ? QString() : ScanManager::tr("The core ended the scan session early");
            // The loop top completes a finished pass, even when a stop landed after its last chunk.
            if (cursor >= total || (!rescan && stopAfterReached())) continue;
            if (stop_) return {};
            if (result.aborted) return ScanManager::tr("The core ended the scan session early");
            if (cursor == chunkStart) return ScanManager::tr("The core made no progress on the scan");
        }
    }

    // A rescan deletes what fails, so one lost exchange must not count: failures are tried again first.
    void ScanWorker::confirmFailures(ChunkResult &result, quint64 chunkStart, quint64 &next) {
        const qsizetype hitsBefore = result.hits.size();
        const auto reportRescued = qScopeGuard([&] {
            if (const auto rescued = static_cast<int>(result.hits.size() - hitsBefore); rescued > 0)
                pushLog(ScanManager::tr("%n result(s) passed only on a retry; the network fails some attempts at random", nullptr, rescued));
        });
        for (int round = 0; round < kScanWorkerRescanRetries && !result.failures.isEmpty(); ++round) {
            for (int waited = 0; waited < kScanWorkerRetryDelayMs && !stop_; waited += 100) QThread::msleep(100);
            if (stop_) {
                result.failures.clear();
                result.aborted = true;
                next = chunkStart;
                return;
            }
            pushLog(ScanManager::tr("Re-testing %n failed result(s)…", nullptr, static_cast<int>(result.failures.size())));
            auto retry = runTargets(result.failures);
            if (!retry.error.isEmpty() || retry.aborted) {
                // Unconfirmed, so nothing is removed; a pause re-runs the whole chunk instead.
                result.failures.clear();
                if (stop_) {
                    result.aborted = true;
                    next = chunkStart;
                }
                return;
            }
            result.hits << retry.hits;
            result.replaced << retry.replaced;
            result.localFailures << retry.localFailures;
            result.failures = retry.failures;
            result.reasons.insert(retry.reasons);
            result.shortNamesOnly.unite(retry.shortNamesOnly);
            result.replied = result.replied || retry.replied;
        }
    }

    ScanWorker::ChunkResult ScanWorker::runChunk(const libcore::ScanTargetSpec &spec, quint64 chunkStart, int maxTargets,
                                                 quint64 *nextCursor) {
        ChunkResult result;
        setStage(Stage::Probing);

        libcore::ScanProbeRequest request;
        request.session_id = sessionId_.toStdString();
        request.spec = spec;
        request.cursor = chunkStart;
        request.max_targets = maxTargets;
        fillProbeRequest(request);

        bool ok = false;
        QString coreError;
        const auto response = API::defaultClient->ScanProbe(&ok, request, &coreError, probeTimeoutMs(maxTargets));

        for (const auto &item : response.results) {
            ProbeOutcome outcome;
            outcome.address = scanWorkerText(item.address.value());
            outcome.port = item.port.value();
            outcome.probePort = item.probe_port.value();
            outcome.latencyMs = item.dns_ms.value();
            outcome.lossPct = scanWorkerLossPct(item.sent.value(), item.answered.value());
            if (item.dns_ms.value() > 0 || item.passed.value()) result.replied = true;
            if (item.passed.value()) {
                addHit(result, outcome);
            } else if (item.local_failure.value()) {
                result.localFailures << scanWorkerEntry(outcome.address, outcome.port, 0);
            } else {
                const auto entry = scanWorkerEntry(outcome.address, outcome.port, 0);
                result.failures << entry;
                auto reason = scanWorkerText(item.dns_reason.value());
                if (reason.isEmpty() && item.dns_ms.value() <= 0) reason = QStringLiteral("timeout");
                result.reasons.insert(scanWorkerKey(entry), reason);
                if (item.short_names_only.value()) result.shortNamesOnly.insert(scanWorkerKey(entry));
            }
        }

        if (!ok || !response.error.value().empty()) {
            // The positions of a call cut short stay unfinished, so its failures prove nothing.
            result.failures.clear();
            result.localFailures.clear();
            result.reasons.clear();
            result.shortNamesOnly.clear();
            const auto error = ok ? scanWorkerText(response.error.value()) : coreError;
            if (scanWorkerIsStopped(error)) stop_.store(true);
            if (stop_) {
                result.aborted = true;
                return result;
            }
            result.error = error.isEmpty() ? ScanManager::tr("The core did not answer the probe request") : error;
            return result;
        }
        *nextCursor = std::max<quint64>(chunkStart, response.next_cursor.value());
        result.aborted = response.aborted.value();
        return result;
    }

    ScanWorker::ChunkResult ScanWorker::runTargets(const QList<Configs::IpListEntry> &targets) {
        quint64 next = 0;
        return runChunk(scanWorkerBuildSpec(targets, {}, QStringLiteral("list"), false, 0), 0, static_cast<int>(targets.size()), &next);
    }

    void ScanWorker::addHit(ChunkResult &result, const ProbeOutcome &candidate) const {
        const int port = candidate.probePort > 0 ? candidate.probePort : candidate.port;
        auto hit = scanWorkerEntry(candidate.address, port, candidate.latencyMs);
        hit.lossPct = candidate.lossPct;
        result.hits << hit;
        // A hit under another port must replace the stored rescan row, not add a sibling.
        if (rescanPass() && port != candidate.port) result.replaced << scanWorkerEntry(candidate.address, candidate.port, 0);
    }

    bool ScanWorker::storeHits(const ChunkResult &result) {
        if (!persist_ || scan_.result_list_id < 0 || result.hits.isEmpty()) return false;
        auto &lists = *Configs::dataManager->ipListsRepo;
        const int resultId = scan_.result_list_id;
        if (std::exchange(replacePending_, false)) {
            lists.ReplaceEntries(resultId, result.hits);
            ClearReplacePending(scan_);
            runFound_ = lists.EntryCount(resultId);
            return true;
        }
        runFound_ += lists.UpsertEntries(resultId, result.hits);
        if (rescanPass() && !result.replaced.isEmpty()) lists.RemoveEntries(resultId, result.replaced);
        return true;
    }

    void ScanWorker::tally(const ChunkResult &result) {
        for (const auto &entry : result.failures) {
            const auto key = scanWorkerKey(entry);
            const auto reason = result.reasons.value(key);
            if (reason == QLatin1String("timeout")) {
                ++tally_.noAnswer;
            } else if (reason == QLatin1String("rcode")) {
                ++tally_.errorReply;
            } else if (reason == QLatin1String("answer")) {
                ++tally_.wrongAnswer;
            } else if (reason == QLatin1String("qname")) {
                if (result.shortNamesOnly.contains(key)) ++tally_.shortNamesOnly;
                else ++tally_.nameTooLong;
            } else {
                ++tally_.other;
            }
        }
        for (const auto &hit : result.hits) {
            if (hit.lossPct >= kScanWorkerHeavyLossPct) ++tally_.heavyLoss;
        }
    }

    void ScanWorker::logSummary() {
        const auto &t = tally_;
        QStringList parts;
        if (t.noAnswer > 0) parts << ScanManager::tr("no answer (%1)").arg(t.noAnswer);
        if (t.errorReply > 0) parts << ScanManager::tr("error reply (%1)").arg(t.errorReply);
        if (t.wrongAnswer > 0) parts << ScanManager::tr("wrong answer (%1)").arg(t.wrongAnswer);
        if (t.nameTooLong > 0) parts << ScanManager::tr("name too long (%1)").arg(t.nameTooLong);
        if (t.shortNamesOnly > 0) parts << ScanManager::tr("short names only (%1)").arg(t.shortNamesOnly);
        if (t.other > 0) parts << ScanManager::tr("other (%1)").arg(t.other);
        QStringList lines;
        if (!parts.isEmpty()) lines << ScanManager::tr("Failures: %1").arg(parts.join(QStringLiteral(", ")));
        if (t.heavyLoss > 0) lines << ScanManager::tr("%n passed with heavy loss", nullptr, t.heavyLoss);
        const auto *dnstt = base_ != nullptr ? base_->Dnstt() : nullptr;
        if (t.shortNamesOnly > 0 && dnstt != nullptr && dnstt->mode == QLatin1String("vaydns"))
            lines << ScanManager::tr("Lower Max QNAME Length in the profile to use the resolvers that pass only short names");
        for (const auto &line : lines) {
            pushLog(line);
            MW_show_log(ScanManager::tr("Scan %1: %2").arg(scan_.name, line));
        }
    }

    bool ScanWorker::networkUp(QString *detail, QStringList *systemResolvers) {
        setStage(Stage::CheckingNetwork);
        pushLog(ScanManager::tr("Checking the network"));
        bool ok = false;
        const bool up = API::defaultClient->ScanCheckNetworkDns(&ok, kScanWorkerNetworkCheckTimeoutMs, systemResolvers, detail);
        return ok && up;
    }

    void ScanWorker::finishPass(bool completed, const QString &error) {
        using Status = Configs::IpScan::Status;
        if (completed) {
            if (rescanPass()) {
                auto lines = removals_.KeptLines();
                if (!removals_.AnyPassed() && removals_.PendingCount() > 0)
                    lines << ScanManager::tr("The tunnel answered through none of the %n result(s), so none was removed", nullptr,
                                             removals_.PendingCount());
                for (const auto &line : lines) {
                    pushLog(line);
                    MW_show_log(ScanManager::tr("Scan %1: %2").arg(scan_.name, line));
                }
                deleteList(scan_.rescan_snapshot_list_id);
                scan_.rescan_snapshot_list_id = -1;
                if (scan_.CanResumeInitial()) {
                    scan_.mode = Configs::IpScan::Mode::Initial;
                    // A pass that stopped at its limit stays Completed; Resume still finds more.
                    scan_.status = initialStatus_ == Status::Completed ? Status::Completed : Status::Paused;
                } else {
                    scan_.status = Status::Completed;
                }
            } else {
                // A pass that stopped at its limit keeps its snapshot, so Resume finds more.
                if (!stoppedByLimit_ || !scan_.CanResumeInitial()) {
                    deleteList(scan_.snapshot_list_id);
                    scan_.snapshot_list_id = -1;
                }
                scan_.status = Status::Completed;
            }
            if (scan_.status == Status::Completed) scan_.finished_at = QDateTime::currentSecsSinceEpoch();
            scan_.last_error.clear();
            logSummary();
        } else {
            if (rescanPass()) scan_.rescan_cursor = std::min(scan_.rescan_cursor, removals_.UnresolvedFrom());
            const bool rescanResumable =
                rescanPass() && scan_.rescan_snapshot_list_id >= 0 && scan_.rescan_cursor < scan_.rescan_total;
            if (rescanPass() && !rescanResumable && scan_.CanResumeInitial()) scan_.mode = Configs::IpScan::Mode::Initial;
            scan_.last_error = error;
            if (rescanResumable || scan_.CanResumeInitial()) scan_.status = Status::Paused;
            else if (!error.isEmpty()) scan_.status = Status::Failed;
            else scan_.status = initialStatus_ == Status::Completed ? Status::Completed : Status::Idle;
        }
        persistPassState();
        finalized_.store(true);
        if (persist_) IpListUpdater::instance()->NotifyListsChanged();
        if (!error.isEmpty()) MW_show_log(ScanManager::tr("Scan %1 stopped: %2").arg(scan_.name, error));

        QMutexLocker locker(&mutex_);
        live_.error = scan_.last_error;
        live_.tested = scan_.ActiveCursor();
        live_.total = scan_.ActiveTotal();
        live_.found = scan_.found;
        live_.removed = scan_.removed;
    }

    void ScanWorker::persistPassState() {
        if (!persist_) return;
        auto &scans = *Configs::dataManager->ipScansRepo;
        // Re-read so a rename made while running survives: the run owns only the pass columns.
        const auto row = scans.GetIpScan(scanId_);
        if (row == nullptr) return;
        row->result_list_id = scan_.result_list_id;
        row->status = scan_.status;
        row->mode = scan_.mode;
        row->snapshot_list_id = scan_.snapshot_list_id;
        row->seed = scan_.seed;
        row->cursor = scan_.cursor;
        row->total = scan_.total;
        row->spec_hash = scan_.spec_hash;
        row->rescan_snapshot_list_id = scan_.rescan_snapshot_list_id;
        row->rescan_cursor = scan_.rescan_cursor;
        row->rescan_total = scan_.rescan_total;
        row->found = scan_.found;
        row->removed = scan_.removed;
        row->last_error = scan_.last_error;
        row->started_at = scan_.started_at;
        row->finished_at = scan_.finished_at;
        scans.Save(row);
    }

    void ScanWorker::saveProgress() {
        if (!persist_) return;
        const quint64 cursor = rescanPass() ? std::min(scan_.rescan_cursor, removals_.UnresolvedFrom()) : scan_.cursor;
        Configs::dataManager->ipScansRepo->SaveProgress(scanId_, scan_.mode, cursor, scan_.ActiveTotal(), scan_.found, scan_.removed);
    }

    void ScanWorker::deleteList(int listId) {
        if (listId < 0 || !persist_) return;
        Configs::dataManager->ipListsRepo->DeleteIpList(listId);
        listsTouched_ = true;
    }

    bool ScanWorker::countTargets(const libcore::ScanTargetSpec &spec, quint64 *total, QString *error) {
        libcore::ScanProbeRequest request;
        request.session_id = sessionId_.toStdString();
        request.spec = spec;
        request.cursor = 0;
        request.max_targets = 0;
        bool ok = false;
        QString coreError;
        const auto response = API::defaultClient->ScanProbe(&ok, request, &coreError, kScanWorkerCountTimeoutMs);
        const auto responseError = ok ? scanWorkerText(response.error.value()) : coreError;
        if (!ok || !responseError.isEmpty()) {
            if (scanWorkerIsStopped(responseError)) stop_.store(true);
            if (stop_) error->clear();
            else *error = responseError.isEmpty() ? ScanManager::tr("The core did not answer the probe request") : responseError;
            return false;
        }
        *total = response.total.value();
        if (const int invalid = response.invalid_entries.value(); invalid > 0)
            pushLog(ScanManager::tr("Skipped %n invalid entry(ies)", nullptr, invalid));
        return true;
    }

    void ScanWorker::fillProbeRequest(libcore::ScanProbeRequest &request) const {
        request.concurrency = scan_.config.concurrency;
        request.spawn_interval_ms = scan_.config.spawnIntervalMs;
        request.dns = dns_;
    }

    int ScanWorker::probeTimeoutMs(int chunkSize) const {
        const auto &config = scan_.config;
        const qint64 concurrency = std::max(1, config.concurrency);
        const qint64 rounds = (std::max(chunkSize, 1) + concurrency - 1) / concurrency;
        return scanWorkerClampTimeout(rounds * kScanWorkerTargetBoundMs + qint64(chunkSize) * std::max(0, config.spawnIntervalMs) +
                                      kScanWorkerRpcSlackMs);
    }

    bool ScanWorker::stopAfterReached() const {
        return scan_.config.stopAfter > 0 && runFound_ >= scan_.config.stopAfter;
    }

    void ScanWorker::pollLoop() {
        while (!pollStop_.load()) {
            QThread::msleep(kScanWorkerPollIntervalMs);
            if (pollStop_.load()) break;
            pollOnce();
        }
    }

    void ScanWorker::pollOnce() {
        bool ok = false;
        const auto response = API::defaultClient->QueryScan(&ok, sessionId_, lastSeq_);
        if (!ok || pollStop_.load()) return;
        // A lower sequence means the core restarted and the session began anew from 1.
        const qint64 seq = response.last_seq.value();
        lastSeq_ = seq < lastSeq_ ? 0 : seq;

        QStringList lines;
        const auto &events = response.events;
        const auto keep = static_cast<size_t>(ScanManager::kLogLines);
        for (size_t i = events.size() > keep ? events.size() - keep : 0; i < events.size(); ++i) {
            if (auto line = scanWorkerFormatEvent(events[i]); !line.isEmpty()) lines << line;
        }

        bool changed = !lines.isEmpty();
        {
            QMutexLocker locker(&mutex_);
            lastProbed_ = std::max<qint64>(lastProbed_, response.probed.value());
            for (const auto &line : lines) live_.log << line;
            while (live_.log.size() > ScanManager::kLogLines) live_.log.removeFirst();
            if (chunkActive_) {
                const quint64 inChunk = static_cast<quint64>(std::clamp<qint64>(lastProbed_ - baseProbed_, 0, qint64(chunkSize_)));
                if (chunkStart_ + inChunk > live_.tested) {
                    live_.tested = chunkStart_ + inChunk;
                    changed = true;
                }
            }
        }
        if (changed) notifyProgress();
    }

    void ScanWorker::beginChunk(quint64 chunkStart, int chunkSize) {
        bool ok = false;
        // The core computes after_seq + 1, so max() would wrap and return the newest events.
        const auto counters = API::defaultClient->QueryScan(&ok, sessionId_, std::numeric_limits<qint64>::max() - 1);
        QMutexLocker locker(&mutex_);
        // Counters are idle between chunks, and a restarted core counts from zero again.
        if (ok) lastProbed_ = counters.probed.value();
        baseProbed_ = lastProbed_;
        chunkStart_ = chunkStart;
        chunkSize_ = static_cast<quint64>(std::max(chunkSize, 0));
        chunkActive_ = true;
        live_.tested = chunkStart;
    }

    void ScanWorker::setStage(Stage stage) {
        QString text;
        switch (stage) {
            case Stage::Preparing:
                text = ScanManager::tr("Preparing");
                break;
            case Stage::Probing:
                text = ScanManager::tr("Probing");
                break;
            case Stage::CheckingNetwork:
                text = ScanManager::tr("Checking network");
                break;
        }
        {
            QMutexLocker locker(&mutex_);
            if (live_.stage == text) return;
            live_.stage = text;
        }
        notifyProgress();
    }

    void ScanWorker::setTested(quint64 tested, quint64 total) {
        {
            QMutexLocker locker(&mutex_);
            live_.tested = tested;
            live_.total = total;
        }
        notifyProgress();
    }

    void ScanWorker::pushLog(const QString &line) {
        {
            QMutexLocker locker(&mutex_);
            live_.log << line;
            while (live_.log.size() > ScanManager::kLogLines) live_.log.removeFirst();
        }
        notifyProgress();
    }

    void ScanWorker::notifyProgress() const {
        if (hooks_.progress) hooks_.progress();
    }
} // namespace Scanner
