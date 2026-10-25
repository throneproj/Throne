#pragma once

#include <QString>

// Per-platform. Both return what failed (empty on success) and may be called from any thread.

// windowsFormat is the Proxy Format setting, e.g. "{ip}:{port}" or "socks={ip}:{port}"; other platforms ignore it.
QString SystemProxy_Apply(const QString &host, int port, const QString &windowsFormat);

// Undoes owned SystemProxy_Apply() changes. Without recorded ownership, proxies set by other apps survive.
QString SystemProxy_Clear();

#ifdef Q_OS_MACOS
// Replays an interrupted macOS proxy transaction after acquiring the per-user owner lock.
// Force Quit cannot execute cleanup; this recovers on the next GUI launch, not while Throne is stopped.
QString SystemProxy_Recover();
#endif
