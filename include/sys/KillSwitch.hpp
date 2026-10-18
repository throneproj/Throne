#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <atomic>
#include <functional>

namespace Sys {
    class GuardProcess;
    struct GuardConfig;

    // UI thread only, except state() and permitExtraCore().
    class KillSwitch : public QObject {
        Q_OBJECT

    public:
        enum class State { Disabled, Arming, Armed, Failed };

        // ThroneCore's exit code when THRONE_GUARD is set but it cannot adopt the guard group (no CAP_SETGID, or the GID is not
        // mapped in this user namespace). Must match guard.IdentityExitCode in core/internal/guard/guard.go.
        static constexpr int kCoreIdentityExitCode = 86;

        static KillSwitch *instance();

        [[nodiscard]] State state() const { return state_.load(); }
        [[nodiscard]] bool allowsStart() const;
        [[nodiscard]] QString failureCode() const { return failureCode_; }
        [[nodiscard]] QString failureMessage() const { return failureMessage_; }
        [[nodiscard]] bool failedForPrivileges() const;
        // Failed with a code a respawn cannot fix while nothing changes: privilege, unsupported, config, overridden.
        [[nodiscard]] bool failedPersistently() const;
        // UI thread. Failed (unsupported) only because the core cannot adopt the guard group while a guard still runs and blocks:
        // profiles are refused, but the kill switch is not "not active".
        [[nodiscard]] bool failedForIdentity() const;
        [[nodiscard]] QString failureText() const;

        // Any thread. Set by noteIdentityUnsupported(); CoreProcess::Start then omits THRONE_GUARD.
        [[nodiscard]] bool identityUnsupported() const { return identityUnsupported_.load(); }

        // Main thread, when a core exited with kCoreIdentityExitCode. Sticky until kill_switch is turned off (never saved): the
        // guard keeps being armed and blocking, but the state reports Failed with code "unsupported" (detail as the message), so
        // starts stay refused and the process lives.
        void noteIdentityUnsupported(const QString &detail);

        // Main thread, before the first apply(); headless --no-kill-switch. A saved kill_switch=true then fails closed with code
        // "overridden" instead of arming, so profiles stay refused while the API can turn the setting off. Never saved.
        void setOverride(bool overridden);

        // Any thread: the Throne API's TCP listen port, 0 when it does not listen. The guard lets the replies of inbound connections
        // to it out, so a kill switch never cuts the API off. A change re-applies an armed or arming guard on the UI thread.
        static void SetApiPort(quint16 port);

        void apply();

        void whenSettled(QObject *context, std::function<void(bool armed)> callback);

        // Worker threads only: blocks until the guard permits the extra core's executable or gives up (Windows).
        bool permitExtraCore(const QString &path);

        void shutdown();

    signals:
        void stateChanged();

    private:
        struct Waiter {
            QPointer<QObject> context;
            bool bound = false;
            // Waits for the pending guard to resolve, not only for the state to leave Arming.
            bool idle = false;
            std::function<void(bool)> callback;
        };

        KillSwitch() = default;

        void spawn(const GuardConfig &config);
        void closeGuard(GuardProcess *guard);
        void readOutput(GuardProcess *guard);
        void readErrors(GuardProcess *guard, bool flush);
        void handleLine(GuardProcess *guard, const QString &line);
        void onReady(GuardProcess *guard);
        void onReadyTimeout(GuardProcess *guard);
        void onStartFailed(GuardProcess *guard);
        void onFinished(GuardProcess *guard, int exitCode, bool crashed);
        void armFailed(const QString &code, const QString &message);
        void protectionLost(const QString &code, const QString &message);
        void fail(const QString &code, const QString &message);
        // Identity unsupported: report Failed (unsupported) while any guard keeps running and blocking.
        void failIdentity();
        void setState(State state);
        void settle();
        void whenIdle(std::function<void()> callback);

        std::atomic<State> state_ = State::Disabled;
        std::atomic<bool> identityUnsupported_ = false;
        QString identityDetail_;
        bool overridden_ = false;
        GuardProcess *active_ = nullptr;
        GuardProcess *pending_ = nullptr;
        QString failureCode_;
        QString failureMessage_;
        QElapsedTimer lastRespawn_;
        QList<Waiter> waiters_;
        bool shutdown_ = false;
    };
} // namespace Sys
