#include "include/scanner/ScanWorker.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QSet>
#include <QScopeGuard>
#include <QThread>

#include <algorithm>
#include <limits>
#include <utility>

#include "include/api/RPC.h"
#include "include/configs/common/DnsTunnel.h"
#include "include/configs/common/ResolverSource.h"
#include "include/configs/common/xrayStreamSetting.h"
#include "include/configs/generate.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include "include/scanner/IpListParse.h"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanProfiles.h"

namespace Scanner {
    namespace {
        constexpr int kScanWorkerPollIntervalMs = 250;
        constexpr int kScanWorkerCountTimeoutMs = 60000;
        constexpr int kScanWorkerNetworkCheckTimeoutMs = 3000;
        // Mirror the core's TunnelHandshakeTimeout, TunnelStartupTimeout and normalizeConcurrency.
        constexpr qint64 kScanWorkerTunnelHandshakeMs = 10000;
        constexpr qint64 kScanWorkerTunnelStartupMs = 5000;
        constexpr int kScanWorkerCoreMaxConcurrency = 100;
        // Targets left without any port.
        constexpr int kScanWorkerFallbackPort = 443;
        constexpr int kScanWorkerPlainHttpPort = 80;
        // Networks that reset a share of handshakes at random need several tries before a live entry is ruled dead.
        constexpr int kScanWorkerRescanRetries = 5;
        constexpr int kScanWorkerRetryDelayMs = 1000;
        constexpr qint64 kScanWorkerRpcSlackMs = 30000;

        const QStringList kScanWorkerNetworkCheckTargets = {
            QStringLiteral("1.1.1.1:443"),
            QStringLiteral("8.8.8.8:443"),
            QStringLiteral("9.9.9.9:443"),
            QStringLiteral("223.5.5.5:443"),
            QStringLiteral("77.88.8.8:443"),
        };

        const QStringList kScanWorkerNetworkCheckTargetsV6 = {
            QStringLiteral("[2606:4700:4700::1111]:443"),
            QStringLiteral("[2001:4860:4860::8888]:443"),
            QStringLiteral("[2620:fe::fe]:443"),
        };

        std::atomic<quint32> scanWorkerSessionCounter{0};

        QString scanWorkerText(const std::string &text) {
            return QString::fromStdString(text);
        }

        int scanWorkerClampTimeout(qint64 ms) {
            return static_cast<int>(std::min<qint64>(ms, std::numeric_limits<int>::max()));
        }

        bool scanWorkerIsAborted(const QString &error) {
            return error.contains(QStringLiteral("test aborted")) || error.contains(QStringLiteral("context canceled"));
        }

        bool scanWorkerIsStopped(const QString &error) {
            return error.contains(QStringLiteral("scan stopped"));
        }

        QString scanWorkerPhaseName(const QString &phase) {
            if (phase == QLatin1String("icmp")) return QStringLiteral("ICMP");
            if (phase == QLatin1String("tcp")) return QStringLiteral("TCP");
            if (phase == QLatin1String("tls")) return QStringLiteral("TLS");
            if (phase == QLatin1String("http")) return QStringLiteral("HTTP");
            if (phase == QLatin1String("dns")) return QStringLiteral("DNS");
            if (phase == QLatin1String("url")) return ScanManager::tr("config test");
            return phase;
        }

        QString scanWorkerFormatEvent(const libcore::ScanEvent &event) {
            const auto kind = scanWorkerText(event.kind.value());
            const auto phase = scanWorkerText(event.phase.value());
            const auto target = scanWorkerText(event.target.value());
            const int ms = event.latency_ms.value();
            if (kind == QLatin1String("start")) return ScanManager::tr("Scanning %1 with %2").arg(target, scanWorkerPhaseName(phase));
            if (kind == QLatin1String("fail"))
                return ScanManager::tr("%1 failed %2: %3").arg(target, scanWorkerPhaseName(phase), scanWorkerText(event.error.value()));
            if (kind != QLatin1String("ok")) return {};
            if (phase == QLatin1String("tcp")) return ScanManager::tr("Connected to %1 with TCP (%2 ms)").arg(target).arg(ms);
            if (phase == QLatin1String("icmp")) return ScanManager::tr("%1 replied to ping (%2 ms)").arg(target).arg(ms);
            if (phase == QLatin1String("tls")) return ScanManager::tr("TLS handshake with %1 succeeded (%2 ms)").arg(target).arg(ms);
            if (phase == QLatin1String("http")) return ScanManager::tr("%1 answered over HTTP (%2 ms)").arg(target).arg(ms);
            if (phase == QLatin1String("url")) return ScanManager::tr("%1 passed the config test (%2 ms)").arg(target).arg(ms);
            if (phase == QLatin1String("dns")) return ScanManager::tr("%1 relays the DNS tunnel (%2 ms)").arg(target).arg(ms);
            return {};
        }

