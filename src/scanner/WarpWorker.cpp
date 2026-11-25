#include "include/scanner/WarpWorker.h"

#include <QDateTime>
#include <QHash>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QThread>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

#ifndef Q_MOC_RUN
#include <core/gen/libcore.pb.h>
#endif

#include "include/api/RPC.h"
#include "include/configs/common/DnsTunnel.h"
#include "include/configs/generate.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include "include/scanner/IpListParse.h"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanProfiles.h"
#include "include/scanner/ScanWorker.h"
#include "include/scanner/WarpPresets.h"

namespace Scanner {
    namespace {
        // WireGuard probes in flight; discovery mostly waits on silent ports, so it sends more at once.
        constexpr int kWarpWorkerConcurrency = 32;
        constexpr int kWarpWorkerDiscoveryConcurrency = 48;
        // More MASQUE TLS handshakes at once lose answers.
        constexpr int kWarpWorkerMasqueConcurrency = 16;
        // Small calls, so the stop rule sees results as they arrive.
        constexpr int kWarpWorkerChunk = 32;
        constexpr int kWarpWorkerSpawnIntervalMs = 2;
        constexpr int kWarpWorkerHandshakeTimeoutMs = 1000;
        constexpr int kWarpWorkerDiscoveryAttempts = 1;
        constexpr int kWarpWorkerAttempts = 3;
        constexpr int kWarpWorkerDiscoveryAddresses = 12;
        constexpr int kWarpWorkerFallbackAddresses = 4;
        // The core dials MASQUE on 443 when the profile has no port.
        constexpr int kWarpWorkerMasqueDefaultPort = 443;
        constexpr int kWarpWorkerPerPrefix = 16;
        // A batch lasts as long as its slowest tunnel, and a failing one takes the whole data check bound.
        constexpr int kWarpWorkerDataBatch = 4;
        constexpr int kWarpWorkerDataTimeoutMs = 3000;
        constexpr int kWarpWorkerStopAfter = 5;
        constexpr qint64 kWarpWorkerQuietMs = 10000;
        // A search's total; its cursor counts the seconds spent.
        constexpr quint64 kWarpWorkerBudgetSeconds = 90;
        constexpr qint64 kWarpWorkerBudgetMs = qint64(kWarpWorkerBudgetSeconds) * 1000;
        constexpr int kWarpWorkerTickMs = 500;
        constexpr int kWarpWorkerKnownGoodCap = 64;
        constexpr int kWarpWorkerMaxPools = 256;
        constexpr int kWarpWorkerNetworkCheckTimeoutMs = 3000;
        constexpr qint64 kWarpWorkerNetworkReuseMs = 10000;
        // Mirror the core's TunnelHandshakeTimeout and TunnelStartupTimeout.
        constexpr qint64 kWarpWorkerTunnelHandshakeMs = 10000;
        constexpr qint64 kWarpWorkerTunnelStartupMs = 5000;
        constexpr qint64 kWarpWorkerRpcSlackMs = 30000;

        const QStringList kWarpWorkerNetworkCheckTargets = {
            QStringLiteral("1.1.1.1:443"),
            QStringLiteral("8.8.8.8:443"),
            QStringLiteral("9.9.9.9:443"),
            QStringLiteral("223.5.5.5:443"),
            QStringLiteral("77.88.8.8:443"),
        };

        const QStringList kWarpWorkerNetworkCheckTargetsV6 = {
            QStringLiteral("[2606:4700:4700::1111]:443"),
            QStringLiteral("[2001:4860:4860::8888]:443"),
            QStringLiteral("[2620:fe::fe]:443"),
        };

        std::atomic<quint32> warpWorkerSessionCounter{0};

        QString warpWorkerText(const std::string &text) {
            return QString::fromStdString(text);
        }

        int warpWorkerClampTimeout(qint64 ms) {
            return static_cast<int>(std::min<qint64>(ms, std::numeric_limits<int>::max()));
        }

        qint64 warpWorkerNowMs() {
            using namespace std::chrono;
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }

        bool warpWorkerIsStopped(const QString &error) {
            return error.contains(QStringLiteral("scan stopped"));
        }

        bool warpWorkerIsAborted(const QString &error) {
            return error.contains(QStringLiteral("test aborted")) || error.contains(QStringLiteral("context canceled"));
        }

        bool warpWorkerIsIpv6(const QString &address) {
            return address.contains(QLatin1Char(':'));
        }

        QString warpWorkerKey(const QString &host, int port) {
            return host + QLatin1Char(' ') + QString::number(port);
        }

        // The canonical host the result lists store; "" for a range or anything else.
        QString warpWorkerHost(const QString &text) {
            const auto canonical = NormalizeCidr(text.trimmed());
            if (canonical.isEmpty() || !IsHostCidr(canonical)) return {};
            const auto slash = canonical.indexOf(QLatin1Char('/'));
            return slash < 0 ? canonical : canonical.left(slash);
        }

        Q_IPV6ADDR warpWorkerMaskV6(Q_IPV6ADDR bytes, int prefix) {
            for (int i = 0; i < 16; ++i) {
                const int keep = std::clamp(prefix - i * 8, 0, 8);
                bytes[i] = quint8(bytes[i] & quint8(0xFF << (8 - keep)));
            }
            return bytes;
        }

        std::shared_ptr<Configs::Profile> warpWorkerCopy(const std::shared_ptr<Configs::Profile> &base) {
            auto copy = Configs::ProfilesRepo::NewProfile(base->type);
            if (copy->outbound == nullptr || !copy->outbound->ParseFromJson(base->outbound->ExportToJson())) return nullptr;
            copy->id = base->id;
            copy->gid = base->gid;
            copy->name = base->name;
            copy->outbound->profile_id = base->outbound->profile_id;
            return copy;
        }

        std::shared_ptr<Configs::Profile> warpWorkerJunkCopy(const std::shared_ptr<Configs::Profile> &base) {
            auto copy = warpWorkerCopy(base);
            auto *wireguard = copy != nullptr ? copy->Wireguard() : nullptr;
            if (wireguard == nullptr) return nullptr;
            ApplyWarpJunk(*wireguard);
            return copy;
        }

        // MASQUE with http_version set to version; only 3 turns the HTTP/2 fallback off.
        std::shared_ptr<Configs::Profile> warpWorkerHttpCopy(const std::shared_ptr<Configs::Profile> &base, int version) {
            auto copy = warpWorkerCopy(base);
            auto *masque = copy != nullptr ? copy->Masque() : nullptr;
            if (masque == nullptr) return nullptr;
            masque->http_version = version;
            masque->disable_version_fallback = version == 3;
            return copy;
        }

        // "" when the profile cannot be built.
        QString warpWorkerProbeJson(const std::shared_ptr<Configs::Profile> &profile) {
            if (profile == nullptr) return {};
            QString error;
            const auto endpoint = WarpProbeEndpoint(*profile, &error);
            return endpoint.isEmpty() ? QString() : QJsonObject2QString(endpoint, true);
        }

        Configs::IpScan warpWorkerFinderScan(int id) {
            Configs::IpScan scan;
            scan.id = id;
            scan.total = kWarpWorkerBudgetSeconds;
            return scan;
        }
    } // namespace

