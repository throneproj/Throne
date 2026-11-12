#include "include/scanner/TuneWorker.h"

#include <QDateTime>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QThread>

#include <algorithm>
#include <utility>

#include "include/api/RPC.h"
#include "include/configs/generate.h"
#include "include/database/IpScansRepo.h"
#include "include/global/Configs.hpp"
#include "include/scanner/TuneProfiles.h"

namespace Scanner {
    namespace {
        constexpr int kTuneWorkerNetworkCheckTimeoutMs = 3000;
        constexpr qint64 kTuneWorkerTunnelHandshakeMs = 10000;
        constexpr qint64 kTuneWorkerTunnelStartupMs = 5000;
        constexpr qint64 kTuneWorkerRpcSlackMs = 30000;
        constexpr int kTuneWorkerMinGapMs = 1000;
        constexpr int kTuneWorkerMaxGapMs = 3000;
        constexpr qint64 kTuneWorkerWindowMs = 60000;
        constexpr qint64 kTuneWorkerSleepSliceMs = 100;

        const QStringList kTuneWorkerNetworkTargets = {
            QStringLiteral("1.1.1.1:443"),
            QStringLiteral("8.8.8.8:443"),
            QStringLiteral("9.9.9.9:443"),
            QStringLiteral("223.5.5.5:443"),
            QStringLiteral("77.88.8.8:443"),
        };

        const QStringList kTuneWorkerNetworkTargetsV6 = {
            QStringLiteral("[2606:4700:4700::1111]:443"),
            QStringLiteral("[2001:4860:4860::8888]:443"),
            QStringLiteral("[2620:fe::fe]:443"),
        };

        std::atomic<quint32> tuneWorkerSessionCounter{0};
    } // namespace

    TuneWorker::TuneWorker(const Configs::IpScan &scan, std::shared_ptr<Configs::Profile> base, QString dialHost, QString specHash,
                           bool resume, ScanRunHooks hooks)
        : scanId_(scan.id),
          sessionId_(QStringLiteral("tune-%1-%2-%3")
                         .arg(scan.id)
                         .arg(QDateTime::currentMSecsSinceEpoch())
                         .arg(tuneWorkerSessionCounter.fetch_add(1))),
          base_(std::move(base)),
          dialHost_(std::move(dialHost)),
          specHash_(std::move(specHash)),
          resume_(resume),
          hooks_(std::move(hooks)),
          scan_(scan) {
        scan_.status = Configs::IpScan::Status::Running;
        scan_.mode = Configs::IpScan::Mode::Initial;
        scan_.last_error.clear();
        live_.running = true;
        live_.stage = ScanManager::tr("Preparing");
        if (resume_) {
            live_.tested = scan.cursor;
            live_.total = scan.total;
            live_.found = scan.found;
        }
    }

    void TuneWorker::Start() {
        const auto self = shared_from_this();
        runOnNewThread([self] { self->run(); });
    }

    void TuneWorker::RequestStop() {
        stop_.store(true);
        QMutexLocker locker(&mutex_);
        live_.stopping = true;
    }

    void TuneWorker::DisablePersist() {
        persist_.store(false);
    }

    ScanLiveState TuneWorker::LiveState() const {
        QMutexLocker locker(&mutex_);
        auto state = live_;
        state.stopping = stop_.load();
        return state;
    }

