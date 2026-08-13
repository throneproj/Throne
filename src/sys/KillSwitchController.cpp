#include "include/sys/KillSwitchController.hpp"

#include <QMutexLocker>

#include <utility>

namespace Configs_sys {

namespace {

QString normalizedError(const QString &action, const KillSwitchResult &result) {
    if (!result.error.trimmed().isEmpty()) {
        return action + QStringLiteral(": ") + result.error.trimmed();
    }
    return action + QStringLiteral(" failed");
}

} // namespace

KillSwitchResult KillSwitchResult::Success() {
    return {true, {}};
}

KillSwitchResult KillSwitchResult::Failure(QString error) {
    return {false, std::move(error)};
}

bool KillSwitchTrustedCorePlan::isValid() const {
    if (executablePaths.isEmpty()) {
        return false;
    }
    for (const auto &path : executablePaths) {
        if (path.trimmed().isEmpty()) {
            return false;
        }
    }
    return true;
}

bool KillSwitchTunInterface::isValid() const {
    return (interfaceIndex != 0 || !name.trimmed().isEmpty()) && (ipv4 || ipv6);
}

KillSwitchController::KillSwitchController(KillSwitchBackend &backend)
    : backend_(backend) {
}

KillSwitchController::InitializationResult KillSwitchController::initialize(
    const bool shouldEnable, KillSwitchTrustedCorePlan trustedCorePlan) {
    QMutexLocker locker(&mutex_);
    if (initialized_) {
        return {
            KillSwitchResult::Failure(
                QStringLiteral("Kill switch controller is already initialized")),
            enabled_,
            recoveredStaleProtection_,
            state_,
        };
    }

    initialized_ = true;
    trustedCorePlan_ = std::move(trustedCorePlan);

    const auto reconciled = backend_.reconcile();
    backendState_ = reconciled.state;
    if (!reconciled.result) {
        if (!shouldEnable && !backendState_.anyActive()) {
            // The preference is explicitly off and the backend did not observe
            // any owned policy. A generic platform-query failure (for example,
            // BFE being unavailable) must preserve legacy/default-off behavior.
            // A later explicit enable will still have to install and verify the
            // baseline before any profile can be torn down.
            enabled_ = false;
            recoveredStaleProtection_ = false;
            state_ = State::Disabled;
            lastError_.clear();
            return {KillSwitchResult::Success(), false, false, state_};
        }
        lastError_ = normalizedError(QStringLiteral("reconcile kill switch state"),
                                     reconciled.result);
        state_ = State::Error;
        // Unknown OS state is never assumed safe to remove or bypass. Latch
        // protection on even if the failed query could not report any objects;
        // every later transition must first prove/install the baseline.
        enabled_ = true;
        recoveredStaleProtection_ = !shouldEnable && backendState_.anyActive();
        return {KillSwitchResult::Failure(lastError_), enabled_,
                recoveredStaleProtection_, state_};
    }

    const bool discoveredProtection = backendState_.anyActive();
    recoveredStaleProtection_ = !shouldEnable && discoveredProtection;
    enabled_ = shouldEnable || discoveredProtection;
    if (!enabled_) {
        // An explicit disable is the only path which removes persistent state.
        // Startup reconciliation must never silently discard crash protection.
        enabled_ = false;
        state_ = State::Disabled;
        lastError_.clear();
        return {KillSwitchResult::Success(), false, false, state_};
    }

    if (!trustedCorePlan_.isValid()) {
        lastError_ = QStringLiteral("No trusted core executable was provided");
        state_ = State::Error;
        return {KillSwitchResult::Failure(lastError_), enabled_,
                recoveredStaleProtection_, state_};
    }

    auto result = ensureBaselineLocked();
    if (!result) {
        state_ = State::Error;
        return {result, enabled_, recoveredStaleProtection_, state_};
    }
    result = removeTunAllowanceLocked();
    if (!result) {
        state_ = State::Error;
        return {result, enabled_, recoveredStaleProtection_, state_};
    }
    result = backend_.startDynamicCore(trustedCorePlan_);
    if (!result) {
        result = backendFailureLocked(QStringLiteral("start trusted core session"),
                                      result);
        state_ = State::Error;
        return {result, enabled_, recoveredStaleProtection_, state_};
    }
    backendState_.dynamicCoreActive = true;

    activeOperationId_ = 0;
    state_ = State::Disconnected;
    lastError_.clear();
    return {KillSwitchResult::Success(), enabled_, recoveredStaleProtection_,
            state_};
}

KillSwitchResult KillSwitchController::enable() {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return KillSwitchResult::Failure(
            QStringLiteral("Kill switch controller is not initialized"));
    }
    if (state_ == State::Exiting) {
        return KillSwitchResult::Failure(
            QStringLiteral("Cannot enable kill switch while exiting"));
    }

