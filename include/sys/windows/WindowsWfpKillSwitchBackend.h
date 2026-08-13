#pragma once

#include "include/sys/KillSwitchController.hpp"

#include <QString>

#include <memory>

// Windows Filtering Platform implementation of Throne's fail-closed policy.
//
// The providerless baseline is persistent and intentionally independent of
// both Throne and ThroneCore. Its catch-all filters continue blocking new
// direct connections after either process exits, crashes, or BFE reloads. The
// core application and TUN interface exceptions live in a dynamic WFP session
// and are therefore removed automatically if Throne exits unexpectedly.
class WindowsWfpKillSwitchBackend final : public Configs_sys::KillSwitchBackend
{
public:
    enum class BaselineState
    {
        Absent,
        Valid,
        StaleOrPartial,
        Error,
    };

    struct BaselineStatus
    {
        BaselineState state = BaselineState::Error;
        QString detail;

        [[nodiscard]] bool isValid() const { return state == BaselineState::Valid; }
        // Error means the backend could not prove absence. Callers must treat
        // it as potentially active and must not authorize an unprotected
        // profile transition from it.
        [[nodiscard]] bool mayBeActive() const { return state != BaselineState::Absent; }
    };

    WindowsWfpKillSwitchBackend();
    ~WindowsWfpKillSwitchBackend();

    WindowsWfpKillSwitchBackend(const WindowsWfpKillSwitchBackend &) = delete;
    WindowsWfpKillSwitchBackend &operator=(const WindowsWfpKillSwitchBackend &) = delete;

    // Reports whether all deterministic Throne objects are absent, valid, or
    // only partially present.  No WFP state is changed.
    [[nodiscard]] BaselineStatus queryBaseline() const;

    // Atomically migrates a marked provider-associated v1-v3 graph, or
    // replaces Throne's exact providerless objects with the current schema.
    // Foreign deterministic-GUID collisions are never removed. An active
    // dynamic session must be stopped first.
    bool reconcileBaseline(QString *error);

    // Idempotently installs the baseline, repairing stale/partial objects when
    // necessary.
    [[nodiscard]] Configs_sys::KillSwitchReconcileResult reconcile() override;
    [[nodiscard]] Configs_sys::KillSwitchResult ensureBaseline() override;

    // Opens a dynamic WFP session and permits only the plan's exact core
    // executable. The current implementation intentionally rejects multiple
    // executable paths rather than broadening the trusted bootstrap boundary.
    [[nodiscard]] Configs_sys::KillSwitchResult startDynamicCore(
        const Configs_sys::KillSwitchTrustedCorePlan &plan) override;

    // Lower-level, detailed-error API used by startDynamicCore. Repeating the
    // same canonical path is idempotent and preserves any active TUN allowance.
    bool startCoreSession(const QString &absoluteCoreExecutablePath, QString *error);

    // Idempotently closes the dynamic session (and thereby all its permits).
    bool stopCoreSession(QString *error);

    // Permits outbound flows whose actual departing next-hop interface is
    // Throne's standard TUN adapter, and inbound flows that actually arrived
    // on that adapter. The adapter must already exist; callers may retry while
    // the core is bringing it up.
    [[nodiscard]] Configs_sys::KillSwitchResult addTunAllowance(
        const Configs_sys::KillSwitchTunInterface &tunInterface) override;

    bool addTunAllowance(QString *error);

    // Variant for tests and future configurable adapter names.  IPv6 should
    // only be disabled when the corresponding TUN family is deliberately off.
    bool addTunAllowanceForInterface(const QString &interfaceAlias,
                                     bool allowIPv4,
                                     bool allowIPv6,
                                     QString *error);

    // Removes the TUN permits in a transaction.  This must happen before the
    // core destroys the adapter so existing ALE flows are re-authorized while
    // the persistent catch-all block is still present.
    [[nodiscard]] Configs_sys::KillSwitchResult removeTunAllowance() override;

    // Closes the dynamic session and then atomically removes only Throne's
    // deterministic persistent objects.  No unrelated WFP/firewall policy is
    // enumerated or changed.
    [[nodiscard]] Configs_sys::KillSwitchResult disable() override;

    [[nodiscard]] bool coreSessionActive() const;

    static QString tunInterfaceAlias();

private:
    bool addTunAllowanceForLuidValue(quint64 interfaceLuid,
                                     bool allowIPv4,
                                     bool allowIPv6,
                                     QString *error);
    bool removeTunAllowanceImpl(QString *error);
    bool disableImpl(QString *error);

    class Impl;
    std::unique_ptr<Impl> impl_;
};