    WarpWorker::WarpWorker(const Configs::IpScan &scan, StartMode mode, std::shared_ptr<Configs::Profile> base, ScanRunHooks hooks)
        : scanId_(scan.id),
          sessionId_(QStringLiteral("warp-%1-%2-%3")
                         .arg(scan.id)
                         .arg(QDateTime::currentMSecsSinceEpoch())
                         .arg(warpWorkerSessionCounter.fetch_add(1))),
          startMode_(mode),
          hooks_(std::move(hooks)),
          initialStatus_(scan.status),
          scan_(scan),
          base_(std::move(base)) {
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

    WarpWorker::WarpWorker(int finderId, std::shared_ptr<Configs::Profile> base, QList<Configs::IpListEntry> seeds, ScanRunHooks hooks)
        : WarpWorker(warpWorkerFinderScan(finderId), StartMode::FromInitial, std::move(base), std::move(hooks)) {
        finder_ = true;
        seeds_ = std::move(seeds);
        persist_.store(false);
        live_.total = scan_.total;
    }

    void WarpWorker::Start() {
        const auto self = shared_from_this();
        runOnNewThread([self] { self->run(); });
        runOnNewThread([self] { self->tickLoop(); });
    }

    void WarpWorker::RequestStop() {
        stop_.store(true);
        QMutexLocker locker(&mutex_);
        live_.stopping = true;
    }

    void WarpWorker::DisablePersist() {
        persist_.store(false);
    }

    ScanLiveState WarpWorker::LiveState() const {
        QMutexLocker locker(&mutex_);
        auto state = live_;
        state.stopping = stop_.load();
        if (searching_.load()) state.tested = budgetCursor();
        return state;
    }

    void WarpWorker::run() {
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
            finishRun(completed, error);
        } catch (...) {
            mainLog(ScanManager::tr("Scan %1: saving its final state failed").arg(scanId_));
        }
        tickStop_.store(true);
        if (hooks_.finished) hooks_.finished();
        bool ok = false;
        API::defaultClient->StopScan(&ok, sessionId_);
    }

    // The budget runs down inside long probe calls too, so a ticker keeps the progress moving.
    void WarpWorker::tickLoop() {
        quint64 shown = std::numeric_limits<quint64>::max();
        while (!tickStop_.load()) {
            QThread::msleep(kWarpWorkerTickMs);
            if (tickStop_.load() || !searching_.load()) continue;
            if (const auto cursor = budgetCursor(); cursor != shown) {
                shown = cursor;
                notifyProgress();
            }
        }
    }

    QString WarpWorker::execute(bool &completed) {
        clockStartMs_ = warpWorkerNowMs();
        if (auto error = prepare(); !error.isEmpty() || stop_) return error;
        if (!finder_) {
            const auto error = preparePass();
            if (std::exchange(listsTouched_, false)) IpListUpdater::instance()->NotifyListsChanged();
            if (!error.isEmpty() || stop_ || !persist_) return error;
            if (rescanPass()) return rescan(completed);
        }
        if (auto error = loadPools(); !error.isEmpty() || stop_) return error;

        // The budget starts here, so the IPv6 check costs none of it.
        budgetBaseMs_ = qint64(std::min(scan_.cursor, kWarpWorkerBudgetSeconds)) * 1000;
        clockStartMs_ = warpWorkerNowMs();
        searching_ = true;
        setTested(budgetCursor(), scan_.total);
        {
            const auto endSearch = qScopeGuard([this] {
                scan_.cursor = budgetCursor();
                searchMs_ = budgetBaseMs_ + elapsedMs();
                searching_ = false;
            });
            if (auto error = search(); !error.isEmpty() || stop_) return error;
        }
        logOutcome();
        completed = true;
        return {};
    }

    QString WarpWorker::prepare() {
        setStage(ScanManager::tr("Preparing"));
        pushLog(ScanManager::tr("Checking the WARP profile"));
        if (base_ == nullptr) return ScanManager::tr("The scan has no WARP profile");
        carrier_ = WarpCarrier(*base_);
        QString error;
        const auto endpoint = WarpProbeEndpoint(*base_, &error);
        if (endpoint.isEmpty()) return error.isEmpty() ? ScanManager::tr("The WARP profile could not be built") : error;
        probeJson_ = QJsonObject2QString(endpoint, true);
        if (auto invalid = Configs::ValidateScanBase(base_); !invalid.isEmpty()) return invalid;
        testUrl_ = Configs::dataManager->settingsRepo->test_latency_url;
        return {};
    }

    QString WarpWorker::preparePass() {
        if (!persist_ || stop_) return {};
        const int previous = scan_.result_list_id;
        if (EnsureScanResultList(scan_) < 0) return ScanManager::tr("The result list could not be created");
        if (scan_.result_list_id != previous) listsTouched_ = true;
        switch (startMode_) {
            case StartMode::FromInitial:
                return prepareSearch();
            case StartMode::RescanResults:
                return prepareRescan();
            case StartMode::Resume:
                break;
        }
        if (!rescanResumable()) {
            // The old flow counted targets, not seconds, so such a pass starts over.
            if (!scan_.CanResumeInitial() || scan_.total != kWarpWorkerBudgetSeconds) return prepareSearch();
            scan_.mode = Configs::IpScan::Mode::Initial;
            replacePending_ = ReplacePending(scan_);
        }
        scan_.found = Configs::dataManager->ipListsRepo->EntryCount(scan_.result_list_id);
        {
            QMutexLocker locker(&mutex_);
            live_.found = scan_.found;
        }
        persistState();
        return {};
    }

    QString WarpWorker::prepareSearch() {
        auto &lists = *Configs::dataManager->ipListsRepo;
        const auto base = lists.GetIpList(scan_.base_list_id);
        if (base == nullptr) return ScanManager::tr("The base IP list no longer exists");
        if (base->entries.isEmpty()) return ScanManager::tr("The base IP list is empty");
        if (buildPools(base->entries).isEmpty()) return ScanManager::tr("The base IP list holds no valid range");
        if (!persist_ || stop_) return {};

        // The old results stay until the pass's first confirmed endpoint replaces them.
        replacePending_ = true;
        deleteList(scan_.snapshot_list_id);
        deleteList(scan_.rescan_snapshot_list_id);
        scan_.snapshot_list_id = -1;
        scan_.rescan_snapshot_list_id = -1;
        scan_.rescan_cursor = 0;
        scan_.rescan_total = 0;
        scan_.mode = Configs::IpScan::Mode::Initial;
        scan_.seed = 0;
        scan_.cursor = 0;
        scan_.total = 0;
        scan_.found = lists.EntryCount(scan_.result_list_id);
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
        if (!lists.AddIpList(snapshot)) return ScanManager::tr("The base IP list could not be copied");
        listsTouched_ = true;
        scan_.snapshot_list_id = snapshot->id;
        scan_.spec_hash = scan_.config.TargetSpecHash(scan_.base_list_id);
        scan_.total = kWarpWorkerBudgetSeconds;
        persistState();
        setTested(0, scan_.total);
        return {};
    }

    QString WarpWorker::prepareRescan() {
        auto &lists = *Configs::dataManager->ipListsRepo;
        deleteList(scan_.rescan_snapshot_list_id);
        scan_.rescan_snapshot_list_id = -1;
        scan_.rescan_cursor = 0;
        scan_.rescan_total = 0;
        const auto entries = lists.GetEntries(scan_.result_list_id);
        if (entries.isEmpty()) {
            persistState();
            return ScanManager::tr("The result list is empty");
        }
        auto snapshot = Configs::IpListsRepo::NewIpList();
        snapshot->name = ScanManager::tr("%1 rescan snapshot").arg(scan_.name);
        snapshot->role = Configs::IpList::Role::ScanSnapshot;
        snapshot->related_test_id = scan_.id;
        snapshot->entries = entries;
        snapshot->entryCount = static_cast<int>(entries.size());
        snapshot->entriesLoaded = true;
        if (!lists.AddIpList(snapshot)) return ScanManager::tr("The result list could not be copied");
        listsTouched_ = true;
        scan_.rescan_snapshot_list_id = snapshot->id;
        scan_.rescan_total = static_cast<quint64>(entries.size());
        scan_.mode = Configs::IpScan::Mode::RescanResult;
        scan_.found = lists.EntryCount(scan_.result_list_id);
        scan_.removed = 0;
        scan_.last_error.clear();
        {
            QMutexLocker locker(&mutex_);
            live_.found = scan_.found;
            live_.removed = 0;
        }
        persistState();
        setTested(0, scan_.rescan_total);
        return {};
    }