        bool scanWorkerIsIpv6(const Configs::IpListEntry &entry) {
            return entry.cidr.contains(QLatin1Char(':'));
        }

        bool scanWorkerHasFamily(const QList<Configs::IpListEntry> &entries, bool ipv6) {
            return std::any_of(entries.cbegin(), entries.cend(),
                               [ipv6](const Configs::IpListEntry &entry) { return scanWorkerIsIpv6(entry) == ipv6; });
        }

        QList<Configs::IpListEntry> scanWorkerFamilyFilter(const QList<Configs::IpListEntry> &entries, bool ipv6) {
            if (ipv6) return entries;
            QList<Configs::IpListEntry> out;
            out.reserve(entries.size());
            for (const auto &entry : entries) {
                if (!scanWorkerIsIpv6(entry)) out.append(entry);
            }
            return out;
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

        // Exact keys suffice only on a finalmask UpgradeXrayFinalmask has canonicalized.
        QJsonObject scanWorkerXdnsSettings(const QJsonObject &finalmask) {
            for (const auto &mask : finalmask.value(QLatin1String("udp")).toArray()) {
                const auto object = mask.toObject();
                if (object.value(QLatin1String("type")).toString() == QLatin1String("xdns"))
                    return object.value(QLatin1String("settings")).toObject();
            }
            return {};
        }

        int scanWorkerProbeLatency(const libcore::ScanProbeResult &result, const Configs::ScanConfig &config) {
            if (config.http.enabled) {
                if (result.http_ms.value() > 0) return result.http_ms.value();
                if (result.tls_ms.value() > 0) return result.tls_ms.value();
            }
            if (config.tcp.enabled && result.tcp_ms.value() > 0) return result.tcp_ms.value();
            if (config.icmp.enabled) return result.icmp_ms.value();
            return 0;
        }

        struct ScanWorkerConfigTest {
            QString url;
            int timeoutMs = 0;
            int concurrency = 1;
            bool warmLatency = true;
            int subBatch = 8;
        };

        ScanWorkerConfigTest scanWorkerConfigTest(const Configs::IpScan &scan) {
            ScanWorkerConfigTest test;
            const int concurrency = std::max(1, scan.config.concurrency);
            if (scan.kind == Configs::IpScan::Kind::Warp) {
                const auto &warp = scan.config.warp;
                test = {warp.url, warp.timeoutMs, concurrency, warp.warmLatency, std::clamp(concurrency * 4, 8, 32)};
            } else {
                const auto &config = scan.config.config;
                test = {config.url, config.timeoutMs, concurrency, config.warmLatency, std::clamp(concurrency * 4, 8, 100)};
            }
            if (test.url.trimmed().isEmpty()) test.url = Configs::dataManager->settingsRepo->test_latency_url;
            if (test.timeoutMs <= 0) test.timeoutMs = Configs::dataManager->settingsRepo->url_test_timeout_ms;
            return test;
        }

        // Test answers only once the whole batch is done, so the deadline covers its worst case.
        int scanWorkerUrlTestTimeoutMs(qsizetype tagCount, const ScanWorkerConfigTest &test, qint64 startupAllowanceMs) {
            int concurrency = test.concurrency;
            if (concurrency <= 0 || concurrency >= 500) concurrency = kScanWorkerCoreMaxConcurrency;
            const qint64 requests = test.warmLatency ? 2 : 1;
            const qint64 rounds = (std::max<qsizetype>(tagCount, 1) + concurrency - 1) / concurrency;
            return scanWorkerClampTimeout(
                rounds * (kScanWorkerTunnelHandshakeMs + requests * test.timeoutMs + kScanWorkerTunnelStartupMs + startupAllowanceMs) +
                kScanWorkerRpcSlackMs);
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

    ScanWorker::ScanWorker(const Configs::IpScan &scan, StartMode mode, std::shared_ptr<Configs::Profile> base, Hooks hooks)
        : scanId_(scan.id),
          sessionId_(QStringLiteral("scan-%1-%2-%3")
                         .arg(scan.id)
                         .arg(QDateTime::currentMSecsSinceEpoch())
                         .arg(scanWorkerSessionCounter.fetch_add(1))),
          startMode_(mode),
          base_(std::move(base)),
          hooks_(std::move(hooks)),
          configPhase_(scan.kind == Configs::IpScan::Kind::Warp ||
                       (scan.kind == Configs::IpScan::Kind::Generic && scan.config.config.enabled)),
          resolverPhase_(scan.kind == Configs::IpScan::Kind::Resolver),
          initialStatus_(scan.status),
          scan_(scan) {
        if (scan_.kind != Configs::IpScan::Kind::Generic) {
            scan_.config.tcp.enabled = false;
            scan_.config.http.enabled = false;
        }
        if (resolverPhase_) {
            scan_.config.icmp.enabled = false;
            scan_.config.config.enabled = false;
        }
        scan_.status = Configs::IpScan::Status::Running;
        scan_.last_error.clear();
        live_.running = true;
        live_.stage = ScanManager::tr("Preparing");
        live_.found = scan.found;
        if (mode == StartMode::Resume) {
            live_.tested = scan.ActiveCursor();
            live_.total = scan.ActiveTotal();
            live_.removed = scan.removed;
        } else if (mode == StartMode::FromInitial) {
            live_.found = 0;
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
        if (configPhase_) {
            if (base_ == nullptr) return ScanManager::tr("The scan has no profile for the config test");
            if (auto error = Configs::ValidateScanBase(base_); !error.isEmpty()) return error;
            // A clone resolves the port the profile dials, including one held only by an OpenVPN remote list.
            auto dialed = base_;
            if (const auto *chain = base_->Chain(); chain != nullptr)
                dialed = chain->list.isEmpty() ? nullptr : Configs::dataManager->profilesRepo->GetProfile(chain->list.first());
            if (const auto probe = Configs::CloneProfileWithServer(dialed, QStringLiteral("127.0.0.1"), 0); probe != nullptr)
                basePort_ = probe->outbound->GetPort().toInt();
        }
        if (resolverPhase_) {
            if (auto error = prepareResolverProbe(); !error.isEmpty()) return error;
        }
        if (stop_) return {};
        if (auto error = preparePass(); !error.isEmpty()) return error;
        if (std::exchange(listsTouched_, false)) IpListUpdater::instance()->NotifyListsChanged();
        if (stop_) return {};
        if (auto error = loadSpec(); !error.isEmpty()) return error;
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
        if (!rescanResumable) scan_.mode = Configs::IpScan::Mode::Initial;
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
        const auto baseEntries = scanWorkerFamilyFilter(base->entries, scan_.config.scanIPv6);
        if (baseEntries.isEmpty()) return ScanManager::tr("The base IP list holds only IPv6 ranges; enable IPv6 scanning");
        if (!persist_ || stop_) return {};

        const int resultId = EnsureScanResultList(scan_);
        if (resultId < 0) return ScanManager::tr("The result list could not be created");
        if (!persist_) return {};
        lists.ClearEntries(resultId);
        listsTouched_ = true;
        deleteList(scan_.snapshot_list_id);
        deleteList(scan_.rescan_snapshot_list_id);
        scan_.snapshot_list_id = -1;
        scan_.rescan_snapshot_list_id = -1;
        scan_.rescan_cursor = 0;
        scan_.rescan_total = 0;
        scan_.mode = Configs::IpScan::Mode::Initial;
        scan_.cursor = 0;
        scan_.total = 0;
        scan_.found = 0;
        scan_.removed = 0;
        scan_.started_at = QDateTime::currentSecsSinceEpoch();
        scan_.finished_at = 0;
        scan_.last_error.clear();
        {
            QMutexLocker locker(&mutex_);
            live_.found = 0;
            live_.removed = 0;
        }
        if (hooks_.results) hooks_.results();

        auto snapshot = Configs::IpListsRepo::NewIpList();
        snapshot->name = ScanManager::tr("%1 snapshot").arg(scan_.name);
        snapshot->role = Configs::IpList::Role::ScanSnapshot;
        snapshot->related_test_id = scan_.id;
        snapshot->entries = baseEntries;
        snapshot->entryCount = static_cast<int>(baseEntries.size());
        snapshot->entriesLoaded = true;
        if (!persist_) return {};
        if (!lists.AddIpList(snapshot)) return ScanManager::tr("The base IP list could not be copied");
        listsTouched_ = true;
        scan_.snapshot_list_id = snapshot->id;
        scan_.seed = QRandomGenerator::system()->generate64();
        scan_.spec_hash = scan_.config.TargetSpecHash(scan_.base_list_id);
        spec_ = scanWorkerBuildSpec(baseEntries, scan_.config.ports, scan_.config.PortModeKey(),
                                    scan_.config.shuffle, scan_.seed);

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
        const auto allEntries = lists.GetEntries(resultId);
        if (allEntries.isEmpty()) {
            persistPassState();
            return ScanManager::tr("The result list is empty");
        }
        // Left out of the snapshot, IPv6 results are neither tested nor removed.
        const auto entries = scanWorkerFamilyFilter(allEntries, scan_.config.scanIPv6);
        if (entries.isEmpty()) {
            persistPassState();
            return ScanManager::tr("The result list holds only IPv6 entries; enable IPv6 scanning");
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
        spec_ = scanWorkerBuildSpec(entries, scan_.config.ports, scan_.config.PortModeKey(),
                                    scan_.config.shuffle, scan_.seed);
        return {};
    }

    QString ScanWorker::prepareResolverProbe() {
        const auto unusable = ScanManager::tr("The DNS tunnel profile cannot be used to find resolvers");
        const auto protocol = base_ == nullptr ? QString() : DnsTunnelProtocol(*base_);
        if (protocol.isEmpty()) return unusable;
        QJsonObject config;
        if (protocol == QLatin1String("xdns")) {
            auto finalmask = base_->outbound->GetXrayStream()->finalmask;
            Configs::UpgradeXrayFinalmask(finalmask);
            config = scanWorkerXdnsSettings(finalmask);
        } else if (auto *source = Configs::ProfileResolverSource(*base_); source != nullptr) {
            // The core replaces the resolvers per target; a placeholder keeps Build() off the IP lists.
            const auto saved = std::exchange(*source, Configs::ResolverSource{{QStringLiteral("127.0.0.1")}, -1});
            const auto built = base_->outbound->Build();
            *source = saved;
            if (!built.error.isEmpty()) return built.error;
            config = built.object;
        }
        if (config.isEmpty()) return unusable;
        const auto &options = scan_.config.resolver;
        const auto level = ResolverScanLevels(protocol).contains(options.level) ? options.level : ResolverScanDefaultLevel(protocol);
        dns_ = {};
        dns_.enabled = true;
        dns_.protocol = protocol.toStdString();
        dns_.config_json = QJsonObject2QString(config, true).toStdString();
        dns_.level = level.toStdString();
        dns_.timeout_ms = options.timeoutMs > 0 ? options.timeoutMs : ResolverScanDefaultTimeoutMs(protocol, level);
        return {};
    }

    QString ScanWorker::runChunks(bool &completed) {
        auto &lists = *Configs::dataManager->ipListsRepo;
        const int resultId = scan_.result_list_id;
        const auto test = scanWorkerConfigTest(scan_);
        const int chunkSize = probePhaseEnabled() ? std::clamp(scan_.config.concurrency * 4, 32, 2048) : test.subBatch;
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
            if (!rescan && scan_.config.stopAfter > 0 && scan_.found >= scan_.config.stopAfter) {
                pushLog(ScanManager::tr("Found %n result(s); stopping as configured", nullptr, scan_.found));
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

            const bool anyFailure = !result.failures.isEmpty() || !result.localFailures.isEmpty();
            const bool answered4 = result.answeredV4 || scanWorkerHasFamily(result.hits, false);
            const bool answered6 = result.answeredV6 || scanWorkerHasFamily(result.hits, true);
            // A hit only proves its own family reachable: an IPv6 hit says nothing about IPv4 failures.
            const bool familyUnproven = (scanWorkerHasFamily(result.failures, false) && !answered4) ||
                                        (scanWorkerHasFamily(result.failures, true) && !answered6);
            const bool guarded =
                result.error.isEmpty() && anyFailure && (rescan || (!answered4 && !answered6) || familyUnproven);
            bool unverified = false;
            bool ipv4Up = false;
            bool ipv6Up = false;
            QString networkError;
            if (guarded && stop_) {
                unverified = true;
            } else if (guarded) {
                const bool ipv4Failed = scanWorkerHasFamily(result.failures, false);
                const bool ipv6Failed = scanWorkerHasFamily(result.failures, true);
                const bool ipv6Any = ipv6Failed || scanWorkerHasFamily(result.localFailures, true);
                const bool ipv6Unproven = ipv6Failed && !answered6;
                QString detail;
                ipv4Up = networkUp(kScanWorkerNetworkCheckTargets, &detail);
                // An initial pass cannot set a failure aside: one whose family is down rewinds the chunk.
                const bool ipv4Blocks = !rescan && !ipv4Up && ipv4Failed;
                if (!ipv4Blocks && ((rescan && ipv6Failed) || (!rescan && ipv6Unproven) || (!ipv4Up && ipv6Any)))
                    ipv6Up = networkUp(kScanWorkerNetworkCheckTargetsV6, nullptr);
                const bool ipv6Blocks = !rescan && !ipv6Up && ipv6Unproven;
                if ((!ipv4Up && !ipv6Up) || ipv4Blocks || ipv6Blocks) {
                    unverified = true;
                    networkError = ipv4Up && ipv6Blocks
                                       ? ScanManager::tr("IPv6 is unreachable from this network; scan paused (turn off IPv6 scanning to go on)")
                                       : ScanManager::tr("Network appears to be down; scan paused");
                    if (!detail.isEmpty()) MW_show_log(ScanManager::tr("Scan %1: network check failed: %2").arg(scanId_).arg(detail));
                } else if (rescan) {
                    if (!ipv4Up && ipv4Failed)
                        pushLog(ScanManager::tr("IPv4 is unreachable from this network; IPv4 results were kept"));
                    if (!ipv6Up && ipv6Failed) pushLog(ScanManager::tr("IPv6 is unreachable from this network; IPv6 results were kept"));
                }
            }

            bool resultsChanged = false;
            if (persist_ && resultId >= 0) {
                if (!result.hits.isEmpty()) {
                    lists.UpsertEntries(resultId, result.hits);
                    if (rescan && !result.replaced.isEmpty()) lists.RemoveEntries(resultId, result.replaced);
                    resultsChanged = true;
                }
                if (rescan && guarded && !unverified) {
                    QList<Configs::IpListEntry> dead;
                    for (const auto &entry : result.failures) {
                        if (scanWorkerIsIpv6(entry) ? ipv6Up : ipv4Up) dead << entry;
                    }
                    if (const int removed = dead.isEmpty() ? 0 : lists.RemoveEntries(resultId, dead); removed > 0) {
                        scan_.removed += removed;
                        resultsChanged = true;
                    }
                }
                scan_.found = lists.EntryCount(resultId);
            }

            if (!result.error.isEmpty() || unverified) cursor = chunkStart;
            else if (result.aborted) cursor = configPhase_ ? chunkStart : next;
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
            if (unverified) return {};
            // The loop top completes a finished pass, even when a stop landed after its last chunk.
            if (cursor >= total || (!rescan && scan_.config.stopAfter > 0 && scan_.found >= scan_.config.stopAfter)) continue;
            if (stop_) return {};
            if (result.aborted) return ScanManager::tr("The core ended the scan session early");
            if (cursor == chunkStart) return ScanManager::tr("The core made no progress on the scan");
        }
    }

    // A rescan deletes what fails, so one lost probe or one flaky config test must not count: failures are tried again first.
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
            const auto spec = scanWorkerBuildSpec(result.failures, {}, QStringLiteral("list"), false, 0);
            quint64 retryNext = 0;
            auto retry = runChunk(spec, 0, static_cast<int>(result.failures.size()), &retryNext);
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
            result.answeredV4 = result.answeredV4 || retry.answeredV4;
            result.answeredV6 = result.answeredV6 || retry.answeredV6;
        }
    }

    ScanWorker::ChunkResult ScanWorker::runChunk(const libcore::ScanTargetSpec &spec, quint64 chunkStart, int maxTargets,
                                                 quint64 *nextCursor) {
        ChunkResult result;
        setStage(probePhaseEnabled() ? Stage::Probing : Stage::TestingConfigs);

        libcore::ScanProbeRequest request;
        request.session_id = sessionId_.toStdString();
        request.spec = spec;
        request.cursor = chunkStart;
        request.max_targets = maxTargets;
        fillProbeRequest(request);

        bool ok = false;
        QString coreError;
        const auto response = API::defaultClient->ScanProbe(&ok, request, &coreError, probeTimeoutMs(maxTargets));

        QList<ProbeOutcome> candidates;
        for (const auto &item : response.results) {
            ProbeOutcome outcome;
            outcome.address = scanWorkerText(item.address.value());
            outcome.port = item.port.value();
            outcome.probePort = item.probe_port.value();
            outcome.latencyMs = resolverPhase_ ? item.dns_ms.value() : scanWorkerProbeLatency(item, scan_.config);
            outcome.maxQName = item.max_qname.value();
            outcome.uploadMtu = item.upload_mtu.value();
            outcome.downloadMtu = item.download_mtu.value();
            if (item.dns_ms.value() > 0) {
                if (outcome.address.contains(QLatin1Char(':'))) result.answeredV6 = true;
                else result.answeredV4 = true;
            }
            if (item.passed.value()) candidates << outcome;
            else if (item.local_failure.value()) result.localFailures << scanWorkerEntry(outcome.address, outcome.port, 0);
            else result.failures << scanWorkerEntry(outcome.address, outcome.port, 0);
        }
        const auto addProbeHits = [&] {
            for (const auto &candidate : candidates) {
                addHit(result, candidate, candidate.probePort > 0 ? candidate.probePort : candidate.port, candidate.latencyMs);
            }
        };

        if (!ok || !response.error.value().empty()) {
            // The positions of a call cut short stay unfinished, so its failures prove nothing.
            result.failures.clear();
            result.localFailures.clear();
            if (!configPhase_) addProbeHits();
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

        if (!configPhase_) {
            addProbeHits();
            return result;
        }
        if (candidates.isEmpty()) return result;
        if (result.aborted || stop_) {
            result.aborted = true;
            return result;
        }
        result.error = testConfigs(candidates, result);
        return result;
    }

    QString ScanWorker::testConfigs(const QList<ProbeOutcome> &candidates, ChunkResult &result) {
        setStage(Stage::TestingConfigs);
        pushLog(ScanManager::tr("Testing %n candidate(s) through the config…", nullptr, static_cast<int>(candidates.size())));
        const auto test = scanWorkerConfigTest(scan_);

        for (qsizetype offset = 0; offset < candidates.size(); offset += test.subBatch) {
            if (stop_) {
                result.aborted = true;
                return {};
            }
            const auto batch = candidates.mid(offset, test.subBatch);
            QList<Configs::ScanTestTarget> targets;
            targets.reserve(batch.size());
            for (const auto &candidate : batch) targets << Configs::ScanTestTarget{candidate.address, candidate.port};

            const auto build = Configs::BuildScanTestConfig(base_, targets);
            if (!build.error.isEmpty()) return build.error;
            const auto configPort = [this](const ProbeOutcome &candidate) {
                return candidate.port > 0 ? candidate.port : basePort_;
            };
            for (const int index : build.unsupported) {
                if (index >= 0 && index < batch.size()) result.failures << scanWorkerEntry(batch[index].address, batch[index].port, 0);
            }
            if (build.build == nullptr || build.build->outboundTags.isEmpty()) continue;

            libcore::ScanURLTestRequest request;
            request.session_id = sessionId_.toStdString();
            libcore::TestReq req;
            for (const auto &tag : build.build->outboundTags) {
                req.outbound_tags.push_back(tag.toStdString());
                const int index = build.tag2target.value(tag, -1);
                const auto label = index >= 0 && index < batch.size() ? FormatTarget(batch[index].address, batch[index].port) : tag;
                request.target_labels.push_back(label.toStdString());
            }
            req.config = QJsonObject2QString(build.build->coreConfig, false).toStdString();
            req.use_default_outbound = false;
            if (build.build->isXrayNeeded) req.xray_config = QJsonObject2QString(build.build->xrayConfig, false).toStdString();
            req.need_xray = !req.xray_config.value().empty();
            for (const auto &xrayConfig : build.build->xrayFullConfigs) req.xray_full_configs.push_back(xrayConfig.toStdString());
            req.xray_outbound_dns_strategy = build.build->xrayDnsStrategy.toStdString();
            req.url = test.url.toStdString();
            req.max_concurrency = test.concurrency;
            req.test_timeout_ms = test.timeoutMs;
            for (const auto &tag : build.vpnEndpointTags) req.vpn_endpoint_tags.push_back(tag.toStdString());
            request.test = std::move(req);
            request.warm_latency = test.warmLatency;

            bool ok = false;
            QString coreError;
            const auto response = API::defaultClient->ScanURLTest(
                &ok, request, &coreError,
                scanWorkerUrlTestTimeoutMs(build.build->outboundTags.size(), test, Configs::DnsTunnelStartupAllowanceMs(base_)));
            if (!ok) {
                if (scanWorkerIsStopped(coreError)) stop_.store(true);
                if (stop_) {
                    result.aborted = true;
                    return {};
                }
                return coreError.isEmpty() ? ScanManager::tr("The core did not answer the config test") : coreError;
            }

            QSet<QString> tunnelsUp;
            for (const auto &status : response.vpn_status) {
                if (status.connected.value()) tunnelsUp.insert(scanWorkerText(status.tag.value()));
            }
            QSet<int> reported;
            for (const auto &item : response.results) {
                const auto tag = scanWorkerText(item.outbound_tag.value());
                const int index = build.tag2target.value(tag, -1);
                if (index < 0 || index >= batch.size() || reported.contains(index)) continue;
                reported.insert(index);
                const auto &candidate = batch[index];
                const auto error = scanWorkerText(item.error.value());
                if (error.isEmpty()) {
                    addHit(result, candidate, configPort(candidate), item.latency_ms.value());
                } else if (scanWorkerIsAborted(error)) {
                    result.aborted = true;
                } else if (tunnelsUp.contains(tag)) {
                    // The tunnel came up, so the server answers; only the probe URL failed behind it.
                    addHit(result, candidate, configPort(candidate), 0);
                } else {
                    result.failures << scanWorkerEntry(candidate.address, candidate.port, 0);
                }
            }
            for (auto it = build.tag2target.cbegin(); it != build.tag2target.cend(); ++it) {
                if (!reported.contains(it.value())) result.aborted = true;
            }
        }
        return {};
    }

    void ScanWorker::addHit(ChunkResult &result, const ProbeOutcome &candidate, int port, int latencyMs) const {
        auto hit = scanWorkerEntry(candidate.address, port, latencyMs);
        hit.maxQName = candidate.maxQName;
        hit.uploadMtu = candidate.uploadMtu;
        hit.downloadMtu = candidate.downloadMtu;
        result.hits << hit;
        // A hit under another port must replace the stored rescan row, not add a sibling.
        if (rescanPass() && port != candidate.port) result.replaced << scanWorkerEntry(candidate.address, candidate.port, 0);
    }

    bool ScanWorker::networkUp(const QStringList &targets, QString *detail) {
        setStage(Stage::CheckingNetwork);
        pushLog(ScanManager::tr("Checking the network…"));
        bool ok = false;
        const bool up = API::defaultClient->ScanCheckNetwork(&ok, targets, kScanWorkerNetworkCheckTimeoutMs, detail);
        return ok && up;
    }

    void ScanWorker::finishPass(bool completed, const QString &error) {
        using Status = Configs::IpScan::Status;
        if (completed) {
            if (rescanPass()) {
                deleteList(scan_.rescan_snapshot_list_id);
                scan_.rescan_snapshot_list_id = -1;
                if (scan_.CanResumeInitial()) {
                    scan_.mode = Configs::IpScan::Mode::Initial;
                    scan_.status = Status::Paused;
                } else {
                    scan_.status = Status::Completed;
                    scan_.finished_at = QDateTime::currentSecsSinceEpoch();
                }
            } else {
                deleteList(scan_.snapshot_list_id);
                scan_.snapshot_list_id = -1;
                scan_.status = Status::Completed;
                scan_.finished_at = QDateTime::currentSecsSinceEpoch();
            }
            scan_.last_error.clear();
        } else {
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
        Configs::dataManager->ipScansRepo->SaveProgress(scanId_, scan_.mode, scan_.ActiveCursor(), scan_.ActiveTotal(),
                                                        scan_.found, scan_.removed);
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
        const auto &config = scan_.config;
        // One port per target for every phase, so the stored hit is the port a rescan probes again.
        const int fallbackPort = config.http.enabled && !config.http.tls ? kScanWorkerPlainHttpPort : kScanWorkerFallbackPort;
        request.concurrency = config.concurrency;
        request.spawn_interval_ms = config.spawnIntervalMs;

        libcore::ScanIcmpOptions icmp;
        icmp.enabled = config.icmp.enabled;
        icmp.timeout_ms = config.icmp.timeoutMs;
        icmp.count = config.icmp.count;
        request.icmp = icmp;

        libcore::ScanTcpOptions tcp;
        tcp.enabled = config.tcp.enabled;
        tcp.timeout_ms = config.tcp.timeoutMs;
        tcp.attempts = config.tcp.attempts;
        tcp.fallback_port = fallbackPort;
        request.tcp = tcp;

        const auto &h = config.http;
        libcore::ScanHttpOptions http;
        http.enabled = h.enabled;
        http.tls = h.tls;
        http.server_name = h.serverName.toStdString();
        http.host = h.host.toStdString();
        http.path = h.path.toStdString();
        http.method = h.method.toStdString();
        http.http_version = h.httpVersion.toStdString();
        for (const auto &alpn : h.alpn) http.alpn.push_back(alpn.toStdString());
        http.min_version = h.minVersion.toStdString();
        http.max_version = h.maxVersion.toStdString();
        http.fingerprint = h.fingerprint.toStdString();
        http.insecure = h.insecure;
        http.disable_sni = h.disableSni;
        http.fragment = h.fragment;
        http.fragment_fallback_delay_ms = h.fragmentFallbackDelayMs;
        http.record_fragment = h.recordFragment;
        http.mixed_case_sni = h.mixedCaseSni;
        http.timeout_ms = h.timeoutMs;
        http.fallback_port = fallbackPort;
        request.http = std::move(http);
        if (resolverPhase_) request.dns = dns_;
    }

    int ScanWorker::probeTimeoutMs(int chunkSize) const {
        const auto &config = scan_.config;
        qint64 perTarget = 0;
        if (config.icmp.enabled) perTarget += qint64(config.icmp.count) * config.icmp.timeoutMs;
        if (config.tcp.enabled) perTarget += qint64(config.tcp.attempts) * config.tcp.timeoutMs;
        if (config.http.enabled) perTarget += 2LL * config.http.timeoutMs;
        if (resolverPhase_) perTarget += dns_.timeout_ms.value();
        perTarget = std::max<qint64>(perTarget, 1000);
        const qint64 concurrency = std::max(1, config.concurrency);
        const qint64 rounds = (std::max(chunkSize, 1) + concurrency - 1) / concurrency;
        return scanWorkerClampTimeout(rounds * perTarget + qint64(chunkSize) * std::max(0, config.spawnIntervalMs) +
                                      kScanWorkerRpcSlackMs);
    }

    bool ScanWorker::probePhaseEnabled() const {
        const auto &config = scan_.config;
        return resolverPhase_ || config.icmp.enabled || config.tcp.enabled || config.http.enabled;
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
            lastProbePassed_ = std::max<qint64>(lastProbePassed_, response.probe_passed.value());
            lastUrlTested_ = std::max<qint64>(lastUrlTested_, response.url_tested.value());
            for (const auto &line : lines) live_.log << line;
            while (live_.log.size() > ScanManager::kLogLines) live_.log.removeFirst();
            if (chunkActive_) {
                const qint64 finished = configPhase_
                    ? (lastProbed_ - lastProbePassed_ - baseProbeFailed_) + (lastUrlTested_ - baseUrlTested_)
                    : lastProbed_ - baseProbed_;
                const quint64 inChunk = static_cast<quint64>(std::clamp<qint64>(finished, 0, qint64(chunkSize_)));
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
        if (ok) {
            lastProbed_ = counters.probed.value();
            lastProbePassed_ = counters.probe_passed.value();
            lastUrlTested_ = counters.url_tested.value();
        }
        baseProbed_ = lastProbed_;
        baseProbeFailed_ = lastProbed_ - lastProbePassed_;
        baseUrlTested_ = lastUrlTested_;
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
            case Stage::TestingConfigs:
                text = ScanManager::tr("Testing configs");
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
