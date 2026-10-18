#pragma once

#include <QtGlobal>

namespace Engine {
    enum class ConnectionState {
        Idle,
        Connecting,
        Running,
        Stopping,
    };

    enum class StartOutcome {
        Started,
        Exiting,
        NotFound,
        GroupUnavailable,
        KillSwitchInactive,
        Superseded,
        CoreUnavailable,
        BuildFailed,
        Busy,
        ExtraCoreBlocked,
        GeoAssetsMissing,
        StrictRouteUnavailable,
        TunFailed,
        StartFailed,
    };

    struct StartRequest {
        int profileId = -1;
        // Unattended callers get every failure through startFinished and the log, never a dialog.
        bool interactive = true;
        quint64 serial = 0;
    };

    struct StopOptions {
        bool crash = false;
        bool block = false;
        bool manual = false;
        bool interactive = true;
        // 0: the channel default (30 s).
        int rpcTimeoutMs = 0;
        // Skip StopTests in the stop.
        bool skipTests = false;
    };

    struct ModeChange {
        bool save = true;
        // Unattended callers get no dialog or elevation prompt, and their restart is an unattended StartRequest carrying restartSerial.
        bool interactive = true;
        bool restart = true;
        quint64 restartSerial = 0;
    };

    // Where the traffic chart marks a run.
    enum class RunMarker {
        Started,
        Switched,
        Stopped,
    };

    // Why the running profile should restart to apply a change; the view translates it.
    enum class RestartReason {
        Settings,
        Routing,
        Profile,
        Group,
    };

    enum class ExitReason {
        None,
        RunUpdater,
        Restart,
        RestartWithTun,
        RestartElevated,
    };

    // Bytes per second.
    struct LiveRates {
        qint64 proxyUp = 0;
        qint64 proxyDown = 0;
        qint64 directUp = 0;
        qint64 directDown = 0;
    };

    enum class TestKind {
        Latency,
        Ip,
        Speed,
    };
} // namespace Engine