    QList<WarpWorker::Pool> WarpWorker::buildPools(const QList<Configs::IpListEntry> &entries) const {
        const bool masque = carrier_ == QLatin1String("masque");
        auto *rng = QRandomGenerator::global();
        QList<Pool> pools;
        for (const auto &entry : entries) {
            auto cidr = entry.cidr.trimmed();
            if (!cidr.contains(QLatin1Char('/'))) cidr += warpWorkerIsIpv6(cidr) ? QStringLiteral("/128") : QStringLiteral("/32");
            const auto subnet = QHostAddress::parseSubnet(cidr);
            if (subnet.first.isNull() || subnet.second < 0) continue;
            const int prefix = subnet.second;
            const int port = entry.port > 0 && entry.port <= 65535 ? entry.port : 0;
            if (subnet.first.protocol() == QAbstractSocket::IPv6Protocol) {
                Pool pool;
                pool.v6 = true;
                pool.v6Net = warpWorkerMaskV6(subnet.first.toIPv6Address(), prefix);
                pool.v6Prefix = prefix;
                pool.port = port;
                pools << pool;
                continue;
            }
            const quint32 mask = prefix == 0 ? 0 : ~quint32(0) << (32 - prefix);
            const quint32 network = subnet.first.toIPv4Address() & mask;
            if (prefix > 24) {
                const quint32 size = quint32(1) << (32 - prefix);
                Pool pool;
                pool.port = port;
                pool.v4First = size > 2 ? network + 1 : network;
                pool.v4Count = size > 2 ? size - 2 : size;
                pools << pool;
                continue;
            }
            // Wider prefixes split into /24s; a very wide one gives a random subset.
            const quint32 blocks = quint32(1) << (24 - prefix);
            QList<quint32> picked;
            if (blocks <= quint32(kWarpWorkerMaxPools)) {
                for (quint32 i = 0; i < blocks; ++i) picked << i;
            } else {
                QSet<quint32> chosen;
                while (chosen.size() < kWarpWorkerMaxPools) chosen.insert(rng->bounded(blocks));
                picked = chosen.values();
                std::sort(picked.begin(), picked.end());
            }
            for (const quint32 block : picked) {
                Pool pool;
                pool.port = port;
                // .1 of a MASQUE /24 is a plain CDN edge.
                pool.v4First = network + (block << 8) + (masque ? 2 : 1);
                pool.v4Count = masque ? 253 : 254;
                pools << pool;
            }
        }
        if (pools.size() > kWarpWorkerMaxPools) {
            QList<qsizetype> indexes(pools.size());
            std::iota(indexes.begin(), indexes.end(), qsizetype(0));
            std::shuffle(indexes.begin(), indexes.end(), *rng);
            indexes.resize(kWarpWorkerMaxPools);
            std::sort(indexes.begin(), indexes.end());
            QList<Pool> kept;
            for (const auto index : indexes) kept << pools[index];
            pools = kept;
        }
        return pools;
    }

    QString WarpWorker::loadPools() {
        const auto entries = finder_ ? seeds_ : Configs::dataManager->ipListsRepo->GetEntries(scan_.snapshot_list_id);
        if (entries.isEmpty()) {
            if (finder_) return ScanManager::tr("The base IP list is empty");
            scan_.snapshot_list_id = -1;
            return ScanManager::tr("The copy of the base IP list is gone; restart the scan");
        }
        pools_ = buildPools(entries);
        if (pools_.isEmpty()) {
            deleteList(scan_.snapshot_list_id);
            scan_.snapshot_list_id = -1;
            return ScanManager::tr("The base IP list holds no valid range");
        }

        const bool hasV4 = std::any_of(pools_.cbegin(), pools_.cend(), [](const Pool &pool) { return !pool.v6; });
        const bool hasV6 = std::any_of(pools_.cbegin(), pools_.cend(), [](const Pool &pool) { return pool.v6; });
        if (hasV6) {
            QString detail;
            if (!networkUp(true, &detail)) {
                if (stop_) return {};
                if (!hasV4) return ScanManager::tr("The base IP list holds only IPv6 ranges, and this network has no IPv6");
                dropIpv6(ScanManager::tr("This network has no IPv6; skipping the IPv6 ranges"));
            }
        }
        return {};
    }

    QString WarpWorker::search() {
        // A finder skips the known-endpoints pass: the profile's own endpoint joins round 1, checked with the settings discovery settled.
        if (!finder_) {
            if (auto error = knownGood(); !error.isEmpty() || stop_ || stopRuleMet()) return error;
        }
        QList<Target> seeds;
        if (auto error = discoverPorts(seeds); !error.isEmpty() || stop_) return error;
        if (finder_) {
            const auto own = warpWorkerHost(base_->outbound->GetAddress());
            int port = base_->outbound->GetPort().toInt();
            if (port <= 0 && isMasque()) port = kWarpWorkerMasqueDefaultPort;
            const bool seeded = std::any_of(seeds.cbegin(), seeds.cend(), [&own](const Target &target) { return target.address == own; });
            if (!own.isEmpty() && port > 0 && !seeded) seeds.prepend(Target{own, port});
        }
        return sampleRounds(seeds);
    }

    QString WarpWorker::knownGood() {
        QList<Target> targets;
        QSet<QString> seen;
        const auto add = [&](const QString &host, int port) {
            if (host.isEmpty() || targets.size() >= kWarpWorkerKnownGoodCap) return;
            if (!ipv6Allowed_ && warpWorkerIsIpv6(host)) return;
            const auto key = warpWorkerKey(host, port);
            if (seen.contains(key)) return;
            seen.insert(key);
            targets << Target{host, port};
        };
        add(warpWorkerHost(base_->outbound->GetAddress()), base_->outbound->GetPort().toInt());
        if (scan_.result_list_id >= 0) {
            for (const auto &entry : Configs::dataManager->ipListsRepo->GetEntries(scan_.result_list_id, 0, kWarpWorkerKnownGoodCap))
                add(warpWorkerHost(entry.cidr), entry.port);
        }
        if (targets.isEmpty()) return {};
        setStage(ScanManager::tr("Re-checking known endpoints"));
        pushLog(ScanManager::tr("Re-checking %n known endpoint(s)", nullptr, static_cast<int>(targets.size())));
        return probeTargets(targets, kWarpWorkerAttempts, true, nullptr, nullptr);
    }