    const bool wasEnabled = enabled_;
    enabled_ = true;
    if (!trustedCorePlan_.isValid()) {
        lastError_ = QStringLiteral("No trusted core executable was provided");
        state_ = State::Error;
        return KillSwitchResult::Failure(lastError_);
    }
    auto result = ensureBaselineLocked();
    if (!result) {
        state_ = State::Error;
        return result;
    }

    if (wasEnabled && state_ != State::Error && state_ != State::Disabled &&
        backendState_.dynamicCoreActive) {
        lastError_.clear();
        return KillSwitchResult::Success();
    }

    // Enabling from Disabled/Error establishes the fail-closed disconnected
    // state.  Never inherit a stale per-interface permit.
    result = removeTunAllowanceLocked();
    if (!result) {
        state_ = State::Error;
        return result;
    }
    result = backend_.startDynamicCore(trustedCorePlan_);
    if (!result) {
        result = backendFailureLocked(QStringLiteral("start trusted core session"),
                                      result);
        state_ = State::Error;
        return result;
    }
    backendState_.dynamicCoreActive = true;
    activeOperationId_ = 0;
    state_ = State::Disconnected;
    lastError_.clear();
    return KillSwitchResult::Success();
}

KillSwitchResult KillSwitchController::disable() {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return KillSwitchResult::Failure(
            QStringLiteral("Kill switch controller is not initialized"));
    }

    const auto result = backend_.disable();
    if (!result) {
        // A backend may have closed its dynamic session before an ownership or
        // persistent-policy removal failed. Refresh the cached state so a
        // later retry never mistakes a vanished core/TUN permit for an active
        // one. Reconciliation is conservative: unknown OS state remains
        // logically enabled and all transitions must re-prove the baseline.
        const auto reconciled = backend_.reconcile();
        backendState_ = reconciled.state;
        allowedTun_ = {};
        activeOperationId_ = 0;
        enabled_ = true;
        lastError_ = normalizedError(QStringLiteral("disable kill switch"), result);
        if (!reconciled.result) {
            lastError_ += QStringLiteral("; ") +
                          normalizedError(QStringLiteral("reconcile after failed disable"),
                                          reconciled.result);
        }
        state_ = State::Error;
        return KillSwitchResult::Failure(lastError_);
    }

    backendState_ = {};
    allowedTun_ = {};
    activeOperationId_ = 0;
    enabled_ = false;
    recoveredStaleProtection_ = false;
    state_ = State::Disabled;
    lastError_.clear();
    return KillSwitchResult::Success();
}

