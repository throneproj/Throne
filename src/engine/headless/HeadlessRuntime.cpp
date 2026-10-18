#include "include/engine/headless/HeadlessRuntime.h"

#include <chrono>
#include <csignal>
#include <utility>

#include <QCoreApplication>
#include <QJsonValue>
#include <QThread>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "NkrVersion.h"
#include "include/api/RPC.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineTypes.h"
#include "include/engine/app/ExitCodes.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/headless/HeadlessApi.h"
#include "include/engine/headless/HeadlessPolicy.h"
#include "include/engine/headless/ResumeGuard.h"
#include "include/engine/headless/RuntimeStatus.h"
#include "include/engine/headless/SupervisorLink.h"
#include "include/engine/supervisor/CoreSupervisor.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/global/Utils.hpp"

namespace Engine {
    namespace {
        HeadlessRuntime *headlessRuntimeInstance = nullptr;

        constexpr int kHeadlessRuntimeReadyWarnMs = 20000;

        bool headlessRuntimeExiting() {
            return Configs::dataManager != nullptr && Configs::dataManager->settingsRepo->prepare_exit;
        }

        QString headlessRuntimeProfileName(std::shared_ptr<Configs::Profile> profile, int fallbackId) {
            if (profile == nullptr && fallbackId >= 0) profile = Configs::dataManager->profilesRepo->GetProfile(fallbackId);
            if (profile == nullptr) return fallbackId >= 0 ? QStringLiteral("profile %1").arg(fallbackId) : QStringLiteral("a profile");
            const QString name = (profile->outbound != nullptr ? profile->outbound->DisplayName() : profile->name).simplified();
            return name.isEmpty() ? QStringLiteral("profile %1").arg(profile->id) : name;
        }

#ifdef Q_OS_UNIX
        // The controlling terminal of a hung-up session can no longer be opened.
        bool headlessRuntimeTtyGone() {
            const int fd = ::open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
            if (fd >= 0) {
                ::close(fd);
                return false;
            }
            return errno == ENXIO || errno == EIO;
        }
#endif
    } // namespace

    HeadlessRuntime::HeadlessRuntime(const HeadlessCli::LaunchOptions &options, const Bootstrap::DataDir &dataDir, qint64 bootMonoMs,
                                     QObject *parent)
        : QObject(parent), m_options(options), m_dataDir(dataDir), m_bootMonoMs(bootMonoMs),
          m_policy(std::make_unique<HeadlessPolicy>()), m_guard(std::make_unique<ResumeGuard>(dataDir.configDir)) {
        if (headlessRuntimeInstance != nullptr) LOG_WARN("A second HeadlessRuntime replaces the first");
        headlessRuntimeInstance = this;
    }

    HeadlessRuntime::~HeadlessRuntime() {
        if (headlessRuntimeInstance == this) headlessRuntimeInstance = nullptr;
        if (auto *engine = host()) engine->detachFrontEnd();
    }

    HeadlessRuntime *HeadlessRuntime::current() {
        return headlessRuntimeInstance;
    }

    void HeadlessRuntime::RequestExit(int code, const QString &reason) {
        if (auto *runtime = current()) {
            runtime->terminate(code, reason);
            return;
        }
        // The relay fallback in a GUI process: a teardown already running would have its nested waits cut short by exit().
        if (headlessRuntimeExiting()) return;
        MW_show_log(code == ExitCode::Restart ? QStringLiteral("Exiting to be restarted.") : QStringLiteral("Exiting."));
        if (auto *engine = host()) engine->shutdown(false);
        QCoreApplication::exit(code);
    }

    EnginePolicy *HeadlessRuntime::policy() {
        return m_policy.get();
    }