    QString WarpWorker::discoverPorts(QList<Target> &seeds) {
        const bool masque = isMasque();
        const auto &primary = masque ? WarpPresets::kMasquePorts : WarpPresets::kWireGuardPorts;
        // .1 of each /24 answers WireGuard; for MASQUE it is a plain CDN edge.
        const auto addresses = discoveryAddresses(kWarpWorkerDiscoveryAddresses, !masque);
        if (addresses.isEmpty()) return {};
        const auto cross = [](const QStringList &hosts, const QList<int> &ports) {
            QList<Target> targets;
            for (const auto &host : hosts) {
                for (const int port : ports) targets << Target{host, port};
            }
            return targets;
        };

        setStage(ScanManager::tr("Finding ports"));
        pushLog(ScanManager::tr("Finding open ports on %n address(es)", nullptr, static_cast<int>(addresses.size())));
        const auto usual = cross(addresses, primary);
        QList<Candidate> found;
        if (auto error = probeTargets(usual, kWarpWorkerDiscoveryAttempts, false, &found, nullptr); !error.isEmpty() || stop_)
            return error;

        if (masque) {
            tryHttp2(usual, found);
            if (stop_) return {};
        } else {
            // Junk on the usual ports comes before the rarer ports: where plain WireGuard is blocked, it answers within seconds.
            if (auto error = tryJunk(usual, found); !error.isEmpty() || stop_) return error;
            if (found.isEmpty() && !stopRuleMet()) {
                QList<int> extra;
                for (const int port : WarpPresets::kWireGuardAllPorts) {
                    if (!primary.contains(port)) extra << port;
                }
                const auto rest = cross(addresses.mid(0, kWarpWorkerFallbackAddresses), extra);
                pushLog(ScanManager::tr("None of the usual ports answered; trying %n more", nullptr, static_cast<int>(extra.size())));
                if (auto error = probeTargets(rest, kWarpWorkerDiscoveryAttempts, false, &found, nullptr); !error.isEmpty() || stop_)
                    return error;
                if (auto error = tryJunk(rest, found); !error.isEmpty() || stop_) return error;
            }
        }
        if (found.isEmpty()) return {};

        QHash<int, int> answers;
        for (const auto &candidate : found) answers[candidate.port] += 1;
        const auto rank = [&primary](int port) {
            const auto index = primary.indexOf(port);
            return index >= 0 ? index : primary.size() + WarpPresets::kWireGuardAllPorts.indexOf(port);
        };
        ports_ = answers.keys();
        std::sort(ports_.begin(), ports_.end(), [&](int a, int b) {
            if (answers.value(a) != answers.value(b)) return answers.value(a) > answers.value(b);
            return rank(a) < rank(b);
        });
        QStringList portTexts;
        for (const int port : ports_) portTexts << QString::number(port);
        pushLog(ScanManager::tr("Ports answering: %1").arg(portTexts.join(QStringLiteral(", "))));

        // Each address that answered opens round 1, on its best port.
        QSet<QString> seeded;
        for (const int port : ports_) {
            for (const auto &candidate : found) {
                if (candidate.port != port || seeded.contains(candidate.address)) continue;
                seeded.insert(candidate.address);
                seeds << Target{candidate.address, port};
            }
        }
        return {};
    }

    // Only for a profile without AmneziaWG of its own: an answer to Throne's junk switches the run to it.
    QString WarpWorker::tryJunk(const QList<Target> &targets, QList<Candidate> &found) {
        const auto *wireguard = base_->Wireguard();
        if (!found.isEmpty() || wireguard == nullptr || wireguard->enable_amnezia || stopRuleMet()) return {};
        const auto junk = warpWorkerJunkCopy(base_);
        const auto json = warpWorkerProbeJson(junk);
        if (json.isEmpty()) return {};
        pushLog(ScanManager::tr("Trying AmneziaWG junk"));
        const auto plain = std::exchange(probeJson_, json);
        if (auto error = probeTargets(targets, kWarpWorkerDiscoveryAttempts, false, &found, nullptr); !error.isEmpty() || stop_)
            return error;
        if (found.isEmpty()) {
            probeJson_ = plain;
            return {};
        }
        base_ = junk;
        junkMode_ = true;
        {
            QMutexLocker locker(&mutex_);
            live_.warpJunk = true;
        }
        const auto line = ScanManager::tr("Endpoints answer only with AmneziaWG junk; continuing with junk packets");
        pushLog(line);
        mainLog(ScanManager::tr("Scan %1: %2").arg(scan_.name, line));
        return {};
    }

    // An HTTP/3-only profile nothing answered: with the fallback on, HTTP/2 over TCP may get through where QUIC is blocked.
    void WarpWorker::tryHttp2(const QList<Target> &targets, QList<Candidate> &found) {
        const auto *masque = base_->Masque();
        if (!found.isEmpty() || masque == nullptr || masque->http_version == 2 || !masque->disable_version_fallback || stopRuleMet())
            return;
        const auto copy = warpWorkerHttpCopy(base_, 0);
        const auto json = warpWorkerProbeJson(copy);
        if (json.isEmpty()) return;
        pushLog(ScanManager::tr("Trying HTTP/2"));
        const auto http3 = std::exchange(probeJson_, json);
        const auto error = probeTargets(targets, kWarpWorkerDiscoveryAttempts, false, &found, nullptr);
        if (stop_) return;
        // A copy that cannot probe here (its ECH fetch needs a running instance) leaves the profile's own outcome.
        if (!error.isEmpty() || found.isEmpty()) {
            probeJson_ = http3;
            found.clear();
            return;
        }
        useHttp2(copy, json);
    }

    // The copy turns the HTTP/2 fallback on rather than forcing HTTP/2, so HTTP/3 stays in use wherever it works.
    void WarpWorker::useHttp2(const std::shared_ptr<Configs::Profile> &copy, const QString &json) {
        base_ = copy;
        probeJson_ = json;
        http2Mode_ = true;
        {
            QMutexLocker locker(&mutex_);
            live_.warpHttp2 = true;
        }
        const auto line = ScanManager::tr("HTTP/3 gets no answer here; continuing over HTTP/2");
        pushLog(line);
        mainLog(ScanManager::tr("Scan %1: %2").arg(scan_.name, line));
    }

    QString WarpWorker::sampleRounds(const QList<Target> &seeds) {
        for (int round = 1;; ++round) {
            if (stop_ || stopRuleMet()) return {};
            auto targets = round == 1 ? seeds : QList<Target>();
            targets << roundTargets();
            if (targets.isEmpty()) return {};
            setStage(ScanManager::tr("Searching ranges"));
            pushLog(ScanManager::tr("Round %1: probing %n address(es)", nullptr, static_cast<int>(targets.size())).arg(round));
            if (auto error = probeTargets(targets, kWarpWorkerAttempts, true, nullptr, nullptr); !error.isEmpty()) return error;
        }
    }

    QString WarpWorker::rescan(bool &completed) {
        const auto rows = Configs::dataManager->ipListsRepo->GetEntries(scan_.rescan_snapshot_list_id);
        if (rows.isEmpty()) {
            scan_.rescan_snapshot_list_id = -1;
            return ScanManager::tr("The copy of the result list is gone; rescan the results again");
        }
        scan_.rescan_total = static_cast<quint64>(rows.size());
        scan_.rescan_cursor = std::min(scan_.rescan_cursor, scan_.rescan_total);
        setStage(ScanManager::tr("Rescanning"));
        pushLog(ScanManager::tr("Rescanning %n result(s)", nullptr, static_cast<int>(scan_.rescan_total - scan_.rescan_cursor)));
        setTested(scan_.rescan_cursor, scan_.rescan_total);
        while (scan_.rescan_cursor < scan_.rescan_total) {
            if (stop_) return {};
            const quint64 start = scan_.rescan_cursor;
            const auto chunk = rows.mid(static_cast<qsizetype>(start), kWarpWorkerChunk);
            // A cut-short chunk runs again whole.
            if (auto error = rescanRows(chunk, start); !error.isEmpty() || stop_) return error;
            scan_.rescan_cursor = start + static_cast<quint64>(chunk.size());
            saveProgress();
            setTested(scan_.rescan_cursor, scan_.rescan_total);
        }
        completed = true;
        return {};
    }

