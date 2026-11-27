#pragma once

#include <functional>
#include <utility>

#include <QList>
#include <QString>

#include "include/engine/EngineTypes.h"
#include "include/engine/vpn/VpnAuth.h"

namespace Engine {
    enum class SlowOperationKind {
        Start,
        Stop,
    };

    // Copyable; dismiss() runs on the main thread and may run any number of times.
    class SlowOperation {
    public:
        SlowOperation() = default;

        explicit SlowOperation(std::function<void()> dismiss) : dismiss_(std::move(dismiss)) {}

        void dismiss() const {
            if (dismiss_) dismiss_();
        }

    private:
        std::function<void()> dismiss_;
    };

    // Every point where the engine asks or tells the user something; MainWindow answers with dialogs, HeadlessPolicy without.
    // Main thread only. A method that returns an answer may block for it (user-started paths only); the others never block, and
    // an `answered` callback runs exactly once on the main thread, possibly before the call returns.
    class EnginePolicy {
    public:
        virtual ~EnginePolicy() = default;

        virtual void geoAssetMissingCategory(const QString &contextName, const QString &needed, const QString &whichFile) = 0;

        // Whether to download the missing geoip.dat / geosite.dat now.
        virtual void confirmGeoAssetDownload(const QString &contextName, std::function<void(bool download)> answered) = 0;

        // error is empty when every file arrived.
        virtual void geoAssetDownloadFinished(const QString &error) = 0;

        // The kill switch is Failed, or it refused a start; Sys::KillSwitch holds the details.
        virtual void killSwitchProblem() = 0;

        // The kill switch changed to any state but Failed.
        virtual void killSwitchProblemResolved() = 0;

        // The kill switch could not admit a profile's extra core, so that start was refused.
        virtual void extraCoreNotPermitted() = 0;

        // Interactive requests only; startFinished has already carried the outcome.
        virtual void startFailed(const StartRequest &request, StartOutcome outcome, const QString &title, const QString &error) = 0;

        virtual void strictRouteUnavailable(const QString &error) = 0;

        // Tun did not come up; whether to reset the core.
        virtual void offerCoreReset(const QString &error, std::function<void(bool reset)> answered) = 0;

        // Windows still held the Tun adapter of a session that did not close cleanly; whether to start the profile again.
        virtual void offerLeftoverTunAdapterRetry(const QString &error, std::function<void(bool retry)> answered) = 0;

        // Interactive stops only.
        virtual void stopFailed(const QString &error) = 0;

        // Surfaces once the operation has run for delayMs; the caller dismisses it when the operation ends.
        virtual SlowOperation slowOperation(SlowOperationKind kind, int delayMs) = 0;

        // Interactive callers only, once the core turned out to lack the privileges Tun needs; whether it has them now.
        // confirmed: the user already chose to grant them, so only the system's own prompt remains.
        virtual bool requestCorePrivileges(bool confirmed) = 0;

        virtual void systemProxyNeedsMixedInbound() = 0;

        // While a sign-in prompt is open the engine holds back new challenges and auth-failure restarts.
        virtual bool vpnPromptOpen() const = 0;

        virtual void vpnChallenge(const VpnAuthChallenge &challenge) = 0;

        // retryWith restarts the profile with session-only credentials.
        virtual void vpnCredentialsRejected(int profileId, const VpnAuthChallenge &challenge,
                                            const std::function<void(const QString &username, const QString &password)> &retryWith) = 0;

        // An endpoint other than the exit refused its sign-in; whether to restart the profile, dropping its open connections,
        // so the endpoint signs in again with a new one-time code.
        virtual void offerVpnSignInRestart(const QString &endpointName, const QString &error,
                                           std::function<void(bool restart)> answered) = 0;

        // The run the pending prompts belong to has ended.
        virtual void vpnChallengesCleared() = 0;

        virtual void settingsNeedTunRestart() = 0;

        // Whether to restart the program now for a setting that needs it.
        virtual bool confirmRestartProgram() = 0;

        // Interactive test requests only.
        virtual void testSessionBusy(const QString &text) = 0;

        // A URL test run left these profiles untested because the local network was down; their earlier results stay.
        virtual void testsInconclusive(const QList<int> &profileIds) = 0;

        // Ends the program the way the front end does (relaunch, updater, elevation); the engine passes only None and Restart.
        // unattended: no dialog during the teardown.
        virtual void exitProgram(ExitReason reason, bool unattended) = 0;
    };
} // namespace Engine
