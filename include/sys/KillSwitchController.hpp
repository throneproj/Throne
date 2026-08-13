#pragma once

#include <QMutex>
#include <QString>
#include <QStringList>
#include <QtTypes>

#include <optional>

namespace Configs_sys {

// The controller deliberately knows nothing about WFP, Windows Firewall, or
// another platform implementation.  In particular, it cannot make a blocking
// policy safe by itself: backend operations which fail must leave the OS in the
// previous state or in a stricter state, never in a less restrictive state.
struct KillSwitchResult {
    bool ok = false;
    QString error;

    [[nodiscard]] static KillSwitchResult Success();
    [[nodiscard]] static KillSwitchResult Failure(QString error);
    [[nodiscard]] explicit operator bool() const { return ok; }
};

struct KillSwitchTrustedCorePlan {
    // The baseline deliberately exempts only these trusted core executables so
    // they can establish and carry the tunnel.  Current profile formats do not
    // expose a complete, static endpoint set (custom cores, Tailscale/DERP and
    // domain rotation are examples), so this is an application-scoped permit,
    // not an endpoint-scoped one.  Keep this list minimal and canonicalized.
    QStringList executablePaths;

    [[nodiscard]] bool isValid() const;
    friend bool operator==(const KillSwitchTrustedCorePlan &,
                           const KillSwitchTrustedCorePlan &) = default;
};

struct KillSwitchTunInterface {
    QString name;
    // Platform interface index, not a process-owned handle.  A name is retained
    // as a diagnostic/fallback identity for platforms without numeric indices.
    quint64 interfaceIndex = 0;
    bool ipv4 = true;
    bool ipv6 = false;

    [[nodiscard]] bool isValid() const;
    friend bool operator==(const KillSwitchTunInterface &,
                           const KillSwitchTunInterface &) = default;
};

struct KillSwitchBackendState {
    bool baselineActive = false;
    bool dynamicCoreActive = false;
    bool tunAllowanceActive = false;

    [[nodiscard]] bool anyActive() const {
        return baselineActive || dynamicCoreActive || tunAllowanceActive;
    }
};

struct KillSwitchReconcileResult {
    KillSwitchResult result;
    KillSwitchBackendState state;
};

class KillSwitchBackend {
public:
    virtual ~KillSwitchBackend() = default;

    // Discover/reconcile only Throne-owned state.  The implementation must not
    // touch unrelated firewall configuration.  Any stale transient session or
    // TUN allow should be made safe before returning its observed state.
    [[nodiscard]] virtual KillSwitchReconcileResult reconcile() = 0;

    // Install or verify the persistent, dual-stack fail-closed baseline.
    [[nodiscard]] virtual KillSwitchResult ensureBaseline() = 0;

    // Create or replace the trusted application permits needed by the core.
    // This is normally installed once at application startup and remains valid
    // across profile changes.  The persistent baseline remains installed.
    [[nodiscard]] virtual KillSwitchResult startDynamicCore(
        const KillSwitchTrustedCorePlan &plan) = 0;

    // These calls are idempotent.  removeTunAllowance must be safe when the TUN
    // has already vanished, and addTunAllowance must never weaken the baseline
    // for another interface.
    [[nodiscard]] virtual KillSwitchResult removeTunAllowance() = 0;
    [[nodiscard]] virtual KillSwitchResult addTunAllowance(
        const KillSwitchTunInterface &tunInterface) = 0;

    // Remove only Throne-owned persistent and transient objects.
    [[nodiscard]] virtual KillSwitchResult disable() = 0;
};

class KillSwitchController {
public:
    enum class State {
        Disabled,
        Connecting,
        Connected,
        Switching,
        Reconnecting,
        Stopping,
        Disconnected,
        Error,
        Exiting,
    };

    enum class StartIntent {
        Connect,
        Switch,
        Reconnect,
    };

    struct PrepareResult {
        // Callers must not stop a working profile unless this is true.
        bool mayTearDownCurrentProfile = false;
        quint64 operationId = 0;
        State state = State::Disabled;
        QString error;

        [[nodiscard]] explicit operator bool() const {
            return mayTearDownCurrentProfile;
        }
    };

