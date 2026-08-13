#include "include/sys/KillSwitchController.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Configs_sys::KillSwitchBackend;
using Configs_sys::KillSwitchBackendState;
using Configs_sys::KillSwitchController;
using Configs_sys::KillSwitchReconcileResult;
using Configs_sys::KillSwitchResult;
using Configs_sys::KillSwitchTrustedCorePlan;
using Configs_sys::KillSwitchTunInterface;

namespace {

class TestFailure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void require(const bool condition, const char *expression, const int line) {
    if (!condition) {
        throw TestFailure("line " + std::to_string(line) + ": " + expression);
    }
}

#define REQUIRE(expression) require(static_cast<bool>(expression), #expression, __LINE__)

class FakeBackend final : public KillSwitchBackend {
public:
    KillSwitchBackendState state;
    KillSwitchTrustedCorePlan lastPlan;
    KillSwitchTunInterface lastTun;
    std::vector<std::string> calls;
    bool failReconcile = false;
    bool failEnsureBaseline = false;
    bool failStartDynamicCore = false;
    bool failRemoveTun = false;
    bool failAddTun = false;
    bool failDisable = false;
    bool failedDisableDropsDynamicState = false;
    int disableCalls = 0;

    [[nodiscard]] KillSwitchReconcileResult reconcile() override {
        calls.emplace_back("reconcile");
        if (failReconcile) {
            return {KillSwitchResult::Failure(QStringLiteral("reconcile failure")),
                    state};
        }
        return {KillSwitchResult::Success(), state};
    }

    [[nodiscard]] KillSwitchResult ensureBaseline() override {
        calls.emplace_back("ensure-baseline");
        if (failEnsureBaseline) {
            return KillSwitchResult::Failure(QStringLiteral("baseline failure"));
        }
        state.baselineActive = true;
        return KillSwitchResult::Success();
    }

    [[nodiscard]] KillSwitchResult startDynamicCore(
        const KillSwitchTrustedCorePlan &plan) override {
        calls.emplace_back("start-core-policy");
        lastPlan = plan;
        if (failStartDynamicCore) {
            return KillSwitchResult::Failure(QStringLiteral("core policy failure"));
        }
        state.dynamicCoreActive = true;
        return KillSwitchResult::Success();
    }

    [[nodiscard]] KillSwitchResult removeTunAllowance() override {
        calls.emplace_back("remove-tun");
        if (failRemoveTun) {
            return KillSwitchResult::Failure(QStringLiteral("remove TUN failure"));
        }
        state.tunAllowanceActive = false;
        lastTun = {};
        return KillSwitchResult::Success();
    }

    [[nodiscard]] KillSwitchResult addTunAllowance(
        const KillSwitchTunInterface &tunInterface) override {
        calls.emplace_back("add-tun");
        if (failAddTun) {
            return KillSwitchResult::Failure(QStringLiteral("add TUN failure"));
        }
        state.tunAllowanceActive = true;
        lastTun = tunInterface;
        return KillSwitchResult::Success();
    }

    [[nodiscard]] KillSwitchResult disable() override {
        calls.emplace_back("disable");
        ++disableCalls;
        if (failDisable) {
            if (failedDisableDropsDynamicState) {
                state.dynamicCoreActive = false;
                state.tunAllowanceActive = false;
                lastTun = {};
            }
            return KillSwitchResult::Failure(QStringLiteral("disable failure"));
        }
        state = {};
        lastTun = {};
        return KillSwitchResult::Success();
    }
};

KillSwitchTrustedCorePlan trustedCorePlan() {
    return {{QStringLiteral("C:/Program Files/Throne/ThroneCore.exe")}};
}

KillSwitchTunInterface dualStackTun() {
    return {QStringLiteral("throne-tun"), 42, true, true};
}

void requireInvariant(const KillSwitchController &controller) {
    QString reason;
    if (!controller.invariantHolds(&reason)) {
        throw TestFailure("controller invariant failed: " + reason.toStdString());
    }
}

quint64 connectTun(KillSwitchController &controller,
                   const KillSwitchTunInterface &tun = dualStackTun()) {
    const auto prepared = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Connect);
    REQUIRE(prepared);
    REQUIRE(prepared.operationId != 0);
    REQUIRE(controller.profileBecameReady(prepared.operationId, tun));
    requireInvariant(controller);
    return prepared.operationId;
}