    QString WarpWorker::rescanRows(const QList<Configs::IpListEntry> &rows, quint64 start) {
        QList<Target> targets;
        QHash<QString, Configs::IpListEntry> stored;
        for (const auto &row : rows) {
            const auto host = warpWorkerHost(row.cidr);
            if (host.isEmpty() || stored.contains(warpWorkerKey(host, row.port))) continue;
            stored.insert(warpWorkerKey(host, row.port), row);
            targets << Target{host, row.port};
        }
        if (targets.isEmpty()) return {};
        QList<Candidate> winners;
        QList<Target> failures;
        if (auto error = probeTargets(targets, kWarpWorkerAttempts, false, &winners, &failures); !error.isEmpty() || stop_) return error;

        std::stable_sort(winners.begin(), winners.end(), &WarpWorker::better);
        QList<Candidate> confirmed;
        QList<Candidate> dataFailed;
        if (auto error = dataCheckAll(winners, confirmed, dataFailed); !error.isEmpty() || stop_) return error;
        if (!dataFailed.isEmpty()) {
            // A single cold request can fail at random: one more try before anything is removed.
            pushLog(ScanManager::tr("Re-checking data through %n endpoint(s)", nullptr, static_cast<int>(dataFailed.size())));
            QList<Candidate> stillFailed;
            if (auto error = dataCheckAll(dataFailed, confirmed, stillFailed); !error.isEmpty() || stop_) return error;
            dataFailed = stillFailed;
        }

        const auto storedRow = [&](const QString &host, int port) {
            return stored.value(warpWorkerKey(host, port), Configs::IpListEntry{host, port});
        };
        QList<Configs::IpListEntry> hits;
        QList<Configs::IpListEntry> dead;
        for (const auto &candidate : confirmed) {
            hits << entryOf(candidate);
            // A hit under another port replaces its row instead of adding a sibling.
            if (candidate.port != candidate.entryPort) dead << storedRow(candidate.address, candidate.entryPort);
        }
        QList<Configs::IpListEntry> failed;
        for (const auto &target : failures) failed << storedRow(target.address, target.port);
        for (const auto &candidate : dataFailed) failed << storedRow(candidate.address, candidate.entryPort);

        runConfirmed_ += static_cast<int>(confirmed.size());

        const auto isV6 = [](const Configs::IpListEntry &entry) { return warpWorkerIsIpv6(entry.cidr); };
        // Once IPv6 proved unusable here, an IPv6 failure says nothing about its row.
        if (!ipv6Allowed_) failed.removeIf(isV6);
        // Fresh checks, without the reuse window: an outage that began during this chunk must not delete its rows.
        auto passes = hits;
        QString detail;
        if (removals_.WouldRemove(passes, failed, false) && !networkUp(false, &detail)) {
            writeResults(hits, dead);
            if (stop_) return {};
            if (!detail.isEmpty()) mainLog(ScanManager::tr("Scan %1: network check failed: %2").arg(scan_.name, detail));
            return networkDownError();
        }
        if (removals_.WouldRemove(passes, failed, true) && !networkUp(true, &detail)) {
            if (stop_) {
                writeResults(hits, dead);
                return {};
            }
            dropIpv6(ScanManager::tr("IPv6 is unreachable from this network; skipping the IPv6 ranges"));
            failed.removeIf(isV6);
            passes.removeIf(isV6);
        }
        removals_.Add(passes, failed, start);
        dead << removals_.TakeRemovable();
        writeResults(hits, dead);
        return {};
    }

    QString WarpWorker::probeTargets(const QList<Target> &targets, int attempts, bool confirm, QList<Candidate> *winners,
                                     QList<Target> *failures) {
        const int chunkSize = chunkFor(attempts);
        for (qsizetype offset = 0; offset < targets.size(); offset += chunkSize) {
            if (stop_ || (!rescanPass() && stopRuleMet())) return {};
            auto chunk = targets.mid(offset, chunkSize);
            if (!ipv6Allowed_) chunk.removeIf([](const Target &target) { return warpWorkerIsIpv6(target.address); });
            if (chunk.isEmpty()) continue;
            auto batch = probe(chunk, attempts);
            if (!batch.error.isEmpty()) return batch.error;
            if (batch.aborted) return {};
            if (auto error = guard(batch); !error.isEmpty()) return error;
            if (batch.answered > 0) answeredAny_ = true;
            if (winners != nullptr) *winners << batch.winners;
            if (failures != nullptr) *failures << batch.failures;
            if (searching_) saveProgress();
            if (confirm) {
                addPending(batch.winners);
                if (auto error = drainPending(); !error.isEmpty()) return error;
            }
        }
        return {};
    }

    WarpWorker::ProbeBatch WarpWorker::probe(const QList<Target> &targets, int attempts) {
        ProbeBatch batch;
        libcore::ScanTargetSpec spec;
        spec.entries.reserve(static_cast<size_t>(targets.size()));
        for (const auto &target : targets) {
            libcore::ScanEntry entry;
            entry.cidr = target.address.toStdString();
            entry.port = target.port;
            spec.entries.push_back(std::move(entry));
        }
        spec.port_mode = "list";
        spec.shuffle = false;

        libcore::ScanWarpOptions warp;
        warp.enabled = true;
        warp.config_json = probeJson_.toStdString();
        warp.attempts = attempts;
        warp.timeout_ms = kWarpWorkerHandshakeTimeoutMs;

        const int concurrency = concurrencyFor(attempts);
        libcore::ScanProbeRequest request;
        request.session_id = sessionId_.toStdString();
        request.spec = std::move(spec);
        request.cursor = 0;
        request.max_targets = static_cast<int>(targets.size());
        request.concurrency = concurrency;
        request.spawn_interval_ms = kWarpWorkerSpawnIntervalMs;
        request.warp = std::move(warp);

        // The core bounds each target by attempts x timeout + 1 s.
        const qint64 rounds = (targets.size() + concurrency - 1) / concurrency;
        const qint64 timeout = rounds * (qint64(attempts) * kWarpWorkerHandshakeTimeoutMs + 1000) +
                               targets.size() * kWarpWorkerSpawnIntervalMs + kWarpWorkerRpcSlackMs;
        bool ok = false;
        QString coreError;
        const auto response = API::defaultClient->ScanProbe(&ok, request, &coreError, warpWorkerClampTimeout(timeout));
        const auto error = ok ? warpWorkerText(response.error.value()) : coreError;
        if (!ok || !error.isEmpty()) {
            if (warpWorkerIsStopped(error)) stop_.store(true);
            if (stop_) batch.aborted = true;
            else batch.error = error.isEmpty() ? ScanManager::tr("The core did not answer the probe request") : error;
            return batch;
        }
        for (const auto &item : response.results) {
            auto address = warpWorkerHost(warpWorkerText(item.address.value()));
            if (address.isEmpty()) address = warpWorkerText(item.address.value());
            const int entryPort = item.port.value();
            const int port = item.probe_port.value() > 0 ? item.probe_port.value() : entryPort;
            if (item.answered.value() > 0 || item.passed.value()) ++batch.answered;
            if (item.passed.value()) {
                batch.winners << Candidate{address, port, entryPort, item.sent.value(), item.answered.value(), item.handshake_ms.value()};
            } else if (item.local_failure.value()) {
                if (warpWorkerIsIpv6(address)) batch.v6LocalFailure = true;
                else batch.v4LocalFailure = true;
            } else {
                batch.failures << Target{address, entryPort};
            }
        }
        if (response.aborted.value()) {
            if (stop_) batch.aborted = true;
            else batch.error = ScanManager::tr("The core ended the scan session early");
        }
        return batch;
    }