    void TuneWorker::run() {
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
            finish(completed, error);
        } catch (...) {
            MW_show_log(ScanManager::tr("Scan %1: saving its final state failed").arg(scanId_));
        }
        if (hooks_.finished) hooks_.finished();
        bool ok = false;
        API::defaultClient->StopScan(&ok, sessionId_);
    }

    QString TuneWorker::execute(bool &completed) {
        clock_.start();
        const auto &options = scan_.config.tune;
        if (base_ == nullptr) return ScanManager::tr("Choose the profile to find working settings for");
        if (auto error = Tune::CheckOptions(options); !error.isEmpty()) return error;
        if (auto error = Configs::ValidateScanBase(base_); !error.isEmpty()) return error;
        const auto space = Tune::SpaceFor(*base_, options);
        if (space.IsEmpty()) return ScanManager::tr("Nothing to try: turn on a setting this profile supports");
        const Tune::Planner planner(space, Tune::LimitsFor(options));
        // A tunnel MTU too large for the path passes a small request and stalls bulk data, so the MTU search downloads.
        if (space.mtu) {
            testUrl_ = Tune::MtuTestUrl(options);
            bodyBytes_ = options.mtuBytes;
        } else {
            testUrl_ = options.url.trimmed();
            if (testUrl_.isEmpty()) testUrl_ = Configs::dataManager->settingsRepo->test_latency_url;
        }

        if (resume_) {
            trials_ = Tune::TrialsFromJson(scan_.tune_state.value(QLatin1String("trials")).toArray());
            elapsedBeforeMs_ = std::max<qint64>(0, scan_.tune_state.value(QLatin1String("elapsedMs")).toInteger());
            note_.clear();
        } else {
            scan_.spec_hash = specHash_;
            scan_.cursor = 0;
            scan_.total = 0;
            scan_.found = 0;
            scan_.started_at = QDateTime::currentSecsSinceEpoch();
            scan_.finished_at = 0;
            if (persist_) Configs::dataManager->ipScansRepo->SaveTuneState(scanId_, {});
            saveRow();
        }
        pushLog(ScanManager::tr("Every variant connects to %1").arg(dialHost_));
        persist(planner);

        const qint64 timeLimitMs = static_cast<qint64>(options.maxMinutes) * 60000;
        int lastStage = -1;
        int lastRound = 0;
        for (;;) {
            if (stop_) return {};
            if (elapsedMs() >= timeLimitMs) {
                note_ = ScanManager::tr("Stopped at the time limit of %n minute(s)", nullptr, options.maxMinutes);
                completed = true;
                return {};
            }
            const int index = planner.Next(trials_);
            if (index < 0) {
                if (Tune::UsedAttempts(trials_) >= options.maxAttempts)
                    note_ = ScanManager::tr("Stopped at the limit of %n attempt(s)", nullptr, options.maxAttempts);
                completed = true;
                return {};
            }
            const auto stage = static_cast<int>(trials_[index].stage);
            if (stage != lastStage || trials_[index].round != lastRound) {
                lastStage = stage;
                lastRound = trials_[index].round;
                setStage(stageText(trials_[index]));
                if (!networkUp()) return {};
            }

            const auto label = Tune::Describe(trials_[index].assignment);
            const auto built = variant(index);
            if (!built.error.isEmpty()) {
                // A variant the core refuses fails alone, without costing an attempt.
                trials_[index].dropped = true;
                trials_[index].lastError = built.error;
                pushLog(ScanManager::tr("%1: cannot be used: %2").arg(label, built.error));
                persist(planner);
                continue;
            }
            if (!pace()) return {};
            int latencyMs = 0;
            QString error;
            const auto outcome = attempt(built, label, &latencyMs, &error);
            if (outcome == Outcome::Stopped) return {};
            if (outcome == Outcome::Passed) {
                auto &trial = trials_[index];
                ++trial.successes;
                trial.latenciesMs << latencyMs;
                pushLog(ScanManager::tr("%1: passed in %2 ms (%3/%4)").arg(label).arg(latencyMs).arg(trial.successes).arg(trial.Attempts()));
            } else {
                // A failure during an outage says nothing about the variant: it is not counted.
                if (outcome == Outcome::NetworkLost) {
                    pauseReason_ = ScanManager::tr("The network is down, so the search paused; resume it when the connection is back");
                    pushLog(pauseReason_);
                    return {};
                }
                if (!networkUp()) return {};
                auto &trial = trials_[index];
                ++trial.failures;
                trial.lastError = error;
                pushLog(ScanManager::tr("%1: failed (%2/%3): %4").arg(label).arg(trial.successes).arg(trial.Attempts()).arg(error));
            }
            persist(planner);
        }
    }

    TuneWorker::Variant TuneWorker::variant(int index) {
        if (const auto it = variants_.constFind(index); it != variants_.cend()) return it.value();
        Variant built;
        QString error;
        const auto profile = Tune::Copy(*base_);
        if (profile == nullptr) {
            built.error = ScanManager::tr("The profile could not be copied");
        } else if (!Tune::Apply(*profile, trials_[index].assignment, &error)) {
            built.error = error;
        } else if (auto problem = Configs::ValidateScanBase(profile); !problem.isEmpty()) {
            built.error = problem;
        } else {
            const auto build = Configs::BuildScanTestConfig(profile, {Configs::ScanTestTarget{dialHost_, 0}});
            if (!build.error.isEmpty()) {
                built.error = build.error;
            } else if (build.build == nullptr || build.build->outboundTags.isEmpty()) {
                built.error = ScanManager::tr("This profile cannot be pointed at %1").arg(dialHost_);
            } else {
                built.build = build.build;
                built.vpnEndpointTags = build.vpnEndpointTags;
            }
        }
        variants_.insert(index, built);
        return built;
    }

    TuneWorker::Outcome TuneWorker::attempt(const Variant &variant, const QString &label, int *latencyMs, QString *error) {
        const auto &build = *variant.build;
        const auto &options = scan_.config.tune;
        libcore::ScanURLTestRequest request;
        request.session_id = sessionId_.toStdString();
        libcore::TestReq req;
        for (const auto &tag : build.outboundTags) {
            req.outbound_tags.push_back(tag.toStdString());
            request.target_labels.push_back(label.toStdString());
        }
        req.config = QJsonObject2QString(build.coreConfig, false).toStdString();
        req.use_default_outbound = false;
        if (build.isXrayNeeded) req.xray_config = QJsonObject2QString(build.xrayConfig, false).toStdString();
        req.need_xray = !req.xray_config.value().empty();
        for (const auto &xrayConfig : build.xrayFullConfigs) req.xray_full_configs.push_back(xrayConfig.toStdString());
        req.xray_outbound_dns_strategy = build.xrayDnsStrategy.toStdString();
        req.url = testUrl_.toStdString();
        req.max_concurrency = 1;
        req.test_timeout_ms = options.timeoutMs;
        for (const auto &tag : variant.vpnEndpointTags) req.vpn_endpoint_tags.push_back(tag.toStdString());
        request.test = std::move(req);
        // Cold: the handshake the censor judges is part of every measurement.
        request.warm_latency = false;
        if (bodyBytes_ > 0) request.body_bytes = bodyBytes_;

        const auto timeoutMs = static_cast<int>(kTuneWorkerTunnelHandshakeMs + options.timeoutMs + kTuneWorkerTunnelStartupMs +
                                                kTuneWorkerRpcSlackMs);
        bool ok = false;
        QString coreError;
        const auto response = API::defaultClient->ScanURLTest(&ok, request, &coreError, timeoutMs);
        lastAttemptEndMs_ = clock_.elapsed();
        if (stop_) return Outcome::Stopped;
        if (!ok) {
            if (coreError.contains(QStringLiteral("scan stopped"))) {
                stop_.store(true);
                return Outcome::Stopped;
            }
            *error = coreError.isEmpty() ? ScanManager::tr("The core did not answer the config test") : coreError;
            return coreError.contains(QStringLiteral("no default interface")) ? Outcome::NetworkLost : Outcome::Failed;
        }
        for (const auto &item : response.results) {
            const auto itemError = QString::fromStdString(item.error.value());
            if (itemError.isEmpty()) {
                *latencyMs = item.latency_ms.value();
                return Outcome::Passed;
            }
            if (itemError.contains(QStringLiteral("test aborted"))) return Outcome::Stopped;
            *error = itemError;
            return Outcome::Failed;
        }
        *error = ScanManager::tr("The core returned no result");
        return Outcome::Failed;
    }

    bool TuneWorker::networkUp() {
        const auto previous = stageText_;
        setStage(ScanManager::tr("Checking network"));
        const bool ipv6 = QHostAddress(dialHost_).protocol() == QAbstractSocket::IPv6Protocol;
        bool ok = false;
        QString detail;
        const bool up = API::defaultClient->ScanCheckNetwork(&ok, ipv6 ? kTuneWorkerNetworkTargetsV6 : kTuneWorkerNetworkTargets,
                                                            kTuneWorkerNetworkCheckTimeoutMs, &detail);
        setStage(previous);
        if (ok && up) return true;
        pauseReason_ = ok ? ScanManager::tr("The network is down, so the search paused; resume it when the connection is back")
                          : ScanManager::tr("The core did not answer the network check, so the search paused");
        pushLog(pauseReason_);
        return false;
    }

    bool TuneWorker::pace() {
        if (lastAttemptEndMs_ >= 0) {
            const int gap = kTuneWorkerMinGapMs + static_cast<int>(QRandomGenerator::global()->bounded(kTuneWorkerMaxGapMs - kTuneWorkerMinGapMs + 1));
            if (!sleepUntil(lastAttemptEndMs_ + gap)) return false;
        }
        const auto perMinute = static_cast<size_t>(std::max(1, scan_.config.tune.perMinute));
        for (;;) {
            const qint64 now = clock_.elapsed();
            while (!attemptStarts_.empty() && attemptStarts_.front() <= now - kTuneWorkerWindowMs) attemptStarts_.pop_front();
            if (attemptStarts_.size() < perMinute) break;
            if (!sleepUntil(attemptStarts_.front() + kTuneWorkerWindowMs)) return false;
        }
        attemptStarts_.push_back(clock_.elapsed());
        return true;
    }

    bool TuneWorker::sleepUntil(qint64 deadlineMs) {
        while (!stop_.load()) {
            const qint64 left = deadlineMs - clock_.elapsed();
            if (left <= 0) return true;
            QThread::msleep(static_cast<unsigned long>(std::min(left, kTuneWorkerSleepSliceMs)));
        }
        return false;
    }

    void TuneWorker::persist(const Tune::Planner &planner) {
        const int used = Tune::UsedAttempts(trials_);
        const int total = planner.EstimateTotal(trials_);
        const auto found = static_cast<int>(std::count_if(trials_.cbegin(), trials_.cend(), [](const Tune::Trial &trial) { return trial.successes > 0; }));
        best_ = planner.Best(trials_);
        mtuWinner_ = planner.MtuWinner(trials_);
        scan_.cursor = used;
        scan_.total = total;
        scan_.found = found;
        {
            QMutexLocker locker(&mutex_);
            live_.tested = used;
            live_.total = total;
            live_.found = found;
        }
        if (persist_) {
            auto &scans = *Configs::dataManager->ipScansRepo;
            scans.SaveTuneState(scanId_, state());
            scans.SaveProgress(scanId_, Configs::IpScan::Mode::Initial, used, total, found, 0);
        }
        notifyProgress();
        if (hooks_.results) hooks_.results();
    }

    void TuneWorker::saveRow() {
        if (!persist_) return;
        auto &scans = *Configs::dataManager->ipScansRepo;
        // Re-read so a rename made while running survives: the run owns only these columns.
        const auto row = scans.GetIpScan(scanId_);
        if (row == nullptr) return;
        row->status = scan_.status;
        row->mode = Configs::IpScan::Mode::Initial;
        row->cursor = scan_.cursor;
        row->total = scan_.total;
        row->spec_hash = scan_.spec_hash;
        row->found = scan_.found;
        row->removed = 0;
        row->last_error = scan_.last_error;
        row->started_at = scan_.started_at;
        row->finished_at = scan_.finished_at;
        scans.Save(row);
    }

    void TuneWorker::finish(bool completed, const QString &error) {
        using Status = Configs::IpScan::Status;
        elapsedBeforeMs_ = elapsedMs();
        clock_.invalidate();
        // Stored trials keep a stopped search resumable, also after an error.
        if (completed) {
            scan_.status = Status::Completed;
            // The estimate still holds the confirm round of a search that ended without one.
            scan_.total = scan_.cursor;
            scan_.last_error.clear();
            scan_.finished_at = QDateTime::currentSecsSinceEpoch();
        } else {
            scan_.status = !trials_.isEmpty() ? Status::Paused : error.isEmpty() ? Status::Idle : Status::Failed;
            scan_.last_error = error.isEmpty() ? pauseReason_ : error;
        }
        if (persist_) Configs::dataManager->ipScansRepo->SaveTuneState(scanId_, state());
        saveRow();
        finalized_.store(true);
        if (!error.isEmpty()) MW_show_log(ScanManager::tr("Scan %1 stopped: %2").arg(scan_.name, error));

        QMutexLocker locker(&mutex_);
        live_.error = scan_.last_error;
        live_.tested = scan_.cursor;
        live_.total = scan_.total;
        live_.found = scan_.found;
    }

    QJsonObject TuneWorker::state() const {
        QJsonObject object;
        object["version"] = 1;
        object["trials"] = Tune::TrialsToJson(trials_);
        object["best"] = best_;
        object["mtuWinner"] = mtuWinner_;
        object["elapsedMs"] = elapsedMs();
        object["dialHost"] = dialHost_;
        object["profileId"] = scan_.config.tune.profileId;
        if (!note_.isEmpty()) object["note"] = note_;
        return object;
    }

    qint64 TuneWorker::elapsedMs() const {
        return elapsedBeforeMs_ + (clock_.isValid() ? clock_.elapsed() : 0);
    }

    QString TuneWorker::stageText(const Tune::Trial &trial) {
        if (trial.round > 1) return ScanManager::tr("Confirming the best variants");
        switch (trial.stage) {
            case Tune::Stage::Baseline:
                return ScanManager::tr("Testing the profile as it is");
            case Tune::Stage::Single:
                return ScanManager::tr("Trying one change at a time");
            case Tune::Stage::Combine:
                return ScanManager::tr("Combining what helped");
            case Tune::Stage::Refine:
                return ScanManager::tr("Trying fragment delays");
            case Tune::Stage::Mtu:
                return ScanManager::tr("Searching for the largest working MTU");
        }
        return {};
    }

    void TuneWorker::setStage(const QString &stage) {
        stageText_ = stage;
        {
            QMutexLocker locker(&mutex_);
            if (live_.stage == stage) return;
            live_.stage = stage;
        }
        notifyProgress();
    }

    void TuneWorker::pushLog(const QString &line) {
        {
            QMutexLocker locker(&mutex_);
            live_.log << line;
            while (live_.log.size() > ScanManager::kLogLines) live_.log.removeFirst();
        }
        notifyProgress();
    }

    void TuneWorker::notifyProgress() const {
        if (hooks_.progress) hooks_.progress();
    }
} // namespace Scanner
