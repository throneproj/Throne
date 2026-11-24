#pragma once
#include <QString>

namespace Configs
{
    // The exit_through setting: what proxied traffic finally leaves through; every exit but Server wraps the selected profile.
    enum class ExitThrough { Server, Warp };

    // The exit wrapper's placeholder in a hop list, and the WARP exit's profile id for traffic stats.
    constexpr int warpProfileID = -2408;

    // Unknown values read as Server.
    inline ExitThrough ExitOf(const QString &value)
    {
        if (value == QLatin1String("warp")) return ExitThrough::Warp;
        return ExitThrough::Server;
    }

    inline QString ExitValue(ExitThrough exit)
    {
        switch (exit) {
            case ExitThrough::Warp: return QStringLiteral("warp");
            case ExitThrough::Server: break;
        }
        return QStringLiteral("server");
    }

    // A proper name, never translated; empty for Server.
    inline QString ExitName(ExitThrough exit)
    {
        switch (exit) {
            case ExitThrough::Warp: return QStringLiteral("WARP");
            case ExitThrough::Server: break;
        }
        return {};
    }

    // Server for an id no exit's profile carries.
    inline ExitThrough ExitOfProfileID(int profileId)
    {
        switch (profileId) {
            case warpProfileID: return ExitThrough::Warp;
            default: return ExitThrough::Server;
        }
    }
}