    QString WarpWorker::guard(ProbeBatch &batch) {
        // Without a default route every target fails locally, with no lost egress to report.
        if (batch.answered == 0 && batch.v4LocalFailure) return stop_ ? QString() : networkDownError();
        bool v6Blamed = false;
        if (batch.v6LocalFailure && ipv6Allowed_) {
            dropIpv6(ScanManager::tr("This host has no IPv6 route; skipping the IPv6 ranges"));
            v6Blamed = true;
        }
        if (batch.answered == 0 && !batch.failures.isEmpty()) {
            const bool v6Only = std::all_of(batch.failures.cbegin(), batch.failures.cend(),
                                            [](const Target &target) { return warpWorkerIsIpv6(target.address); });
            QString detail;
            if (v6Only) {
                if (ipv6Allowed_ && !networkUp(true, &detail) && !stop_) {
                    dropIpv6(ScanManager::tr("IPv6 is unreachable from this network; skipping the IPv6 ranges"));
                    v6Blamed = true;
                }
            } else if (networkUpAtMs_ < 0 || elapsedMs() - networkUpAtMs_ >= kWarpWorkerNetworkReuseMs) {
                // Nothing answered: either nothing is here, or the network is gone.
                if (!networkUp(false, &detail)) {
                    if (stop_) return {};
                    if (!detail.isEmpty()) mainLog(ScanManager::tr("Scan %1: network check failed: %2").arg(scan_.name, detail));
                    return networkDownError();
                }
                networkUpAtMs_ = elapsedMs();
            }
        }
        // IPv6 failures just blamed on the network say nothing about their rows.
        if (v6Blamed) batch.failures.removeIf([](const Target &target) { return warpWorkerIsIpv6(target.address); });
        return {};
    }

    void WarpWorker::addPending(const QList<Candidate> &winners) {
        for (const auto &winner : winners) {
            if (confirmedAddresses_.contains(winner.address)) continue;
            const auto it = std::find_if(pending_.begin(), pending_.end(),
                                         [&winner](const Candidate &candidate) { return candidate.address == winner.address; });
            if (it == pending_.end()) pending_ << winner;
            else if (better(winner, *it)) *it = winner;
        }
    }

    QString WarpWorker::drainPending() {
        while (!pending_.isEmpty()) {
            if (stop_ || stopRuleMet()) return {};
            std::stable_sort(pending_.begin(), pending_.end(), &WarpWorker::better);
            const auto batch = pending_.mid(0, kWarpWorkerDataBatch);
            pending_.remove(0, batch.size());
            QList<Candidate> confirmed;
            QList<Candidate> failed;
            // A check cut short by a pause or an error still keeps what passed before it ended.
            const auto error = dataCheck(batch, confirmed, failed);
            storeConfirmed(confirmed);
            if (!error.isEmpty()) return error;
            if (stop_) return {};
        }
        return {};
    }

    QString WarpWorker::dataCheckAll(const QList<Candidate> &candidates, QList<Candidate> &confirmed, QList<Candidate> &failed) {
        for (qsizetype offset = 0; offset < candidates.size(); offset += kWarpWorkerDataBatch) {
            if (stop_) return {};
            if (auto error = dataCheck(candidates.mid(offset, kWarpWorkerDataBatch), confirmed, failed); !error.isEmpty()) return error;
        }
        return {};
    }

    QString WarpWorker::dataCheck(const QList<Candidate> &batch, QList<Candidate> &confirmed, QList<Candidate> &failed) {
        if (batch.isEmpty()) return {};
        const auto restoreStage = qScopeGuard([this, previous = stage_] { setStage(previous); });
        setStage(ScanManager::tr("Checking data"));
        pushLog(ScanManager::tr("Checking data through %n endpoint(s)", nullptr, static_cast<int>(batch.size())));
        QList<Configs::ScanTestTarget> targets;
        targets.reserve(batch.size());
        for (const auto &candidate : batch) targets << Configs::ScanTestTarget{candidate.address, candidate.port};
        const auto build = Configs::BuildScanTestConfig(base_, targets);
        if (!build.error.isEmpty()) return build.error;
        for (const int index : build.unsupported) {
            if (index >= 0 && index < batch.size()) failed << batch[index];
        }
        if (build.build == nullptr || build.build->outboundTags.isEmpty()) return {};

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
        req.url = testUrl_.toStdString();
        req.max_concurrency = static_cast<int>(batch.size());
        req.test_timeout_ms = kWarpWorkerDataTimeoutMs;
        request.test = std::move(req);
        // One cold request: the latency stored is the handshake's, so a warm-up only costs time.
        request.warm_latency = false;

        // The whole batch runs at once, so one handshake, one request and one startup bound it.
        const qint64 timeout = kWarpWorkerTunnelHandshakeMs + kWarpWorkerDataTimeoutMs + kWarpWorkerTunnelStartupMs +
                               Configs::DnsTunnelStartupAllowanceMs(base_) + kWarpWorkerRpcSlackMs;
        bool ok = false;
        QString coreError;
        const auto response = API::defaultClient->ScanURLTest(&ok, request, &coreError, warpWorkerClampTimeout(timeout));
        if (!ok) {
            if (warpWorkerIsStopped(coreError)) stop_.store(true);
            if (stop_) return {};
            return coreError.isEmpty() ? ScanManager::tr("The core did not answer the data check") : coreError;
        }

        QSet<int> reported;
        bool aborted = false;
        for (const auto &item : response.results) {
            const int index = build.tag2target.value(warpWorkerText(item.outbound_tag.value()), -1);
            if (index < 0 || index >= batch.size() || reported.contains(index)) continue;
            reported.insert(index);
            const auto error = warpWorkerText(item.error.value());
            if (error.isEmpty()) confirmed << batch[index];
            else if (warpWorkerIsAborted(error)) aborted = true;
            else failed << batch[index];
        }
        for (auto it = build.tag2target.cbegin(); it != build.tag2target.cend(); ++it) {
            if (!reported.contains(it.value())) aborted = true;
        }
        if (aborted && !stop_) return ScanManager::tr("The core ended the data check early");
        return {};
    }

    void WarpWorker::storeConfirmed(const QList<Candidate> &confirmed) {
        if (confirmed.isEmpty()) return;
        QList<Configs::IpListEntry> entries;
        for (const auto &candidate : confirmed) {
            entries << entryOf(candidate);
            if (!confirmedAddresses_.contains(candidate.address)) {
                confirmedAddresses_.insert(candidate.address);
                lastConfirmMs_ = elapsedMs();
            }
        }
        runConfirmed_ += static_cast<int>(confirmed.size());
        pushLog(ScanManager::tr("Confirmed %n endpoint(s)", nullptr, static_cast<int>(confirmed.size())));
        if (finder_) addHits(confirmed);
        else writeResults(entries, {});
    }

    void WarpWorker::addHits(const QList<Candidate> &confirmed) {
        for (const auto &candidate : confirmed) {
            const auto it = std::find_if(hits_.begin(), hits_.end(),
                                         [&candidate](const Candidate &hit) { return hit.address == candidate.address; });
            if (it == hits_.end()) hits_ << candidate;
            else if (better(candidate, *it)) *it = candidate;
        }
        std::stable_sort(hits_.begin(), hits_.end(), &WarpWorker::better);
        QList<Configs::IpListEntry> entries;
        entries.reserve(hits_.size());
        for (const auto &hit : std::as_const(hits_)) entries << entryOf(hit);
        {
            QMutexLocker locker(&mutex_);
            live_.found = static_cast<int>(entries.size());
            live_.hits = std::move(entries);
        }
        if (hooks_.results) hooks_.results();
    }