    void HeadlessRuntime::attach(EngineHost *engine) {
        if (m_host != nullptr) {
            LOG_WARN("HeadlessRuntime::attach() called again; ignored");
            return;
        }
        m_host = engine;
        auto *supervisor = engine->supervisor();
        m_status = new RuntimeStatus(engine, m_bootMonoMs, this);
        connect(m_status, &RuntimeStatus::readyChanged, this, [this] {
            LOG_INFO(QStringLiteral("Ready %1 ms after launch: %2").arg(MonotonicMs() - m_bootMonoMs).arg(m_status->summary()));
        });
        m_link = new SupervisorLink(this);
        m_link->attach(m_status, engine->exitController());
        m_link->setInPlaceRestart([this] { return m_inPlaceRestart && exitCode(-1) == ExitCode::Restart; });

        connect(supervisor, &CoreSupervisor::coreGaveUp, this, &HeadlessRuntime::onCoreGaveUp);
        connect(supervisor, &CoreSupervisor::resumeCancelled, this, [this] { m_retryResumeId = -1; }, Qt::DirectConnection);
        connect(supervisor, &CoreSupervisor::listenFailed, this, [](const QString &error) {
            LOG_ERROR(QStringLiteral("The core IPC listener failed: %1").arg(error));
        });

        m_stableTimer = new QTimer(this);
        m_stableTimer->setSingleShot(true);
        m_stableTimer->setInterval(std::chrono::milliseconds(ResumeGuard::kSurviveMs));
        connect(m_stableTimer, &QTimer::timeout, this, [this] {
            if (m_host->supervisor()->state() != CoreState::Connected) return;
            const bool wasStayingUp = stayUp();
            m_guard->markStable();
            if (wasStayingUp) {
                m_coreRetryTimer->stop();
                LOG_INFO(QStringLiteral("[Core] The core stayed connected for %1 s; a core give-up exits %2 again.")
                             .arg(ResumeGuard::kSurviveMs / 1000)
                             .arg(ExitCode::CoreGaveUp));
            }
        });
        m_coreRetryTimer = new QTimer(this);
        m_coreRetryTimer->setInterval(kStayUpRetryMs);
        connect(m_coreRetryTimer, &QTimer::timeout, this, [this] {
            auto *core = m_host->supervisor();
            if (core->state() != CoreState::GaveUp || terminating()) return;
            LOG_INFO(QStringLiteral("[Core] Starting the core again (retried every %1 min while it gives up).")
                         .arg(kStayUpRetryMs / 60000));
            // resetCore() only posts the restart, so the boot start is in place before the new core's coreStarted.
            if (core->resetCore() && m_retryResumeId >= 0) {
                m_host->connection()->setBootStart(StartRequest{m_retryResumeId, false, m_bootSerial});
                m_bootPhase = BootPhase::Waiting;
                core->setStartWhenUp(m_retryResumeId);
            }
        });
        connect(supervisor, &CoreSupervisor::stateChanged, this, [this](CoreState state) {
            if (state == CoreState::Connected) {
                m_stableTimer->start();
            } else {
                m_stableTimer->stop();
            }
        }, Qt::DirectConnection);

        // Before the config is built: a Throne crash or hang while building the boot profile strikes it at the next boot.
        connect(engine->connection(), &ConnectionManager::startDispatching, this, [this](quint64 serial, int profileId) {
            // Once any start is dispatched the stay-up retry no longer resumes: a profile that ran may be what kills the core.
            m_retryResumeId = -1;
            if (m_bootSerial == 0 || serial != m_bootSerial || m_bootPhase != BootPhase::Waiting) return;
            m_bootPhase = BootPhase::Armed;
            m_guard->arm(profileId);
        }, Qt::DirectConnection);
        // Fallback for the same transition; after the ConnectionManager's slot, which dispatched the boot start, or already
        // finished it (held Tun, NotFound).
        connect(supervisor, &CoreSupervisor::coreStarted, this, [this](int profileId) {
            if (m_bootPhase != BootPhase::Waiting || profileId != m_bootProfileId) return;
            m_bootPhase = BootPhase::Armed;
            m_guard->arm(profileId);
        }, Qt::DirectConnection);
        connect(supervisor, &CoreSupervisor::coreExited, this, [this](bool) {
            ++m_runGeneration;
            // A crash stop keeps started_id; a switch or a disconnect has already moved it off the boot profile.
            const int startedId = Configs::dataManager->settingsRepo->started_id;
            if (m_bootPhase == BootPhase::Armed || (m_bootProfileId >= 0 && startedId == m_bootProfileId)) {
                m_guard->strike();
            } else {
                m_guard->disarm();
            }
        }, Qt::DirectConnection);

        connect(engine, &EngineControl::startFinished, this, [this](quint64 serial, int profileId, StartOutcome outcome, const QString &) {
            if (outcome == StartOutcome::Started) {
                noteProfileStarted(profileId);
                m_retryResumeId = -1;
            }
            // The exit's noteExit settles the arm; an outcome the teardown delivers must not disarm what a 70/71 kept.
            if (terminating()) return;
            if (m_bootSerial == 0 || serial != m_bootSerial) return;
            // A give-up ended the boot start before it was dispatched, so the profile cannot be the cause; the stay-up retry resumes it.
            if (outcome == StartOutcome::CoreUnavailable && m_bootPhase == BootPhase::Waiting) m_retryResumeId = profileId;
            if (std::exchange(m_bootPhase, BootPhase::Done) != BootPhase::Armed) return;
            if (outcome != StartOutcome::Started) {
                // The core died under the Start RPC; its exit report usually lands after this.
                const bool coreDied = outcome == StartOutcome::CoreUnavailable &&
                                      (API::defaultClient == nullptr || !API::defaultClient->IsConnected());
                if (coreDied) {
                    m_guard->strike();
                } else {
                    m_guard->disarm();
                }
                return;
            }
            QTimer::singleShot(std::chrono::milliseconds(ResumeGuard::kSurviveMs), this, [this, profileId] {
                // A strike or an exit already disarmed it.
                if (!m_guard->armed()) return;
                const auto running = m_host->runningSnapshot();
                if (running != nullptr && running->id == profileId && m_host->connectionState() == ConnectionState::Running) {
                    noteSurvived(profileId);
                } else {
                    m_guard->disarm();
                }
            });
        }, Qt::DirectConnection);

        connect(engine, &EngineControl::connectionStateChanged, this, [this](ConnectionState state) {
            ++m_runGeneration;
            if (state != ConnectionState::Running) return;
            const auto running = m_host->runningSnapshot();
            if (running == nullptr) return;
            QTimer::singleShot(std::chrono::milliseconds(ResumeGuard::kSurviveMs), this,
                               [this, generation = m_runGeneration, profileId = running->id] {
                                   if (generation == m_runGeneration) noteSurvived(profileId);
                               });
        }, Qt::DirectConnection);
    }