void disabledPreservesExistingBehavior() {
    FakeBackend backend;
    KillSwitchController controller(backend);

    const auto initialized = controller.initialize(false, trustedCorePlan());
    REQUIRE(initialized);
    REQUIRE(!initialized.enabled);
    REQUIRE(!initialized.recoveredStaleProtection);
    REQUIRE(initialized.state == KillSwitchController::State::Disabled);
    REQUIRE(backend.disableCalls == 0);
    REQUIRE(backend.calls.size() == 1);
    REQUIRE(backend.calls.front() == "reconcile");

    const auto prepared = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Connect);
    REQUIRE(prepared);
    REQUIRE(prepared.operationId == 0);
    REQUIRE(controller.profileBecameReady(0));
    requireInvariant(controller);
}

void staleProtectionIsRecoveredNotDeleted() {
    FakeBackend backend;
    backend.state = {true, false, true};
    KillSwitchController controller(backend);

    const auto initialized = controller.initialize(false, trustedCorePlan());
    REQUIRE(initialized);
    REQUIRE(initialized.enabled);
    REQUIRE(initialized.recoveredStaleProtection);
    REQUIRE(initialized.state == KillSwitchController::State::Disconnected);
    REQUIRE(backend.disableCalls == 0);
    REQUIRE(backend.state.baselineActive);
    REQUIRE(backend.state.dynamicCoreActive);
    REQUIRE(!backend.state.tunAllowanceActive);

    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.recoveredStaleProtection);
    requireInvariant(controller);
}

void reconcileFailureRetainsObservedProtection() {
    FakeBackend backend;
    backend.state.baselineActive = true;
    backend.failReconcile = true;
    KillSwitchController controller(backend);

    const auto initialized = controller.initialize(false, trustedCorePlan());
    REQUIRE(!initialized);
    REQUIRE(initialized.enabled);
    REQUIRE(initialized.recoveredStaleProtection);
    REQUIRE(initialized.state == KillSwitchController::State::Error);
    REQUIRE(backend.disableCalls == 0);
    requireInvariant(controller);
}

void reconcileFailureWithoutObservedStateStillFailsClosed() {
    FakeBackend backend;
    backend.failReconcile = true;
    backend.failEnsureBaseline = true;
    KillSwitchController controller(backend);

    const auto initialized = controller.initialize(false, trustedCorePlan());
    REQUIRE(!initialized);
    REQUIRE(initialized.enabled);
    REQUIRE(!initialized.recoveredStaleProtection);
    REQUIRE(initialized.state == KillSwitchController::State::Error);

    const auto prepared = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Connect);
    REQUIRE(!prepared);
    REQUIRE(!prepared.mayTearDownCurrentProfile);
    REQUIRE(backend.disableCalls == 0);
    requireInvariant(controller);
}

void normalTunConnectionIsDualStack() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    const auto initialized = controller.initialize(true, trustedCorePlan());
    REQUIRE(initialized);
    REQUIRE(!initialized.recoveredStaleProtection);
    REQUIRE(backend.state.baselineActive);
    REQUIRE(backend.state.dynamicCoreActive);
    REQUIRE(!backend.state.tunAllowanceActive);

    connectTun(controller);
    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Connected);
    REQUIRE(snapshot.backend.tunAllowanceActive);
    REQUIRE(snapshot.allowedTun.ipv4);
    REQUIRE(snapshot.allowedTun.ipv6);
    REQUIRE(backend.lastTun.ipv4);
    REQUIRE(backend.lastTun.ipv6);
}

