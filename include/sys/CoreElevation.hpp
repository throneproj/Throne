#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QtGlobal>

#include <functional>

class QObject;

// Windows: Throne runs as a standard user while ThroneCore (the core and --guard) runs elevated through a per-user scheduled
// task that a one-time UAC grant registers, the counterpart of the setuid core on Linux and macOS. Elsewhere a no-op.
namespace Sys::CoreElevation {
    enum class Status {
        Unsupported,
        NotGranted,
        // Granted for a ThroneCore.exe that has changed since (an update): it needs a new grant.
        Stale,
        Granted,
    };

    // The task path ThroneCore's unelevated stub runs; set only when the core should start elevated.
    inline constexpr const char *kTaskEnv = "THRONE_ELEVATION_TASK";

    // Exit codes of the elevated helper mode; -1 from Grant()/Revoke()/RunUpdater() means UAC was declined.
    enum HelperExit : int {
        HelperOk = 0,
        HelperNotElevated = 2,
        HelperWrongUser = 3,
        HelperBadArguments = 4,
        HelperCopyFailed = 10,
        HelperTaskFailed = 11,
        HelperRemoveFailed = 12,
        HelperUpdaterFailed = 13,
    };

#ifdef Q_OS_WIN
    // Cached after the first call; any thread.
    Status CurrentStatus();
    void Invalidate();
    // A split-token administrator whose Throne is not elevated: the grant elevates this same account.
    bool CanGrant();
    // Throne.exe's directory accepts new files (an unelevated updater can replace it).
    bool InstallDirWritable();
    // Always removes kTaskEnv, then sets it when Throne is not elevated and the grant is current.
    void ApplyEnvironment(QProcessEnvironment &env);
    // Each privilege report of the core: whether that core was started through the grant, and whether it is privileged.
    void NoteCoreReport(bool throughGrant, bool privileged);
    // The grant is current, yet the last core started through it is not privileged: granting again cannot help.
    bool GrantIneffective();
    // UAC, then the elevated helper; blocks until the helper exits. A HelperExit, or -1 when UAC was declined.
    int Grant();
    int Revoke();
    // Grant() on a worker thread; done runs on context's thread unless context is gone.
    void GrantAsync(QObject *context, std::function<void(int code)> done);
    // UAC, then the helper copies zipPath into the install dir and starts its updater elevated once guiPid exits; returns
    // without waiting for that. A HelperExit, or -1 when UAC was declined.
    int RunUpdater(const QString &zipPath, qint64 guiPid);
    QString ResultText(int code);
    // main(): argv names the helper mode; RunHelper's result is then the process exit code.
    bool IsHelperInvocation(int argc, char *argv[]);
    int RunHelper(int argc, char *argv[]);

    // A process Throne did not start itself (the elevated core), opened for SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION so
    // its pid cannot be reused while held. nullptr on failure. Any thread.
    void *OpenWatch(qint64 pid);
    // True once the process has exited; false when it still runs after waitMs (0 polls).
    bool WaitWatch(void *watch, int waitMs);
    void CloseWatch(void *watch);
#else
    inline Status CurrentStatus() { return Status::Unsupported; }
    inline void Invalidate() {}
    inline bool CanGrant() { return false; }
    inline bool InstallDirWritable() { return true; }
    inline void ApplyEnvironment(QProcessEnvironment &env) { env.remove(QString::fromLatin1(kTaskEnv)); }
    inline void NoteCoreReport(bool, bool) {}
    inline bool GrantIneffective() { return false; }
    inline void *OpenWatch(qint64) { return nullptr; }
    inline bool WaitWatch(void *, int) { return true; }
    inline void CloseWatch(void *) {}
#endif
} // namespace Sys::CoreElevation