    EngineHost::BootStart HeadlessRuntime::bootStart() {
        EngineHost::BootStart start;
        if (m_bootPhase != BootPhase::None) {
            start.profileId = m_bootProfileId;
            start.serial = m_bootSerial;
            return start;
        }
        m_guard->settlePrevious();
        if (stayUp()) {
            LOG_WARN(QStringLiteral("[Core] The last %1 boots never had a core connected for %2 s, so this one does not exit %3 when "
                                    "the core gives up: it stays up (degraded core_gave_up) for the API and retries the core every %4 min.")
                         .arg(m_guard->unstableBoots())
                         .arg(ResumeGuard::kSurviveMs / 1000)
                         .arg(ExitCode::CoreGaveUp)
                         .arg(kStayUpRetryMs / 60000));
        }
        auto *engine = m_host != nullptr ? m_host : host();
        const int remembered = Configs::dataManager->settingsRepo->remember_id;
        if (engine == nullptr) {
            LOG_WARN("HeadlessRuntime::bootStart() before EngineHost::create(); nothing resumes");
        } else if (!m_options.resume) {
            LOG_INFO("[Resume] Off (--no-resume or THRONE_RESUME); the core starts with no profile.");
        } else if (remembered < 0) {
            LOG_INFO("[Resume] No profile to resume.");
        } else if (const auto profile = Configs::dataManager->profilesRepo->GetProfile(remembered); profile == nullptr) {
            LOG_INFO(QStringLiteral("[Resume] Profile %1 no longer exists; the core starts with no profile.").arg(remembered));
        } else {
            start.profileId = m_guard->decide(remembered);
            if (m_guard->suppressed()) engine->setResumeSuppressed(true);
            if (start.profileId >= 0) {
                start.serial = engine->nextStartSerial();
                const QString name = profile->outbound != nullptr ? profile->outbound->DisplayName() : profile->name;
                LOG_INFO(QStringLiteral("[Resume] Resuming profile %1%2 once the core is up.")
                             .arg(start.profileId)
                             .arg(name.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(name)));
            }
        }
        m_bootProfileId = start.profileId;
        m_bootSerial = start.serial;
        m_bootPhase = start.profileId >= 0 ? BootPhase::Waiting : BootPhase::Done;
        return start;
    }

    RuntimeStatus *HeadlessRuntime::status() const {
        return m_status;
    }

    void HeadlessRuntime::setHadTty(bool hadTty) {
        m_hadTty = hadTty;
    }

    void HeadlessRuntime::setParentRelaysHangup(bool relays) {
        m_parentRelaysHangup = relays;
    }

    void HeadlessRuntime::setInPlaceRestart(bool inPlace) {
        m_inPlaceRestart = inPlace;
    }

    void HeadlessRuntime::abandonReload() {
        if (m_link != nullptr) m_link->abandonReload();
    }