void configuredLaunchDoesNotReportStaleRecovery() {
    FakeBackend backend;
    backend.state.baselineActive = true;
    KillSwitchController controller(backend);

    const auto initialized = controller.initialize(true, trustedCorePlan());
    REQUIRE(initialized);
    REQUIRE(initialized.enabled);
    REQUIRE(!initialized.recoveredStaleProtection);
    REQUIRE(!controller.snapshot().recoveredStaleProtection);
    REQUIRE(backend.disableCalls == 0);
    requireInvariant(controller);
}

void systemProxyConnectionNeedsNoTunAllowance() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));

    const auto prepared = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Connect);
    REQUIRE(prepared);
    REQUIRE(controller.profileBecameReady(prepared.operationId, std::nullopt));
    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Connected);
    REQUIRE(!snapshot.backend.tunAllowanceActive);
    requireInvariant(controller);
}

void switchOwnsOperationAcrossNestedStop() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    connectTun(controller);

    const auto switching = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Switch);
    REQUIRE(switching);
    REQUIRE(switching.state == KillSwitchController::State::Switching);
    REQUIRE(!backend.state.tunAllowanceActive);
    REQUIRE(backend.calls.size() >= 2);
    REQUIRE(backend.calls[backend.calls.size() - 2] == "ensure-baseline");
    REQUIRE(backend.calls.back() == "remove-tun");

    const auto nestedStop = controller.prepareForProfileStop();
    REQUIRE(nestedStop);
    REQUIRE(nestedStop.operationId == switching.operationId);
    REQUIRE(nestedStop.state == KillSwitchController::State::Switching);
    REQUIRE(controller.profileStopped());
    REQUIRE(controller.snapshot().activeOperationId == switching.operationId);

    REQUIRE(controller.profileBecameReady(switching.operationId, dualStackTun()));
    REQUIRE(controller.snapshot().state == KillSwitchController::State::Connected);
    requireInvariant(controller);
}

void failedSwitchRemainsFailClosed() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    connectTun(controller);

    const auto switching = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Switch);
    REQUIRE(switching);
    REQUIRE(controller.profileStartFailed(
        switching.operationId, QStringLiteral("new profile rejected")));

    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Error);
    REQUIRE(snapshot.backend.baselineActive);
    REQUIRE(snapshot.backend.dynamicCoreActive);
    REQUIRE(!snapshot.backend.tunAllowanceActive);
    REQUIRE(snapshot.activeOperationId == 0);
    requireInvariant(controller);
}

void manualStopLeavesBaselineActive() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    connectTun(controller);

    const auto stopping = controller.prepareForProfileStop();
    REQUIRE(stopping);
    REQUIRE(stopping.state == KillSwitchController::State::Stopping);
    REQUIRE(!backend.state.tunAllowanceActive);
    REQUIRE(controller.profileStopped());

    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Disconnected);
    REQUIRE(snapshot.backend.baselineActive);
    REQUIRE(!snapshot.backend.tunAllowanceActive);
    requireInvariant(controller);
}

void failedStopCanRestoreCurrentTunAndRetry() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    const auto tun = dualStackTun();
    connectTun(controller, tun);
    REQUIRE(controller.prepareForProfileStop());

    backend.failAddTun = true;
    REQUIRE(!controller.profileStopFailed(tun, QStringLiteral("stop RPC failed")));
    REQUIRE(controller.snapshot().state == KillSwitchController::State::Stopping);
    REQUIRE(!controller.snapshot().backend.tunAllowanceActive);
    requireInvariant(controller);

    backend.failAddTun = false;
    REQUIRE(controller.profileStopFailed(tun, QStringLiteral("stop RPC failed")));
    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Connected);
    REQUIRE(snapshot.backend.tunAllowanceActive);
    REQUIRE(snapshot.allowedTun == tun);
    REQUIRE(snapshot.lastError == QStringLiteral("stop RPC failed"));
    requireInvariant(controller);
}

