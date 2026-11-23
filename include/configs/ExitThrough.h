#pragma once
#include <QString>

namespace Configs
{
    // The exit_through setting: what proxied traffic finally leaves through; every exit but Server wraps the selected profile.
    enum class ExitThrough { Server, Warp, Tor, Psiphon };

    // The exit wrapper's placeholder in a hop list, and the WARP exit's profile id; each exit's profile has its own id for traffic stats.
    constexpr int warpProfileID = -2408;
    constexpr int torExitProfileID = -2409;
    constexpr int psiphonExitProfileID = -2410;

    // Unknown values read as Server.
    inline ExitThrough ExitOf(const QString &value)
    {
        if (value == QLatin1String("warp")) return ExitThrough::Warp;
        if (value == QLatin1String("tor")) return ExitThrough::Tor;
        if (value == QLatin1String("psiphon")) return ExitThrough::Psiphon;
        return ExitThrough::Server;
    }

    inline QString ExitValue(ExitThrough exit)
    {
        switch (exit) {
            case ExitThrough::Warp: return QStringLiteral("warp");
            case ExitThrough::Tor: return QStringLiteral("tor");
            case ExitThrough::Psiphon: return QStringLiteral("psiphon");
            case ExitThrough::Server: break;
        }
        return QStringLiteral("server");
    }

    // A proper name, never translated; empty for Server.
    inline QString ExitName(ExitThrough exit)
    {
        switch (exit) {
            case ExitThrough::Warp: return QStringLiteral("WARP");
            case ExitThrough::Tor: return QStringLiteral("Tor");
            case ExitThrough::Psiphon: return QStringLiteral("Psiphon");
            case ExitThrough::Server: break;
        }
        return {};
    }

    // Server for an id no exit's profile carries.
    inline ExitThrough ExitOfProfileID(int profileId)
    {
        switch (profileId) {
            case warpProfileID: return ExitThrough::Warp;
            case torExitProfileID: return ExitThrough::Tor;
            case psiphonExitProfileID: return ExitThrough::Psiphon;
            default: return ExitThrough::Server;
        }
    }
}
