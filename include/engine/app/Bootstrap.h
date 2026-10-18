#pragma once

#include <functional>

#include <QDir>
#include <QString>
#include <QStringList>

namespace Engine {
    class DataDirLock;
}

namespace Engine::Bootstrap {
    enum class Mode {
        Gui,
        DumpConfigs,
        Headless,
        // --health / --print-connection / --check: ask a running headless instance or inspect the host, never own the data dir.
        HeadlessClient,
    };

    struct Launch {
        Mode mode = Mode::Gui;
        // --service, recorded for the Windows service mode.
        bool service = false;
    };

    // Runs before any Qt object exists, so it only looks at raw argv and the environment. -dump-configs wins; --health,
    // --print-connection or --check (one or two dashes) is HeadlessClient even without --headless; --headless/-headless or a truthy
    // THRONE_HEADLESS (not ""/0/false/no/off) is Headless.
    Launch PreScan(int argc, char **argv);

    // software_name and software_core_name; before anything titles a dialog or notice.
    void SetSoftwareNames();

    struct DataDir {
        QDir wd;
        bool useAppdata = false;
        // As given after -appdata; empty when the flag had no value.
        QString appdataDir;
        QString configDir;
    };

    // Sets the application name first, since AppConfigLocation depends on it. Creates wd and wd/config; a relative -appdata resolves against relativeBase.
    DataDir ResolveDataDir(const QStringList &arguments, const QDir &relativeBase);

    QString InstanceServerName(const QDir &wd);

    // An explicit -appdata dir used to keep its Xray geo assets in AppConfigLocation; copies the ones it lacks.
    void AdoptLegacyGeoAssets(const DataDir &dataDir);

    // The instance that relaunched us (-wait_pid=); it keeps the data dir lock and the socket until it exits.
    class RelaunchParent {
    public:
        // Stripped on every OS so the flag never lands in the saved argv; Windows also waits up to 30 s for the exit.
        explicit RelaunchParent(QStringList &arguments);
        ~RelaunchParent();

        RelaunchParent(const RelaunchParent &) = delete;
        RelaunchParent &operator=(const RelaunchParent &) = delete;

        [[nodiscard]] bool relaunched() const { return pid > 0; }

        // Once it is gone, whoever holds the lock is another instance, so the normal hand-off applies again.
        [[nodiscard]] bool gone() const;

        qint64 pid = 0;

    private:
#ifdef Q_OS_WIN
        void *handle = nullptr;
#endif
    };

    void WarnDroppedPayload(const QStringList &payload);

    enum class ClaimOutcome {
        Owned,
        HandedOff,
        HeldByOther,
        // A headless instance owns the data dir; the GUI tells the user instead of handing off.
        HeldByHeadless,
    };

    struct Claim {
        ClaimOutcome outcome = ClaimOutcome::Owned;
        qint64 ownerPid = 0;
    };

    // Owned also covers a lock that could not be taken at all: that run continues unlocked, as before.
    // handOff false (headless): Busy is HeldByOther right after the relaunch wait and no payload is handed off; the legacy probe
    // answering (HandedOff) then means an older lock-less Throne owns the dir. handOff true (GUI): a busy lock whose owner mode is
    // "headless" is HeldByHeadless at once; at the hand-off deadline a HELLO answered by a headless owner is HeldByHeadless too.
    Claim ClaimDataDir(DataDirLock &lock, const DataDir &dataDir, const QString &serverName, const QString &lockMode,
                       const QStringList &payload, const RelaunchParent &relaunchParent, bool handOff = true);

    // Windows: when stdout/stderr are not valid handles, AttachConsole(ATTACH_PARENT_PROCESS) and reopen them on CONOUT$.
    // A no-op elsewhere.
    void AttachParentConsole();

    // CWD to <config>, temp wipe, legacy geo assets, start epoch, initDB, log level, stats DB, launch flags.
    void OpenDataDir(const DataDir &dataDir, const QStringList &arguments);

    // Bundled Qt plugins (TLS backends) and the DS_cores thread the core process lives on.
    void PrepareRuntime();

    // PrepareRuntime's plugin half alone (--check probes TLS without starting DS_cores).
    void AddBundledPluginPath();

    // Must follow the lock: Init truncates the log and writes the crash marker.
    void StartLogging(const DataDir &dataDir);

    // False before initDB.
    bool ExitInProgress();

    // Or'ed into the signum ArmSignalHandlers passes on for a SIGHUP whose sender is the parent we booted with.
    inline constexpr int kSignalFromParent = 0x80;
    // Or'ed in for a SIGHUP from the kernel (a terminal hang-up) or from a process of the session we booted in (its shells).
    inline constexpr int kSignalFromSession = 0x40;

#ifdef Q_OS_UNIX
    struct SignalSet {
        // SIGHUP as well as SIGTERM/SIGINT.
        bool hangup = false;
        // A SIGHUP or SIGINT that was SIG_IGN at install (nohup, a background job) stays ignored; SIGTERM is always caught.
        bool keepInheritedIgnore = false;
    };

    // False when pipe() failed; directFallback (may be null) is then installed as the plain handler. The handler keeps errno.
    bool InstallTerminationHandlers(void (*directFallback)(int), SignalSet set = {});

    void ArmTerminationHandlers(std::function<void()> onSignal);

    // Every caught signal once, in arrival order (each drained byte); never disarms and ignores ExitInProgress. Main thread.
    void ArmSignalHandlers(std::function<void(int signum)> onSignal);

    // Main thread, once no notifier reads the pipe (after exec() returned): the signals still in it, as ArmSignalHandlers
    // would pass them on. Never blocks.
    QList<int> TakePendingSignals();

#endif

    // The short command name of a live process (/proc/<pid>/comm on Linux, proc_name() on macOS, the image file name on
    // Windows); empty when it is gone or unknown.
    QString ProcessName(qint64 pid);
} // namespace Engine::Bootstrap