void failedSwitchRestorationCanBeCancelledAndStopped() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    const auto tun = dualStackTun();
    connectTun(controller, tun);

    const auto switching = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Switch);
    REQUIRE(switching);
    REQUIRE(controller.prepareForProfileStop());

    backend.failAddTun = true;
    REQUIRE(!controller.profileStopFailed(
        tun, QStringLiteral("stop and TUN restoration failed")));
    REQUIRE(controller.snapshot().state == KillSwitchController::State::Switching);
    REQUIRE(controller.snapshot().activeOperationId == switching.operationId);

    REQUIRE(controller.profileStartFailed(
        switching.operationId, QStringLiteral("switch cancelled")));
    REQUIRE(controller.snapshot().state == KillSwitchController::State::Error);
    REQUIRE(controller.snapshot().activeOperationId == 0);
    backend.failAddTun = false;

    REQUIRE(controller.prepareForProfileStop());
    REQUIRE(controller.profileStopped());
    REQUIRE(controller.snapshot().state == KillSwitchController::State::Disconnected);
    requireInvariant(controller);
}

void preparationFailureDoesNotAuthorizeTeardown() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    connectTun(controller);
    backend.failEnsureBaseline = true;

    const auto switching = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Switch);
    REQUIRE(!switching);
    REQUIRE(!switching.mayTearDownCurrentProfile);
    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Connected);
    REQUIRE(snapshot.backend.tunAllowanceActive);
    REQUIRE(snapshot.activeOperationId == 0);
    requireInvariant(controller);
}

void tunReadinessCanBeRetriedWithoutOpeningDirectAccess() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    const auto prepared = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Connect);
    REQUIRE(prepared);

    backend.failAddTun = true;
    REQUIRE(!controller.profileBecameReady(prepared.operationId, dualStackTun()));
    auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Connecting);
    REQUIRE(snapshot.activeOperationId == prepared.operationId);
    REQUIRE(snapshot.backend.baselineActive);
    REQUIRE(!snapshot.backend.tunAllowanceActive);
    requireInvariant(controller);

    backend.failAddTun = false;
    REQUIRE(controller.profileBecameReady(prepared.operationId, dualStackTun()));
    snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Connected);
    REQUIRE(snapshot.backend.tunAllowanceActive);
    requireInvariant(controller);
}

void crashAndExitKeepThePersistentBlock() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    connectTun(controller);

    REQUIRE(controller.coreTerminatedUnexpectedly(true));
    auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Reconnecting);
    REQUIRE(snapshot.backend.baselineActive);
    REQUIRE(!snapshot.backend.tunAllowanceActive);
    requireInvariant(controller);

    const auto reconnect = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Reconnect);
    REQUIRE(reconnect);
    REQUIRE(controller.profileStartFailed(
        reconnect.operationId, QStringLiteral("reconnect failed")));
    const auto exiting = controller.prepareForExit();
    REQUIRE(exiting);
    snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Exiting);
    REQUIRE(snapshot.backend.baselineActive);
    REQUIRE(backend.disableCalls == 0);
    requireInvariant(controller);
}

void crashStillRemovesTunWhenBaselineVerificationFails() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    connectTun(controller);
    backend.failEnsureBaseline = true;

    REQUIRE(!controller.coreTerminatedUnexpectedly(false));
    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Error);
    REQUIRE(!snapshot.backend.tunAllowanceActive);
    REQUIRE(!backend.state.tunAllowanceActive);
    REQUIRE(backend.calls.back() == "remove-tun");
    requireInvariant(controller);
}

void explicitDisableIsTheOnlyRemovalPath() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    connectTun(controller);

    REQUIRE(controller.disable());
    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.state == KillSwitchController::State::Disabled);
    REQUIRE(!snapshot.enabled);
    REQUIRE(!snapshot.backend.anyActive());
    REQUIRE(backend.disableCalls == 1);
    requireInvariant(controller);
}