    bool HeadlessRuntime::reloadPending() const {
        return m_link != nullptr && m_link->reloadPending();
    }

    void HeadlessRuntime::inheritReload() {
        if (m_link != nullptr) m_link->inheritReload();
    }

    void HeadlessRuntime::endSession(const QString &reason) {
        terminate(ExitCode::Ok, reason);
        // A running teardown already wrote last_exit (terminate() wrote the stop override); no recorded code: another teardown owns it.
        if (m_terminating || m_exitCode < 0 || headlessRuntimeExiting()) return;
        // The queued runTerminate returns at once; the caller runs the teardown.
        m_terminating = true;
        if (m_stableTimer != nullptr) m_stableTimer->stop();
        if (m_coreRetryTimer != nullptr) m_coreRetryTimer->stop();
        if (m_watchdogTimer != nullptr) m_watchdogTimer->stop();
        const int code = exitCode(m_exitCode);
        m_guard->noteExit(code, m_exitReason);
        const auto line = QStringLiteral("EXIT %1: %2").arg(code).arg(m_exitReason);
        if (code == ExitCode::Ok || code == ExitCode::Restart) {
            LOG_INFO(line);
        } else {
            LOG_WARN(line);
        }
    }

    void HeadlessRuntime::beforeExec() {
        // Posted, so the flag flips inside exec(): an exit() from a loop that ends before exec() starts would be lost.
        QMetaObject::invokeMethod(this, [this] {
            m_execRunning = true;
            if (m_exitCode >= 0) postTerminate();
        }, Qt::QueuedConnection);
        QTimer::singleShot(kHeadlessRuntimeReadyWarnMs, this, [this] {
            if (m_status == nullptr || m_status->ready() || terminating()) return;
            LOG_WARN(QStringLiteral("Not ready after %1 s; waiting for: %2")
                         .arg(kHeadlessRuntimeReadyWarnMs / 1000)
                         .arg(m_status->pendingSteps().join(QStringLiteral(", "))));
        });
        if (m_link != nullptr) m_link->start();
        if (m_watchdogTimer == nullptr) {
            m_watchdogTimer = new QTimer(this);
            m_watchdogTimer->setInterval(kWatchdogTickMs);
            connect(m_watchdogTimer, &QTimer::timeout, this, &HeadlessRuntime::evaluateWatchdog);
            m_watchdogTimer->start();
        }
    }

    void HeadlessRuntime::onSignal(int signum) {
        [[maybe_unused]] const bool fromParent = (signum & Bootstrap::kSignalFromParent) != 0;
        [[maybe_unused]] const bool fromSession = (signum & Bootstrap::kSignalFromSession) != 0;
        signum &= ~(Bootstrap::kSignalFromParent | Bootstrap::kSignalFromSession);
#ifdef Q_OS_UNIX
        if (signum == SIGHUP) {
            // While exiting only a hang-up counts: it turns a recorded restart into a stop, never into the second-signal kill.
            const bool judged = !terminating() || (m_exitCode == ExitCode::Restart && !m_stopOverride);
            // A pty relay (sudo use_pty, su --pty) hangs up while its pty still opens; an outsider's SIGHUP restarts even then.
            const bool hangup = judged && m_hadTty &&
                                ((fromParent && m_parentRelaysHangup) || ((fromSession || fromParent) && headlessRuntimeTtyGone()));
            if (hangup) {
                terminate(ExitCode::Ok, QStringLiteral("SIGHUP (the terminal hung up)"));
            } else if (terminating()) {
                LOG_INFO("SIGHUP ignored: already exiting.");
            } else {
                terminate(ExitCode::Restart, QStringLiteral("SIGHUP"));
            }
            return;
        }
#endif
        if (signum == SIGTERM) {
            stopSignal(QStringLiteral("SIGTERM"));
        } else if (signum == SIGINT) {
            stopSignal(QStringLiteral("SIGINT"));
        } else {
            LOG_DEBUG(QStringLiteral("Signal %1 ignored.").arg(signum));
        }
    }

    void HeadlessRuntime::onStopRequest(bool fast, const QString &source) {
        const bool first = !terminating();
        stopSignal(source);
        // A fast stop whose graceful one never reached the runtime (both held until exec()) still kills the core in the teardown.
        if (fast && first) QMetaObject::invokeMethod(this, [this, source] { stopSignal(source); }, Qt::QueuedConnection);
    }