KillSwitchController::PrepareResult KillSwitchController::prepareForProfileStart(
    const StartIntent intent) {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return {false, 0, state_,
                QStringLiteral("Kill switch controller is not initialized")};
    }
    if (state_ == State::Exiting) {
        return {false, 0, state_,
                QStringLiteral("Cannot start a profile while exiting")};
    }
    if (!enabled_) {
        return {true, 0, State::Disabled, {}};
    }
    if (activeOperationId_ != 0) {
        return {false, activeOperationId_, state_,
                QStringLiteral("Another protected profile transition is active")};
    }
    if (!startAllowedLocked(intent)) {
        return {false, 0, state_,
                QStringLiteral("Cannot begin %1 from kill switch state %2")
                    .arg(intent == StartIntent::Connect
                             ? QStringLiteral("connect")
                             : intent == StartIntent::Switch
                                   ? QStringLiteral("switch")
                                   : QStringLiteral("reconnect"),
                         stateName(state_))};
    }

    const State originalState = state_;
    auto result = ensureBaselineLocked();
    if (!result) {
        return prepareFailureLocked(originalState, result.error);
    }

    if (!backendState_.dynamicCoreActive) {
        // A reconciled or restarted backend can lose its dynamic session.  It is
        // safe to reconstruct it here because the old profile remains up until
        // this entire preparation succeeds.
        result = backend_.startDynamicCore(trustedCorePlan_);
        if (!result) {
            const auto failure = backendFailureLocked(
                QStringLiteral("start trusted core session"), result);
            return prepareFailureLocked(originalState, failure.error);
        }
        backendState_.dynamicCoreActive = true;
    }

    // This is deliberately last.  Until all prerequisite protection is ready,
    // the old TUN permission and working connection are left untouched.
    result = removeTunAllowanceLocked();
    if (!result) {
        return prepareFailureLocked(originalState, result.error);
    }

    activeOperationId_ = nextOperationIdLocked();
    switch (intent) {
        case StartIntent::Connect:
            state_ = State::Connecting;
            break;
        case StartIntent::Switch:
            state_ = State::Switching;
            break;
        case StartIntent::Reconnect:
            state_ = State::Reconnecting;
            break;
    }
    lastError_.clear();
    return {true, activeOperationId_, state_, {}};
}

KillSwitchResult KillSwitchController::profileBecameReady(
    const quint64 operationId,
    std::optional<KillSwitchTunInterface> tunInterface) {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return KillSwitchResult::Failure(
            QStringLiteral("Kill switch controller is not initialized"));
    }
    if (!enabled_) {
        return KillSwitchResult::Success();
    }
    if (operationId == 0 || operationId != activeOperationId_) {
        return KillSwitchResult::Failure(
            QStringLiteral("Ignoring stale profile-ready notification"));
    }
    if (state_ != State::Connecting && state_ != State::Switching &&
        state_ != State::Reconnecting) {
        return KillSwitchResult::Failure(
            QStringLiteral("Profile cannot become ready from kill switch state %1")
                .arg(stateName(state_)));
    }
    if (tunInterface.has_value() && !tunInterface->isValid()) {
        return KillSwitchResult::Failure(
            QStringLiteral("Cannot allow an unidentified TUN interface"));
    }

    if (tunInterface.has_value()) {
        const auto result = backend_.addTunAllowance(*tunInterface);
        if (!result) {
            // Retain the operation and transition state so readiness can be
            // retried after a transient interface-enumeration/backend failure.
            // The absent allowance keeps traffic fail-closed while retrying.
            lastError_ = normalizedError(
                QStringLiteral("allow ready TUN interface"), result);
            return KillSwitchResult::Failure(lastError_);
        }

        backendState_.tunAllowanceActive = true;
        allowedTun_ = *tunInterface;
    } else {
        backendState_.tunAllowanceActive = false;
        allowedTun_ = {};
    }
    activeOperationId_ = 0;
    state_ = State::Connected;
    lastError_.clear();
    return KillSwitchResult::Success();
}

KillSwitchResult KillSwitchController::profileStartFailed(
    const quint64 operationId, QString error) {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return KillSwitchResult::Failure(
            QStringLiteral("Kill switch controller is not initialized"));
    }
    if (!enabled_) {
        return KillSwitchResult::Success();
    }
    if (operationId == 0 || operationId != activeOperationId_) {
        return KillSwitchResult::Failure(
            QStringLiteral("Ignoring stale profile-failure notification"));
    }

    auto protectionResult = removeTunAllowanceLocked();
    activeOperationId_ = 0;
    state_ = State::Error;

    error = error.trimmed();
    if (error.isEmpty()) {
        error = QStringLiteral("Profile failed to start");
    }
    if (!protectionResult) {
        error += QStringLiteral("; ") + protectionResult.error;
    }
    lastError_ = error;

    // The profile failed, but this result reports whether the controller handled
    // that failure safely.  Direct access remains blocked in either case; a
    // backend failure is returned so the caller can surface the degraded state.
    if (!protectionResult) {
        return KillSwitchResult::Failure(lastError_);
    }
    return KillSwitchResult::Success();
}

