#pragma once

#include <optional>

#include <QString>

// A host-wide marker that a headless Throne runs, whatever its data dir: the GUI warns, refuses Tun and refuses a self-update
// from the same install while one lives. Windows: a named file mapping Global\ThroneHeadless (Local\ThroneHeadless
// without SeCreateGlobalPrivilege) holding the same JSON; it dies with its last handle, so it never goes stale.
namespace Sys::HostPresence {
    struct Holder {
        qint64 pid = 0;
        QString installDir;
        QString dataDir;
        QString version;
        // Seconds since the epoch.
        qint64 started = 0;
    };

    // Headless, right after the data-dir lock. euid 0: /run/throne/headless.lock (Linux) or /var/run/throne/headless.lock
    // (macOS); otherwise $XDG_RUNTIME_DIR/throne/headless.lock; otherwise none (logged). flock LOCK_EX|LOCK_NB on a CLOEXEC fd
    // held for the process lifetime (released by exit and by an exec restart), then the JSON written over the file. The file
    // is never unlinked. Windows: the mapping, created once and held for the process lifetime. False (logged, never fatal) when
    // another headless holds it or no location works. Main thread.
    bool Publish(const QString &dataDir);

    // The live headless instance on this host other than this process, from the root location and the caller's
    // $XDG_RUNTIME_DIR one; a LOCK_SH|LOCK_NB probe that succeeds means nobody holds it. Windows: Global, then Local mapping.
    // Never blocks; nullopt when none.
    std::optional<Holder> OtherHeadless();

    // Clean paths; case-insensitive on Windows.
    bool SameInstall(const Holder &holder, const QString &installDir);
} // namespace Sys::HostPresence