    void HeadlessRuntime::stopSignal(const QString &name) {
        const bool exiting = headlessRuntimeExiting();
        if (m_exitCode < 0 && !exiting) {
            terminate(ExitCode::Ok, name);
            return;
        }
        if (m_exitCode == ExitCode::Restart && !m_stopOverride) {
            terminate(ExitCode::Ok, name);
            return;
        }
        if (!exiting) {
            // The recorded exit is still queued, and this byte may have drained with the first: judge it inside the teardown.
            if (!std::exchange(m_secondSignalQueued, true)) {
                QMetaObject::invokeMethod(this, [this, name] { stopSignal(name); }, Qt::QueuedConnection);
            }
            return;
        }
        if (std::exchange(m_fastPathUsed, true)) {
            LOG_INFO(QStringLiteral("%1 ignored: the core is already being killed.").arg(name));
            return;
        }
        LOG_WARN(QStringLiteral("%1: second signal: killing the core").arg(name));
        if (auto *engine = host()) engine->supervisor()->killForExit();
    }

    void HeadlessRuntime::terminate(int code, const QString &reason) {
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, [this, code, reason] { terminate(code, reason); }, Qt::QueuedConnection);
            return;
        }
        if (m_exitCode >= 0) {
            if (code == ExitCode::Ok && m_exitCode == ExitCode::Restart && !m_stopOverride) {
                m_stopOverride = true;
                m_exitReason = reason;
                // Once the teardown runs, its noteExit(75) is already written.
                if (m_terminating) {
                    m_guard->noteExit(ExitCode::Ok, reason);
                    LOG_INFO(QStringLiteral("EXIT %1: %2 (instead of the restart in progress)").arg(ExitCode::Ok).arg(reason));
                }
                return;
            }
            LOG_INFO(QStringLiteral("Exit %1 (%2) ignored: already exiting with %3.").arg(code).arg(reason).arg(exitCode(m_exitCode)));
            return;
        }
        if (headlessRuntimeExiting()) {
            LOG_INFO(QStringLiteral("Exit %1 (%2) ignored: a shutdown is already running.").arg(code).arg(reason));
            return;
        }
        m_exitCode = code;
        m_exitReason = reason;
        if (m_execRunning) postTerminate();
    }

    void HeadlessRuntime::postTerminate() {
        if (std::exchange(m_terminatePosted, true)) return;
        QMetaObject::invokeMethod(this, [this] { runTerminate(); }, Qt::QueuedConnection);
    }

    void HeadlessRuntime::runTerminate() {
        if (m_terminating) return;
        if (headlessRuntimeExiting()) {
            LOG_INFO(QStringLiteral("Exit %1 (%2) dropped: a shutdown is already running.").arg(m_exitCode).arg(m_exitReason));
            return;
        }
        m_terminating = true;
        if (m_stableTimer != nullptr) m_stableTimer->stop();
        if (m_coreRetryTimer != nullptr) m_coreRetryTimer->stop();
        if (m_watchdogTimer != nullptr) m_watchdogTimer->stop();
        const int code = exitCode(m_exitCode);
        m_guard->noteExit(code, m_exitReason);
        const auto line = QStringLiteral("EXIT %1: %2").arg(code).arg(m_exitReason);
        if (code == ExitCode::Ok || code == ExitCode::Restart) {
            LOG_INFO(line);
        } else {
            LOG_WARN(line);
        }
        // exit() ends every nested loop on this thread, so it only follows a finished teardown.
        if (auto *engine = host()) engine->shutdown(false);
        QCoreApplication::exit(exitCode(code));
    }

    int HeadlessRuntime::exitCode(int execResult) const {
        if (m_exitCode < 0) return execResult;
        return m_stopOverride ? ExitCode::Ok : m_exitCode;
    }

    bool HeadlessRuntime::terminating() const {
        return m_exitCode >= 0 || headlessRuntimeExiting();
    }

    bool HeadlessRuntime::stayUp() const {
        return m_guard->stayUp();
    }

    void HeadlessRuntime::noteExecRefused(const QString &why) {
        // A stop that arrived during the restart already wrote its own last_exit, and the process exits 0.
        if (m_stopOverride) return;
        m_guard->noteExit(ExitCode::Restart, QStringLiteral("exec restart refused: ") + why);
    }

    QJsonObject HeadlessRuntime::health() const {
        if (!m_execRunning || m_host == nullptr || m_status == nullptr) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("mode"), QStringLiteral("headless")},
                {QStringLiteral("ready"), false},
                {QStringLiteral("state"), QStringLiteral("starting")},
                {QStringLiteral("pid"), QCoreApplication::applicationPid()},
                {QStringLiteral("uptime_s"), (MonotonicMs() - m_bootMonoMs) / 1000},
                {QStringLiteral("version"), QStringLiteral(NKR_VERSION)},
                {QStringLiteral("headless_dialogs"), HeadlessDialogCount()},
            };
        }
        auto reply = m_host->statusJson();
        const auto fields = m_status->healthFields();
        for (auto it = fields.begin(); it != fields.end(); ++it) reply.insert(it.key(), it.value());
        const auto previousExit = m_guard->previousExit();
        reply.insert(QStringLiteral("previous_exit"), previousExit.isEmpty() ? QJsonValue() : QJsonValue(previousExit));
        reply.insert(QStringLiteral("api"), HeadlessApi::StatusBlock());
        // "tun" is the requested mode; this is what the running config does (a refused "tun off", a custom config's tun inbound).
        reply.insert(QStringLiteral("tun_running"), m_host->runningTun());
        return reply;
    }

    void HeadlessRuntime::noteSurvived(int profileId) {
        const bool wasSuppressed = m_guard->suppressed();
        m_guard->survived(profileId);
        m_guard->liftSuppression(profileId);
        if (wasSuppressed && !m_guard->suppressed() && m_host != nullptr) m_host->setResumeSuppressed(false);
    }

    void HeadlessRuntime::noteProfileStarted(int profileId) {
        if (!m_guard->suppressed()) return;
        m_guard->liftSuppression(profileId);
        if (!m_guard->suppressed() && m_host != nullptr) m_host->setResumeSuppressed(false);
    }

    void HeadlessRuntime::onCoreGaveUp(const QString &reason) {
        if (!stayUp()) {
            terminate(ExitCode::CoreGaveUp, QStringLiteral("the core gave up: %1").arg(reason));
            return;
        }
        if (terminating()) return;
        LOG_ERROR(QStringLiteral("[Core] %1; staying up so the API stays reachable (the last %2 boots never got a working core); "
                                 "retrying the core every %3 min")
                      .arg(reason, QString::number(m_guard->unstableBoots()), QString::number(kStayUpRetryMs / 60000)));
        // Settled as far as it gets: summary() reports the degradation and readiness does not wait for a core that may never come.
        if (m_status != nullptr) {
            m_status->mark(RuntimeStatus::CoreStep);
            m_status->mark(RuntimeStatus::ResumeStep);
        }
        m_coreRetryTimer->start();
    }

    void HeadlessRuntime::evaluateWatchdog() {
        if (terminating() || m_host == nullptr || m_status == nullptr) return;
        const qint64 connectingMs = m_status->connectingMs();
        const qint64 stoppingMs = m_status->stoppingMs();
        // Sampled on every tick: the first unusable sample starts its clock.
        const qint64 unusableMs = m_status->coreUnusableMs();
        if (connectingMs > kConnectingLimitMs) {
            const QString name = headlessRuntimeProfileName(nullptr, m_host->connection()->lastProfileId());
            stuck(QStringLiteral("connecting to %1 for %2 s").arg(name).arg(connectingMs / 1000), ExitCode::Stuck);
        } else if (stoppingMs > kStoppingLimitMs) {
            const QString name = headlessRuntimeProfileName(m_host->runningSnapshot(), m_host->connection()->lastProfileId());
            stuck(QStringLiteral("stopping %1 for %2 s").arg(name).arg(stoppingMs / 1000), ExitCode::Stuck);
        } else if (unusableMs > kCoreUnusableLimitMs) {
            stuck(QStringLiteral("the core has not been usable for %1 s").arg(unusableMs / 1000), ExitCode::CoreGaveUp);
        } else {
            m_stuckEpisode = false;
        }
    }

    void HeadlessRuntime::stuck(const QString &reason, int code) {
        if (terminating()) return;
        if (!stayUp()) {
            terminate(code, reason);
            return;
        }
        if (std::exchange(m_stuckEpisode, true)) return;
        LOG_ERROR(QStringLiteral("[Watchdog] %1; staying up (stay-up mode), resetting the core").arg(reason));
        // As a stay-up give-up: readiness must not wait for a core this reset may never bring back.
        m_status->mark(RuntimeStatus::CoreStep);
        m_status->mark(RuntimeStatus::ResumeStep);
        m_host->supervisor()->resetCore();
    }
} // namespace Engine
