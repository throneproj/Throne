#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <QList>
#include <QObject>
#include <QString>

#include "include/engine/EngineTypes.h"

namespace Configs {
    class Profile;
}

namespace Engine {
    // What the Throne API drives; EngineHost implements it. Main thread only unless noted.
    class EngineControl : public QObject {
        Q_OBJECT

    public:
        using QObject::QObject;

        virtual ConnectionState connectionState() const = 0;

        // Safe from any thread.
        virtual std::shared_ptr<Configs::Profile> runningSnapshot() const = 0;

        virtual int resolveLastProfile() = 0;

        virtual quint64 nextStartSerial() = 0;

        // startFinished fires for every request, deferred and superseded ones included.
        virtual void start(const StartRequest &request) = 0;

        virtual void stop(const StopOptions &options) = 0;

        // Each returns whether it restarted the running profile.
        virtual bool setSystemProxy(bool enable, const ModeChange &change) = 0;

        virtual bool setTun(bool enable, const ModeChange &change) = 0;

        virtual bool chooseRoute(int routeId, bool interactive, quint64 serial) = 0;

        virtual int clearTestResults(const QList<int> &profileIds) = 0;

        // Empty when nothing runs, stats are off, or the last sample is older than maxAgeMs.
        virtual std::optional<LiveRates> freshLiveRates(qint64 maxAgeMs) const = 0;

        virtual bool testsRunning() = 0;

        // Safe from any thread.
        virtual bool testsStopRequested() const = 0;

        // Unattended: a refusal is reported by the false return, never a dialog. onFinished may run on a test thread.
        virtual bool runTests(TestKind kind, const QList<int> &profileIds, const std::function<void()> &onFinished) = 0;

        virtual void queueUrlTests(const QList<int> &profileIds, const std::function<void()> &onFinished) = 0;

        // Asks the core to stop and returns without waiting for the session to wind down; blocks on an RPC, so never on the main thread.
        virtual void stopTests() = 0;

        // A no-op while nothing runs.
        virtual void noteRestartNeeded(RestartReason reason) = 0;

        virtual bool restartPending() const = 0;

        // Queued: returns at once and the exit runs later from the event loop, unattended. Accepts only None (shut down) and
        // Restart; false when an exit is already running.
        virtual bool requestExit(ExitReason reason) = 0;

        // "" when nothing is wrong; the headless-only codes appear only in a headless process.
        virtual QString degradedReason() const = 0;

        virtual bool systemProxySupported() const = 0;

        // Main thread; may block on a privilege RPC only where the runtime options allow it.
        virtual bool tunPrivileged() = 0;

        // Whether a restart brings the running profile back.
        virtual bool resumesOnBoot() const = 0;

        // A remembered Tun the boot could not restore (headless).
        virtual bool tunRestoreHeld() const = 0;

        // Why turning Tun on would collide with another Tun owner on this host; empty when free or while ours runs.
        virtual QString tunBlockedReason() = 0;

    signals:
        void startFinished(quint64 serial, int profileId, Engine::StartOutcome outcome, const QString &error);

        void stopFinished(int profileId);

        void connectionStateChanged(Engine::ConnectionState state);
    };
} // namespace Engine
