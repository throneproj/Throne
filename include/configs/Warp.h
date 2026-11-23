#pragma once
#include <QString>

namespace Configs
{
    // The stored identity of the built-in WARP's mode ("wireguard", "amnezia" or "masque") is complete enough to build.
    bool WarpConfigured(const QString &mode);

    // Accepts host, host:port, [v6] and [v6]:port; a bare IPv6 address takes defaultPort.
    void SplitWarpEndpoint(const QString &endpoint, int defaultPort, QString &host, int &port);
}
