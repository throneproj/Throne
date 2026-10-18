#pragma once

#include <memory>

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

#include "include/engine/EngineHost.h"
#include "include/engine/app/Bootstrap.h"
#include "include/engine/headless/HeadlessCli.h"

namespace Engine {
    class EnginePolicy;
    class HeadlessPolicy;
    class ResumeGuard;
    class RuntimeStatus;
    class SupervisorLink;

    // Exit decisions, signals, boot resume and HEALTH of the headless process. Main thread only.
    class HeadlessRuntime final : public QObject {
        Q_OBJECT

    public:
        // Loads <config>/headless-state.json, so construct it after StartLogging/OpenDataDir.
        HeadlessRuntime(const HeadlessCli::LaunchOptions &options, const Bootstrap::DataDir &dataDir, qint64 bootMonoMs,
                        QObject *parent = nullptr);

        // host()->detachFrontEnd(); current() is null afterwards.
        ~HeadlessRuntime() override;

        static HeadlessRuntime *current();

        // The headless policy's exits land here. Without a runtime (the relay fallback in a GUI process): the prepare_exit
        // check, host()->shutdown(false) and QCoreApplication::exit(code).
        static void RequestExit(int code, const QString &reason);

        // The owned, stateless HeadlessPolicy.
        EnginePolicy *policy();

        // After EngineHost::create(), before init(): coreGaveUp -> onCoreGaveUp (terminate(70), or stay up when stayUp()), the
        // listenFailed log, RuntimeStatus, the stable/retry timers and the ResumeGuard wiring (startDispatching, coreStarted,
        // startFinished, coreExited, connectionStateChanged).
        void attach(EngineHost *host);

        // Before init(): candidate = options.resume ? remember_id : -1 (remember_enable ignored) -> ResumeGuard::decide;
        // serial = host->nextStartSerial() when a profile resumes. A suppression sets host->setResumeSuppressed(true).
        EngineHost::BootStart bootStart();

        RuntimeStatus *status() const;

        // Whether a terminal was the controlling tty at boot; once it is gone, a SIGHUP from the kernel, a process of the boot session
        // or the boot parent is a hang-up, which stops (a shell of that session running kill -HUP counts too); anyone else's restarts.
        void setHadTty(bool hadTty);

        // The parent gives us a pty of its own and relays the terminal's hang-up (sudo, su, doas, run0, script): a SIGHUP it
        // sends is a hang-up too, although our tty still opens.
        void setParentRelaysHangup(bool relays);

        // HeadlessMain, before attach(): the effective restart mode is exec, so an exit 75 restarts in place (SupervisorLink then
        // sends RELOADING, not STOPPING).
        void setInPlaceRestart(bool inPlace);

        // After exec() returned, when the in-place restart did not exec: SupervisorLink::abandonReload().
        void abandonReload();

        // A RELOADING went out that no READY ended yet (the exec restart carries it to the next image).
        bool reloadPending() const;

        // The image an exec restart started while the unit was reloading: its READY or its exit must end that reload.
        void inheritReload();

        // WM_ENDSESSION, main thread: terminate(0, reason), then write last_exit at once; the caller runs the teardown itself.
        void endSession(const QString &reason);

        // Right before exec(): recorded terminate() calls run from the loop from now on; starts SupervisorLink and the progress
        // watchdog (Connecting > 180 s or Stopping > 90 s -> 71, core unusable > 60 s -> 70; in stay-up mode a core reset instead).
        void beforeExec();

        // From the armed self-pipe notifier; returns at once.
        void onSignal(int signum);

        // Windows stop sources (console ctrl, the hidden window, the SCM), main thread: as SIGTERM/SIGINT, source names the
        // reason; fast takes the second-signal path (the core is killed at once once the teardown runs).
        void onStopRequest(bool fast, const QString &source);

        // Queued; the first wins; a stop (0) during a 75 teardown turns the result into 0. Ignored once prepare_exit is set,
        // apart from that override. Body: guard noteExit, log "EXIT <code>: <reason>", host()->shutdown(false),
        // QCoreApplication::exit(code).
        void terminate(int code, const QString &reason);

        // The recorded code (with the stop override), else execResult.
        int exitCode(int execResult) const;

        bool terminating() const;

        // The boot streak reached ResumeGuard::kUnstableBootsToStayUp: a core give-up (and the 4B watchdog) keeps the process up,
        // degraded, with a slow core retry, instead of exiting 70/71.
        bool stayUp() const;

        // After exec() returned, when the exec restart could not happen: last_exit becomes {75, "exec restart refused: <why>"}, so
        // the next boot's previous_exit explains why nothing restarted.
        void noteExecRefused(const QString &why);

        // Before beforeExec(): the "starting" object; afterwards statusJson + healthFields + previous_exit + api.
        QJsonObject health() const;

    private:
        enum class BootPhase : quint8 { None, Waiting, Armed, Done };

        void postTerminate();

        void runTerminate();

        void stopSignal(const QString &name);

        void noteSurvived(int profileId);

        void noteProfileStarted(int profileId);

        void onCoreGaveUp(const QString &reason);

        // Every kWatchdogTickMs from beforeExec(); skipped while terminating.
        void evaluateWatchdog();

        // A progress limit was passed: terminate(code, reason), or in stay-up mode one LOG_ERROR and resetCore() per episode.
        void stuck(const QString &reason, int code);

        HeadlessCli::LaunchOptions m_options;
        Bootstrap::DataDir m_dataDir;
        qint64 m_bootMonoMs;
        std::unique_ptr<HeadlessPolicy> m_policy;
        std::unique_ptr<ResumeGuard> m_guard;
        RuntimeStatus *m_status = nullptr;
        EngineHost *m_host = nullptr;
        bool m_hadTty = false;
        bool m_parentRelaysHangup = false;
        bool m_inPlaceRestart = false;
        bool m_execRunning = false;
        bool m_terminating = false;
        bool m_terminatePosted = false;
        bool m_stopOverride = false;
        bool m_fastPathUsed = false;
        int m_exitCode = -1;
        QString m_exitReason;
        int m_bootProfileId = -1;
        quint64 m_bootSerial = 0;
        BootPhase m_bootPhase = BootPhase::None;
        bool m_secondSignalQueued = false;
        // Bumped by every connection state change and unexpected core exit; a survival check from an older value is stale.
        quint64 m_runGeneration = 0;
        // Single-shot, kSurviveMs after each core connection: still connected -> ResumeGuard::markStable().
        QTimer *m_stableTimer = nullptr;
        // Stay-up mode after a give-up: retries the core every kStayUpRetryMs while it is GaveUp.
        QTimer *m_coreRetryTimer = nullptr;
        static constexpr int kStayUpRetryMs = 5 * 60 * 1000;
        static constexpr int kWatchdogTickMs = 5000;
        static constexpr qint64 kConnectingLimitMs = 180000;
        static constexpr qint64 kStoppingLimitMs = 90000;
        static constexpr qint64 kCoreUnusableLimitMs = 60000;
        QTimer *m_watchdogTimer = nullptr;
        // A stuck() episode already acted; cleared once no limit is passed.
        bool m_stuckEpisode = false;
        SupervisorLink *m_link = nullptr;
        // The boot profile whose start a give-up ended before it was dispatched; the stay-up retry starts it once the core is back.
        int m_retryResumeId = -1;
    };
} // namespace Engine