void failedDisableRefreshesTransientBackendState() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    connectTun(controller);

    backend.failDisable = true;
    backend.failedDisableDropsDynamicState = true;
    REQUIRE(!controller.disable());
    auto snapshot = controller.snapshot();
    REQUIRE(snapshot.enabled);
    REQUIRE(snapshot.state == KillSwitchController::State::Error);
    REQUIRE(snapshot.backend.baselineActive);
    REQUIRE(!snapshot.backend.dynamicCoreActive);
    REQUIRE(!snapshot.backend.tunAllowanceActive);
    requireInvariant(controller);

    backend.failDisable = false;
    const auto reconnect = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Connect);
    REQUIRE(reconnect);
    REQUIRE(backend.state.dynamicCoreActive);
    REQUIRE(controller.profileBecameReady(reconnect.operationId, dualStackTun()));
    requireInvariant(controller);
}

void failedDisableWithUnknownReconcileStillRefusesUnprotectedStart() {
    FakeBackend backend;
    KillSwitchController controller(backend);
    REQUIRE(controller.initialize(true, trustedCorePlan()));
    connectTun(controller);

    backend.failDisable = true;
    backend.failedDisableDropsDynamicState = true;
    backend.failReconcile = true;
    backend.failEnsureBaseline = true;
    REQUIRE(!controller.disable());
    const auto snapshot = controller.snapshot();
    REQUIRE(snapshot.enabled);
    REQUIRE(snapshot.state == KillSwitchController::State::Error);
    REQUIRE(!snapshot.backend.dynamicCoreActive);

    const auto reconnect = controller.prepareForProfileStart(
        KillSwitchController::StartIntent::Connect);
    REQUIRE(!reconnect);
    REQUIRE(!reconnect.mayTearDownCurrentProfile);
    requireInvariant(controller);
}

} // namespace

int main() {
    const std::vector<std::pair<const char *, void (*)()>> tests = {
        {"disabled preserves existing behavior", disabledPreservesExistingBehavior},
        {"stale protection is recovered, not deleted", staleProtectionIsRecoveredNotDeleted},
        {"reconcile failure retains observed protection", reconcileFailureRetainsObservedProtection},
        {"unknown reconcile failure still fails closed", reconcileFailureWithoutObservedStateStillFailsClosed},
        {"normal TUN connection is dual stack", normalTunConnectionIsDualStack},
        {"configured launch is not stale recovery", configuredLaunchDoesNotReportStaleRecovery},
        {"System Proxy connection needs no TUN allowance", systemProxyConnectionNeedsNoTunAllowance},
        {"switch owns operation across nested stop", switchOwnsOperationAcrossNestedStop},
        {"failed switch remains fail closed", failedSwitchRemainsFailClosed},
        {"manual stop leaves baseline active", manualStopLeavesBaselineActive},
        {"failed stop can restore current TUN and retry", failedStopCanRestoreCurrentTunAndRetry},
        {"failed switch restoration can be cancelled", failedSwitchRestorationCanBeCancelledAndStopped},
        {"preparation failure does not authorize teardown", preparationFailureDoesNotAuthorizeTeardown},
        {"TUN readiness can be retried", tunReadinessCanBeRetriedWithoutOpeningDirectAccess},
        {"crash and exit keep the persistent block", crashAndExitKeepThePersistentBlock},
        {"crash removes TUN after baseline verification failure", crashStillRemovesTunWhenBaselineVerificationFails},
        {"explicit disable is the only removal path", explicitDisableIsTheOnlyRemovalPath},
        {"failed disable refreshes transient state", failedDisableRefreshesTransientBackendState},
        {"failed disable with unknown state stays closed", failedDisableWithUnknownReconcileStillRefusesUnprotectedStart},
    };

    int failures = 0;
    for (const auto &[name, test] : tests) {
        try {
            test();
            std::cout << "PASS: " << name << '\n';
        } catch (const std::exception &error) {
            ++failures;
            std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures) << '/'
              << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