KillSwitchController::PrepareResult KillSwitchController::prepareForProfileStop() {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return {false, 0, state_,
                QStringLiteral("Kill switch controller is not initialized")};
    }
    if (state_ == State::Exiting) {
        // prepareForExit already removed the allowance.  The ordinary profile
        // stop path can now run without changing the Exiting state.
        return {true, 0, state_, {}};
    }
    if (!enabled_) {
        return {true, 0, State::Disabled, {}};
    }

    const State originalState = state_;
    auto result = ensureBaselineLocked();
    if (!result) {
        return prepareFailureLocked(originalState, result.error);
    }
    result = removeTunAllowanceLocked();
    if (!result) {
        return prepareFailureLocked(originalState, result.error);
    }

    if ((state_ == State::Switching || state_ == State::Reconnecting) &&
        activeOperationId_ != 0) {
        // profile_start() currently invokes the ordinary profile_stop() path
        // between preparation and the next RPC Start.  Preserve the enclosing
        // operation so its eventual ready/failure callback remains valid.
        lastError_.clear();
        return {true, activeOperationId_, state_, {}};
    }

    // A standalone stop invalidates any in-flight Start callback before the
    // core is torn down.
    activeOperationId_ = 0;
    state_ = State::Stopping;
    lastError_.clear();
    return {true, 0, state_, {}};
}

KillSwitchResult KillSwitchController::profileStopped() {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return KillSwitchResult::Failure(
            QStringLiteral("Kill switch controller is not initialized"));
    }
    if (!enabled_) {
        return KillSwitchResult::Success();
    }
    if (state_ == State::Exiting) {
        return KillSwitchResult::Success();
    }
    if ((state_ == State::Switching || state_ == State::Reconnecting) &&
        activeOperationId_ != 0) {
        return KillSwitchResult::Success();
    }
    if (state_ != State::Stopping) {
        return KillSwitchResult::Failure(
            QStringLiteral("Profile stopped without a protected stop preparation"));
    }

    state_ = State::Disconnected;
    lastError_.clear();
    return KillSwitchResult::Success();
}

KillSwitchResult KillSwitchController::profileStopFailed(
    std::optional<KillSwitchTunInterface> stillActiveTun, QString error) {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return KillSwitchResult::Failure(
            QStringLiteral("Kill switch controller is not initialized"));
    }
    if (!enabled_) {
        return KillSwitchResult::Success();
    }
    if (state_ != State::Stopping &&
        !((state_ == State::Switching || state_ == State::Reconnecting) &&
          activeOperationId_ != 0)) {
        return KillSwitchResult::Failure(
            QStringLiteral("Profile stop failed without a protected stop preparation"));
    }
    if (stillActiveTun.has_value() && !stillActiveTun->isValid()) {
        return KillSwitchResult::Failure(
            QStringLiteral("Cannot restore an unidentified TUN interface"));
    }

    if (stillActiveTun.has_value()) {
        const auto result = backend_.addTunAllowance(*stillActiveTun);
        if (!result) {
            // Retain Stopping/Switching and the absent allowance.  This is
            // deliberately retryable and remains fail-closed until it succeeds.
            lastError_ = normalizedError(
                QStringLiteral("restore TUN allowance after failed stop"), result);
            return KillSwitchResult::Failure(lastError_);
        }
        backendState_.tunAllowanceActive = true;
        allowedTun_ = *stillActiveTun;
    } else {
        backendState_.tunAllowanceActive = false;
        allowedTun_ = {};
    }

    activeOperationId_ = 0;
    state_ = State::Connected;
    error = error.trimmed();
    lastError_ = error.isEmpty() ? QStringLiteral("Profile failed to stop")
                                 : std::move(error);
    return KillSwitchResult::Success();
}