    void WarpWorker::writeResults(const QList<Configs::IpListEntry> &hits, const QList<Configs::IpListEntry> &dead) {
        if (!persist_ || scan_.result_list_id < 0) return;
        auto &lists = *Configs::dataManager->ipListsRepo;
        if (!hits.isEmpty()) {
            if (std::exchange(replacePending_, false)) {
                lists.ReplaceEntries(scan_.result_list_id, hits);
                ClearReplacePending(scan_);
            } else {
                lists.UpsertEntries(scan_.result_list_id, hits);
            }
        }
        if (!dead.isEmpty()) scan_.removed += lists.RemoveEntries(scan_.result_list_id, dead);
        scan_.found = lists.EntryCount(scan_.result_list_id);
        {
            QMutexLocker locker(&mutex_);
            live_.found = scan_.found;
            live_.removed = scan_.removed;
        }
        saveProgress();
        if (hooks_.results) hooks_.results();
    }

    bool WarpWorker::stopRuleMet() const {
        if (confirmedAddresses_.size() >= kWarpWorkerStopAfter) return true;
        const qint64 elapsed = elapsedMs();
        if (budgetBaseMs_ + elapsed >= kWarpWorkerBudgetMs) return true;
        // Quiet only once every handshake winner has had its data check.
        return lastConfirmMs_ >= 0 && pending_.isEmpty() && elapsed - lastConfirmMs_ >= kWarpWorkerQuietMs;
    }

    qint64 WarpWorker::elapsedMs() const {
        return warpWorkerNowMs() - clockStartMs_.load();
    }

    // Seconds of the budget spent; below the total while the search can still resume.
    quint64 WarpWorker::budgetCursor() const {
        const qint64 spent = (budgetBaseMs_.load() + elapsedMs()) / 1000;
        return static_cast<quint64>(std::clamp<qint64>(spent, 0, qint64(kWarpWorkerBudgetSeconds) - 1));
    }

    bool WarpWorker::networkUp(bool ipv6, QString *detail) {
        const auto restoreStage = qScopeGuard([this, previous = stage_] { setStage(previous); });
        setStage(ScanManager::tr("Checking network"));
        pushLog(ScanManager::tr("Checking the network"));
        bool ok = false;
        const bool up = API::defaultClient->ScanCheckNetwork(&ok, ipv6 ? kWarpWorkerNetworkCheckTargetsV6 : kWarpWorkerNetworkCheckTargets,
                                                             kWarpWorkerNetworkCheckTimeoutMs, detail);
        return ok && up;
    }

    void WarpWorker::dropIpv6(const QString &why) {
        if (!ipv6Allowed_) return;
        ipv6Allowed_ = false;
        pools_.removeIf([](const Pool &pool) { return pool.v6; });
        pushLog(why);
    }

    QStringList WarpWorker::discoveryAddresses(int count, bool anchors) {
        QList<Pool *> eligible;
        for (auto &pool : pools_) {
            if (pool.port <= 0 && !pool.exhausted) eligible << &pool;
        }
        QStringList addresses;
        if (anchors) {
            for (auto *pool : eligible) {
                if (addresses.size() >= count) break;
                if (pool->v6) continue;
                const auto anchor = QHostAddress(pool->v4First).toString();
                if (pool->used.contains(anchor)) continue;
                pool->used.insert(anchor);
                addresses << anchor;
            }
        }
        // IPv6 prefixes first, so a mixed list tries both families.
        std::stable_partition(eligible.begin(), eligible.end(), [](const Pool *pool) { return pool->v6; });
        qsizetype idle = 0;
        for (qsizetype i = 0; addresses.size() < count && idle < eligible.size(); ++i) {
            const auto address = nextAddress(*eligible[i % eligible.size()]);
            if (address.isEmpty()) {
                ++idle;
                continue;
            }
            idle = 0;
            addresses << address;
        }
        return addresses;
    }

    QList<WarpWorker::Target> WarpWorker::roundTargets() {
        QList<QList<Target>> perPool;
        for (auto &pool : pools_) {
            if (pool.exhausted || (pool.port <= 0 && ports_.isEmpty())) continue;
            QList<Target> picks;
            for (int i = 0; i < kWarpWorkerPerPrefix; ++i) {
                auto address = nextAddress(pool);
                if (address.isEmpty()) break;
                picks << Target{address, pool.port};
            }
            if (!picks.isEmpty()) perPool << picks;
        }
        // Interleaved across ranges; portless ones rotate through the ports found.
        QList<Target> targets;
        for (int i = 0; i < kWarpWorkerPerPrefix; ++i) {
            for (const auto &picks : perPool) {
                if (i >= picks.size()) continue;
                auto target = picks[i];
                if (target.port <= 0) target.port = ports_[portCursor_++ % ports_.size()];
                targets << target;
            }
        }
        return targets;
    }

    QString WarpWorker::nextAddress(Pool &pool) {
        if (pool.exhausted) return {};
        auto *rng = QRandomGenerator::global();
        const auto take = [&pool](const QString &address) {
            pool.used.insert(address);
            return address;
        };
        if (pool.v6 && pool.v6Prefix < 120) {
            auto bytes = pool.v6Net;
            for (int i = 0; i < 16; ++i) {
                const int keep = std::clamp(pool.v6Prefix - i * 8, 0, 8);
                if (keep == 8) continue;
                const auto mask = quint8(0xFF << (8 - keep));
                bytes[i] = quint8((bytes[i] & mask) | (quint8(rng->bounded(256)) & quint8(~mask)));
            }
            return take(QHostAddress(bytes).toString());
        }
        // At most 256 addresses: drawn without repeats.
        const quint32 count = pool.v6 ? quint32(1) << (128 - pool.v6Prefix) : pool.v4Count;
        const auto addressAt = [&pool](quint32 offset) {
            if (!pool.v6) return QHostAddress(pool.v4First + offset).toString();
            auto bytes = pool.v6Net;
            bytes[15] = quint8(bytes[15] | quint8(offset));
            return QHostAddress(bytes).toString();
        };
        if (quint32(pool.used.size()) >= count) {
            pool.exhausted = true;
            return {};
        }
        for (int tries = 0; tries < 64; ++tries) {
            const auto address = addressAt(rng->bounded(count));
            if (!pool.used.contains(address)) return take(address);
        }
        for (quint32 offset = 0; offset < count; ++offset) {
            const auto address = addressAt(offset);
            if (!pool.used.contains(address)) return take(address);
        }
        pool.exhausted = true;
        return {};
    }

    void WarpWorker::logOutcome() {
        if (runConfirmed_ > 0) {
            pushLog(ScanManager::tr("Found %n endpoint(s)", nullptr, static_cast<int>(confirmedAddresses_.size())));
            // A finder run's caller applies these itself from LiveState().
            if (finder_) return;
            QString line;
            if (junkMode_)
                line = ScanManager::tr("These endpoints need AmneziaWG junk: turn on AmneziaWG in the profile (Jc %1, Jmin %2, Jmax %3)")
                           .arg(WarpPresets::kJunkCount)
                           .arg(WarpPresets::kJunkMin)
                           .arg(WarpPresets::kJunkMax);
            else if (http2Mode_)
                line = ScanManager::tr("These endpoints need HTTP/2: set the profile's HTTP version to HTTP/3 (fallback to HTTP/2)");
            if (!line.isEmpty()) {
                pushLog(line);
                mainLog(ScanManager::tr("Scan %1: %2").arg(scan_.name, line));
            }
            return;
        }
        QString line;
        if (!answeredAny_) line = ScanManager::tr("No endpoint answered the WARP handshake; this network may block it");
        else if (junkMode_) line = ScanManager::tr("Endpoints answered only with AmneziaWG junk, and no data flowed through them");
        else line = ScanManager::tr("Endpoints answered the handshake, but no data flowed through them");
        pushLog(line);
        mainLog(ScanManager::tr("Scan %1: %2").arg(scan_.name, line));
    }

