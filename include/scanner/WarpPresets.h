#pragma once

#include <QList>
#include <QStringList>

// Seeds for editable IP lists, not probe constants: Cloudflare changes them silently.
namespace Scanner::WarpPresets {
    inline const QStringList kWireGuardRanges = {
        QStringLiteral("162.159.192.0/24"),
        QStringLiteral("162.159.195.0/24"),
        QStringLiteral("188.114.96.0/22"),
        QStringLiteral("2606:4700:d0::/64"),
        QStringLiteral("2606:4700:d1::/64"),
    };
    inline const QList<int> kWireGuardPorts = {2408, 500, 1701, 4500};
    // The documented four plus the ports WARP clients have found answering since.
    inline const QList<int> kWireGuardAllPorts = {
        500,  854,  859,  864,  878,  880,  890,  891,  894,  903,  908,  928,  934,  939,  942,  943,  945,  946,
        955,  968,  987,  988,  1002, 1010, 1014, 1018, 1070, 1074, 1180, 1387, 1701, 1843, 2371, 2408, 2506, 3138,
        3476, 3581, 3854, 4177, 4198, 4233, 4500, 5279, 5956, 7103, 7152, 7156, 7281, 7559, 8319, 8742, 8854, 8886,
    };

    // Every sampled address serves MASQUE over TCP and QUIC (2026-11-09), except .1 of each /24, a plain CDN edge over TCP.
    inline const QStringList kMasqueEndpoints = {
        QStringLiteral("162.159.198.0/24"),
        QStringLiteral("162.159.199.0/24"),
        QStringLiteral("2606:4700:103::1"),
        QStringLiteral("2606:4700:103::2"),
        QStringLiteral("2606:4700:104::1"),
        QStringLiteral("2606:4700:104::2"),
    };
    // HTTP/3 over UDP and HTTP/2 over TCP alike.
    inline const QList<int> kMasquePorts = {443, 500, 1701, 4500, 4443, 8443, 8095};
} // namespace Scanner::WarpPresets