KillSwitchResult KillSwitchController::coreTerminatedUnexpectedly(
    const bool reconnectPlanned) {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return KillSwitchResult::Failure(
            QStringLiteral("Kill switch controller is not initialized"));
    }
    if (!enabled_) {
        return KillSwitchResult::Success();
    }

    const bool wasExiting = state_ == State::Exiting;
    const auto baselineResult = ensureBaselineLocked();
    const auto baselineError = baselineResult.error;
    const auto removalResult = removeTunAllowanceLocked();
    activeOperationId_ = 0;
    if (!baselineResult || !removalResult) {
        state_ = State::Error;
        if (!baselineResult && !removalResult) {
            lastError_ = baselineError + QStringLiteral("; ") + removalResult.error;
        } else if (!baselineResult) {
            lastError_ = baselineError;
        }
        return KillSwitchResult::Failure(lastError_);
    }

    if (wasExiting) {
        state_ = State::Exiting;
        lastError_.clear();
    } else if (reconnectPlanned) {
        state_ = State::Reconnecting;
        lastError_.clear();
    } else {
        state_ = State::Error;
        lastError_ = QStringLiteral("Core terminated unexpectedly");
    }
    return KillSwitchResult::Success();
}

KillSwitchController::PrepareResult KillSwitchController::prepareForExit() {
    QMutexLocker locker(&mutex_);
    if (!initialized_) {
        return {false, 0, state_,
                QStringLiteral("Kill switch controller is not initialized")};
    }
    if (state_ == State::Exiting) {
        return {true, 0, state_, {}};
    }
    if (!enabled_) {
        activeOperationId_ = 0;
        state_ = State::Exiting;
        lastError_.clear();
        return {true, 0, state_, {}};
    }

    const State originalState = state_;
    auto result = ensureBaselineLocked();
    if (!result) {
        return prepareFailureLocked(originalState, result.error);
    }
    result = removeTunAllowanceLocked();
    if (!result) {
        return prepareFailureLocked(originalState, result.error);
    }

    activeOperationId_ = 0;
    state_ = State::Exiting;
    lastError_.clear();
    return {true, 0, state_, {}};
}

KillSwitchController::Snapshot KillSwitchController::snapshot() const {
    QMutexLocker locker(&mutex_);
    return {
        initialized_,
        enabled_,
        recoveredStaleProtection_,
        state_,
        backendState_,
        allowedTun_,
        activeOperationId_,
        lastError_,
    };
}

bool KillSwitchController::invariantHolds(QString *reason) const {
    QMutexLocker locker(&mutex_);
    return invariantHoldsLocked(reason);
}

QString KillSwitchController::stateName(const State state) {
    switch (state) {
        case State::Disabled:
            return QStringLiteral("Disabled");
        case State::Connecting:
            return QStringLiteral("Connecting");
        case State::Connected:
            return QStringLiteral("Connected");
        case State::Switching:
            return QStringLiteral("Switching");
        case State::Reconnecting:
            return QStringLiteral("Reconnecting");
        case State::Stopping:
            return QStringLiteral("Stopping");
        case State::Disconnected:
            return QStringLiteral("Disconnected");
        case State::Error:
            return QStringLiteral("Error");
        case State::Exiting:
            return QStringLiteral("Exiting");
    }
    return QStringLiteral("Unknown");
}

KillSwitchResult KillSwitchController::ensureBaselineLocked() {
    const auto result = backend_.ensureBaseline();
    if (!result) {
        return backendFailureLocked(QStringLiteral("establish kill switch baseline"),
                                    result);
    }
    backendState_.baselineActive = true;
    return KillSwitchResult::Success();
}

KillSwitchResult KillSwitchController::removeTunAllowanceLocked() {
    const auto result = backend_.removeTunAllowance();
    if (!result) {
        return backendFailureLocked(QStringLiteral("remove TUN allowance"), result);
    }
    backendState_.tunAllowanceActive = false;
    allowedTun_ = {};
    return KillSwitchResult::Success();
}

KillSwitchResult KillSwitchController::backendFailureLocked(
    const QString &action, const KillSwitchResult &result) {
    lastError_ = normalizedError(action, result);
    return KillSwitchResult::Failure(lastError_);
}