    void WarpWorker::finishRun(bool completed, const QString &error) {
        if (finder_) {
            // A stop ends on what was found too; an error speaks for itself.
            if (!completed && error.isEmpty()) pushLog(ScanManager::tr("Found %n endpoint(s)", nullptr, static_cast<int>(hits_.size())));
            finalized_.store(true);
            QMutexLocker locker(&mutex_);
            live_.running = false;
            live_.error = error;
            // The seconds the search took, where a scan would show its whole budget.
            live_.tested = std::min<quint64>(scan_.total, static_cast<quint64>((searchMs_ + 500) / 1000));
            live_.total = scan_.total;
            return;
        }
        using Status = Configs::IpScan::Status;
        if (completed) {
            if (rescanPass()) {
                QStringList lines;
                if (!removals_.AnyPassed() && removals_.PendingCount() > 0) {
                    lines << ScanManager::tr("No result passed, so none was removed; the network or the profile may be at fault");
                } else {
                    pushLog(ScanManager::tr("%n result(s) passed", nullptr, runConfirmed_));
                    lines << removals_.KeptLines();
                }
                for (const auto &line : lines) {
                    pushLog(line);
                    mainLog(ScanManager::tr("Scan %1: %2").arg(scan_.name, line));
                }
                deleteList(scan_.rescan_snapshot_list_id);
                scan_.rescan_snapshot_list_id = -1;
                // A search paused before the rescan stays resumable.
                if (scan_.CanResumeInitial()) {
                    scan_.mode = Configs::IpScan::Mode::Initial;
                    scan_.status = Status::Paused;
                } else {
                    scan_.status = Status::Completed;
                }
            } else {
                deleteList(scan_.snapshot_list_id);
                scan_.snapshot_list_id = -1;
                scan_.cursor = scan_.total;
                scan_.status = Status::Completed;
            }
            if (scan_.status == Status::Completed) scan_.finished_at = QDateTime::currentSecsSinceEpoch();
            scan_.last_error.clear();
        } else {
            if (rescanPass()) scan_.rescan_cursor = std::min(scan_.rescan_cursor, removals_.UnresolvedFrom());
            const bool rescanOpen = rescanResumable();
            if (rescanPass() && !rescanOpen && scan_.CanResumeInitial()) scan_.mode = Configs::IpScan::Mode::Initial;
            scan_.last_error = error;
            if (rescanOpen || scan_.CanResumeInitial()) scan_.status = Status::Paused;
            else if (!error.isEmpty()) scan_.status = Status::Failed;
            else scan_.status = initialStatus_ == Status::Completed ? Status::Completed : Status::Idle;
        }
        persistState();
        finalized_.store(true);
        if (persist_) IpListUpdater::instance()->NotifyListsChanged();
        if (!error.isEmpty()) mainLog(ScanManager::tr("Scan %1 stopped: %2").arg(scan_.name, error));

        QMutexLocker locker(&mutex_);
        live_.error = scan_.last_error;
        live_.tested = scan_.ActiveCursor();
        live_.total = scan_.ActiveTotal();
        live_.found = scan_.found;
        live_.removed = scan_.removed;
    }

    void WarpWorker::persistState() {
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

    void WarpWorker::saveProgress() {
        if (searching_) scan_.cursor = budgetCursor();
        if (!persist_) return;
        // Rows still waiting for a pass are checked again after a pause.
        const quint64 cursor = rescanPass() ? std::min(scan_.rescan_cursor, removals_.UnresolvedFrom()) : scan_.cursor;
        Configs::dataManager->ipScansRepo->SaveProgress(scanId_, scan_.mode, cursor, scan_.ActiveTotal(), scan_.found, scan_.removed);
    }

    void WarpWorker::deleteList(int listId) {
        if (listId < 0 || !persist_) return;
        Configs::dataManager->ipListsRepo->DeleteIpList(listId);
        listsTouched_ = true;
    }

    void WarpWorker::setStage(const QString &stage) {
        stage_ = stage;
        {
            QMutexLocker locker(&mutex_);
            if (live_.stage == stage) return;
            live_.stage = stage;
        }
        notifyProgress();
    }

    void WarpWorker::setTested(quint64 tested, quint64 total) {
        {
            QMutexLocker locker(&mutex_);
            live_.tested = tested;
            live_.total = total;
        }
        notifyProgress();
    }

    void WarpWorker::pushLog(const QString &line) {
        {
            QMutexLocker locker(&mutex_);
            live_.log << line;
            while (live_.log.size() > ScanManager::kLogLines) live_.log.removeFirst();
        }
        notifyProgress();
    }

    void WarpWorker::mainLog(const QString &line) const {
        if (!finder_) MW_show_log(line);
    }

    void WarpWorker::notifyProgress() const {
        if (hooks_.progress) hooks_.progress();
    }

    // A finder run cannot resume, so its error promises no pause.
    QString WarpWorker::networkDownError() const {
        return finder_ ? ScanManager::tr("Network appears to be down") : ScanManager::tr("Network appears to be down; scan paused");
    }

    bool WarpWorker::rescanResumable() const {
        return rescanPass() && scan_.rescan_snapshot_list_id >= 0 && scan_.rescan_cursor < scan_.rescan_total;
    }

    int WarpWorker::concurrencyFor(int attempts) const {
        if (isMasque()) return kWarpWorkerMasqueConcurrency;
        return attempts == kWarpWorkerDiscoveryAttempts ? kWarpWorkerDiscoveryConcurrency : kWarpWorkerConcurrency;
    }

    // Discovery calls hold two waves, so its usual targets go out at once; other calls stay small, so data checks start early.
    int WarpWorker::chunkFor(int attempts) const {
        return attempts == kWarpWorkerDiscoveryAttempts ? 2 * concurrencyFor(attempts) : kWarpWorkerChunk;
    }

    bool WarpWorker::better(const Candidate &a, const Candidate &b) {
        const int lossA = std::max(lossPct(a), 0);
        const int lossB = std::max(lossPct(b), 0);
        if (lossA != lossB) return lossA < lossB;
        const int rttA = a.handshakeMs > 0 ? a.handshakeMs : std::numeric_limits<int>::max();
        const int rttB = b.handshakeMs > 0 ? b.handshakeMs : std::numeric_limits<int>::max();
        return rttA < rttB;
    }

    int WarpWorker::lossPct(const Candidate &candidate) {
        if (candidate.sent <= 0) return -1;
        return static_cast<int>(std::lround(100.0 * std::max(0, candidate.sent - candidate.answered) / candidate.sent));
    }

    Configs::IpListEntry WarpWorker::entryOf(const Candidate &candidate) {
        Configs::IpListEntry entry;
        entry.cidr = candidate.address;
        entry.port = candidate.port;
        entry.latencyMs = std::max(candidate.handshakeMs, 0);
        entry.lossPct = lossPct(candidate);
        return entry;
    }
} // namespace Scanner
