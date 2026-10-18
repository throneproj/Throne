#pragma once

#include <functional>

#include <QProcessEnvironment>
#include <QString>

// What a service manager (systemd, the Windows SCM) needs from a headless Throne, and what its children must not inherit.
// The Notify* calls map to sd_notify on Linux and to SetServiceStatus under the SCM (--service); no-ops otherwise.
namespace Sys::ServiceManager {
    // A copy without NOTIFY_SOCKET, WATCHDOG_USEC, WATCHDOG_PID, LISTEN_PID, LISTEN_FDS, LISTEN_FDNAMES, CREDENTIALS_DIRECTORY
    // and every THRONE_* variable except the ones the core reads (THRONE_CORE_SOCKET, THRONE_CORE_DEBUG, THRONE_GUARD).
    // Never touches this process's own environment: an exec restart needs it.
    QProcessEnvironment ChildEnvironment(QProcessEnvironment env = QProcessEnvironment::systemEnvironment());

    // Linux with a usable NOTIFY_SOCKET ("/path" or "@abstract"), or Windows under the SCM; false otherwise. Main thread only,
    // like everything below unless noted.
    bool NotifyEnabled();

    // READY=1 + STATUS=<status>. status must be one line.
    void NotifyReady(const QString &status);

    void NotifyStatus(const QString &status);

    // STOPPING=1 + STATUS=<status>.
    void NotifyStopping(const QString &status);

    // RELOADING=1 + MONOTONIC_USEC=<now> + STATUS=<status>: an in-place exec restart; the new image's READY=1 ends it. STOPPING
    // there would make systemd ignore that READY and kill the new image at the stop timeout.
    void NotifyReloading(const QString &status);

    void NotifyWatchdog();

    // EXTEND_TIMEOUT_USEC=<usec>: the start (or stop) timeout is extended by that much from now.
    void NotifyExtendTimeout(qint64 usec);

    // WATCHDOG_USEC when set and WATCHDOG_PID is unset or this process; 0 otherwise.
    qint64 WatchdogUsec();

    // Windows SCM: START_PENDING checkpoint++ (after the lock, initDB and EngineHost::init); no-op elsewhere.
    void NotifyStartProgress();

    // Windows SCM: SERVICE_STOPPED with code (ERROR_SERVICE_SPECIFIC_ERROR when non-zero), but NO_ERROR/0 after a stop the SCM
    // asked for (STOP, PRESHUTDOWN, SHUTDOWN), so recovery never undoes it. main() calls it as its very last
    // statement, after RunHeadless returned and destroyed the app, the lock and the presence handle; no-op elsewhere.
    void NotifyStopped(int exitCode);

    // Windows: the process runs as LocalSystem; false elsewhere.
    bool RunningAsLocalSystem();

    // Windows with service or LocalSystem: %ProgramData%\Throne; empty otherwise (the caller's default chain goes on).
    QString SystemDataDir(bool service);

    // Windows, for the SystemDataDir default only: a new dir gets the protected DACL O:SYG:SYD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA);
    // an existing one, opened without following links, must be owned by SYSTEM, Administrators or this account and must not
    // let any other account write to it; it gets that DACL again, then every entry under it must be neither a link nor a
    // reparse point and have such an owner. Otherwise false with *error naming the owner or the relative entry (the caller
    // exits 78); nothing is re-owned. True elsewhere.
    bool SecureDataDir(const QString &path, bool created, QString *error);

    // Windows: which of Users, Authenticated Users or Everyone may create or write files in path (its DACL), for a warning
    // about an explicit data dir; empty when none or off Windows.
    QString UsersCanWriteDataDir(const QString &path);

    // The other end of a connected local socket (QLocalSocket::socketDescriptor(): the pipe handle on Windows). Server side:
    // the client runs as SYSTEM or a member of Administrators. True off Windows.
    bool LocalClientIsAdmin(qintptr descriptor);

    // Client side: the pipe's server runs as this user or SYSTEM (named-pipe spoofing of --health/--print-connection); *why
    // says otherwise. A server process the caller cannot open is trusted only when the caller is neither elevated nor SYSTEM
    // and the pipe's DACL lets no account but SYSTEM, Administrators and the caller add instances or rewrite it. True off Windows.
    bool PipeServerTrusted(qintptr descriptor, QString *why);

#ifdef Q_OS_WIN
    enum class StopKind { Graceful, Fast };

    // Right after PreScan with --service: runs StartServiceCtrlDispatcherW on a helper thread and waits up to waitMs for
    // ServiceMain. False when not started by the SCM (ERROR_FAILED_SERVICE_CONTROLLER_CONNECT): the caller exits 64.
    bool StartServiceDispatcher(int waitMs = 5000);

    bool IsService();

    // This process runs with an elevated token (TokenElevation).
    bool SelfElevated();

    // After the app object; handler runs on the main thread (queued). A stop seen earlier is held until ArmStopHandler().
    void SetStopHandler(std::function<void(StopKind)> handler);

    // Right before exec(): replays a held stop (exit() before exec() is a no-op).
    void ArmStopHandler();

    // Non-service only: a console ctrl handler when a console is attached (CTRL_C/BREAK -> Graceful, again -> Fast;
    // CTRL_CLOSE/LOGOFF/SHUTDOWN -> Graceful, then waits up to 4.5 s for SignalShutdownComplete) and a hidden top-level window
    // (WM_CLOSE -> Graceful; WM_ENDSESSION -> sessionEnd() synchronously on the main thread).
    void InstallShutdownSignals(std::function<void()> sessionEnd);

    // Any thread, once the teardown finished: releases a console CLOSE/LOGOFF/SHUTDOWN handler that is waiting.
    void SignalShutdownComplete();
#endif
} // namespace Sys::ServiceManager