KillSwitchController::PrepareResult KillSwitchController::prepareFailureLocked(
    const State originalState, const QString &error) const {
    // originalState is retained so a prepare failure never claims that the
    // caller may tear down a still-working connection.
    return {false, 0, originalState, error};
}

bool KillSwitchController::invariantHoldsLocked(QString *reason) const {
    const auto fail = [reason](const QString &message) {
        if (reason != nullptr) {
            *reason = message;
        }
        return false;
    };

    if (!initialized_) {
        if (enabled_ || state_ != State::Disabled || backendState_.anyActive() ||
            activeOperationId_ != 0) {
            return fail(QStringLiteral("Uninitialized controller has active state"));
        }
        return true;
    }

    if (state_ == State::Disabled) {
        if (enabled_ || backendState_.anyActive() || activeOperationId_ != 0) {
            return fail(QStringLiteral("Disabled state still owns protection"));
        }
        return true;
    }

    if (!enabled_) {
        if (state_ == State::Exiting && !backendState_.anyActive() &&
            activeOperationId_ == 0) {
            return true;
        }
        // Reconciliation/disable can fail while the persisted user setting is
        // off.  Error is the only non-enabled state allowed to represent that.
        if (state_ == State::Error && activeOperationId_ == 0) {
            return true;
        }
        return fail(QStringLiteral("Non-enabled controller is in an active state"));
    }

    if (state_ != State::Error) {
        if (!backendState_.baselineActive) {
            return fail(QStringLiteral("Protected state has no fail-closed baseline"));
        }
        if (!backendState_.dynamicCoreActive) {
            return fail(QStringLiteral("Protected state has no trusted-core allowance"));
        }
    }
    if (backendState_.tunAllowanceActive) {
        if (!backendState_.baselineActive || !backendState_.dynamicCoreActive) {
            return fail(QStringLiteral("TUN allowance exists without its prerequisites"));
        }
        if (!allowedTun_.isValid()) {
            return fail(QStringLiteral("TUN allowance has no interface identity"));
        }
    } else if (allowedTun_.isValid()) {
        return fail(QStringLiteral("Inactive TUN allowance retains an interface identity"));
    }

    const bool hasOperation = activeOperationId_ != 0;
    switch (state_) {
        case State::Disabled:
            return fail(QStringLiteral("Enabled controller reports Disabled"));
        case State::Connecting:
        case State::Switching:
            if (!hasOperation || !backendState_.dynamicCoreActive ||
                backendState_.tunAllowanceActive) {
                return fail(QStringLiteral("Connect/switch transition is not fail-closed"));
            }
            break;
        case State::Connected:
            if (hasOperation) {
                return fail(QStringLiteral("Connected state retains a start operation"));
            }
            break;
        case State::Reconnecting:
            if (backendState_.tunAllowanceActive ||
                (hasOperation && !backendState_.dynamicCoreActive)) {
                return fail(QStringLiteral("Reconnect transition is not fail-closed"));
            }
            break;
        case State::Stopping:
        case State::Disconnected:
        case State::Exiting:
            if (hasOperation || backendState_.tunAllowanceActive) {
                return fail(QStringLiteral("Non-connected state still allows a TUN"));
            }
            break;
        case State::Error:
            if (hasOperation) {
                return fail(QStringLiteral("Error state retains an active operation"));
            }
            break;
    }
    return true;
}

bool KillSwitchController::startAllowedLocked(const StartIntent intent) const {
    switch (intent) {
        case StartIntent::Connect:
            return state_ == State::Disconnected || state_ == State::Error;
        case StartIntent::Switch:
            return state_ == State::Connected;
        case StartIntent::Reconnect:
            return state_ == State::Connected || state_ == State::Disconnected ||
                   state_ == State::Reconnecting || state_ == State::Error;
    }
    return false;
}

quint64 KillSwitchController::nextOperationIdLocked() {
    ++operationCounter_;
    if (operationCounter_ == 0) {
        ++operationCounter_;
    }
    return operationCounter_;
}

} // namespace Configs_sys