    struct Snapshot {
        bool initialized = false;
        bool enabled = false;
        bool recoveredStaleProtection = false;
        State state = State::Disabled;
        KillSwitchBackendState backend;
        KillSwitchTunInterface allowedTun;
        quint64 activeOperationId = 0;
        QString lastError;
    };

    struct InitializationResult {
        KillSwitchResult result;
        bool enabled = false;
        bool recoveredStaleProtection = false;
        State state = State::Disabled;

        [[nodiscard]] explicit operator bool() const {
            return static_cast<bool>(result);
        }
    };

    explicit KillSwitchController(KillSwitchBackend &backend);

    // Must be called once after settings are loaded.  Reconciliation runs even
    // when shouldEnable is false.  Discovered Throne protection is retained and
    // promoted to enabled; only an explicit disable() removes persistent rules.
    [[nodiscard]] InitializationResult initialize(
        bool shouldEnable, KillSwitchTrustedCorePlan trustedCorePlan);
    [[nodiscard]] KillSwitchResult enable();
    [[nodiscard]] KillSwitchResult disable();

    // The successful return is the prepare-before-stop security boundary:
    // baseline -> constrained core session -> remove old TUN allow.  No caller
    // may tear down the old profile before it receives success.
    [[nodiscard]] PrepareResult prepareForProfileStart(
        StartIntent intent);

    // operationId rejects late readiness/failure callbacks from an older start.
    // System Proxy profiles pass std::nullopt; TUN profiles pass the ready
    // interface including the IP families it carries.
    [[nodiscard]] KillSwitchResult profileBecameReady(
        quint64 operationId,
        std::optional<KillSwitchTunInterface> tunInterface = std::nullopt);
    [[nodiscard]] KillSwitchResult profileStartFailed(
        quint64 operationId, QString error);

    [[nodiscard]] PrepareResult prepareForProfileStop();
    [[nodiscard]] KillSwitchResult profileStopped();

    // Rolls back a prepared stop when the stop RPC failed and the old profile
    // is still operational.  TUN profiles pass the still-live interface so its
    // allowance can be restored; System Proxy profiles pass std::nullopt.  A
    // failed add is safe to retry and leaves the connection blocked meanwhile.
    [[nodiscard]] KillSwitchResult profileStopFailed(
        std::optional<KillSwitchTunInterface> stillActiveTun = std::nullopt,
        QString error = {});

    // Called after an unplanned daemon/core exit.  The baseline is re-verified
    // and the obsolete TUN permission is removed; it is never disabled.
    [[nodiscard]] KillSwitchResult coreTerminatedUnexpectedly(
        bool reconnectPlanned);

    // With the kill switch enabled, normal application exit intentionally keeps
    // the persistent baseline for fail-closed crash/exit semantics.
    [[nodiscard]] PrepareResult prepareForExit();

    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] bool invariantHolds(QString *reason = nullptr) const;
    [[nodiscard]] static QString stateName(State state);

private:
    [[nodiscard]] KillSwitchResult ensureBaselineLocked();
    [[nodiscard]] KillSwitchResult removeTunAllowanceLocked();
    [[nodiscard]] KillSwitchResult backendFailureLocked(
        const QString &action, const KillSwitchResult &result);
    [[nodiscard]] PrepareResult prepareFailureLocked(
        State originalState, const QString &error) const;
    [[nodiscard]] bool invariantHoldsLocked(QString *reason) const;
    [[nodiscard]] bool startAllowedLocked(StartIntent intent) const;
    [[nodiscard]] quint64 nextOperationIdLocked();

    KillSwitchBackend &backend_;
    mutable QMutex mutex_;
    bool initialized_ = false;
    bool enabled_ = false;
    bool recoveredStaleProtection_ = false;
    State state_ = State::Disabled;
    KillSwitchBackendState backendState_;
    KillSwitchTunInterface allowedTun_;
    quint64 operationCounter_ = 0;
    quint64 activeOperationId_ = 0;
    KillSwitchTrustedCorePlan trustedCorePlan_;
    QString lastError_;
};

} // namespace Configs_sys
