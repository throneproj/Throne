#pragma once

#include "include/engine/EnginePolicy.h"

namespace Engine {
    // No user to ask: never blocks, logs what a dialog would have shown, and takes the safe answer.
    class HeadlessPolicy final : public EnginePolicy {
    public:
        void geoAssetMissingCategory(const QString &contextName, const QString &needed, const QString &whichFile) override;

        bool confirmGeoAssetDownload(const QString &contextName) override;

        void geoAssetDownloadFinished(const QString &error) override;

        void killSwitchProblem() override;

        void killSwitchProblemResolved() override;

        void extraCoreNotPermitted() override;

        void startFailed(const StartRequest &request, StartOutcome outcome, const QString &title, const QString &error) override;

        void strictRouteUnavailable(const QString &error) override;

        bool offerCoreReset(const QString &error) override;

        bool offerLeftoverTunAdapterRetry(const QString &error) override;

        void stopFailed(const QString &error) override;

        SlowOperation slowOperation(SlowOperationKind kind, int delayMs) override;

        bool requestCorePrivileges() override;

        void systemProxyNeedsMixedInbound() override;

        bool vpnPromptOpen() const override;

        void vpnChallenge(const VpnAuthChallenge &challenge) override;

        void vpnCredentialsRejected(int profileId, const VpnAuthChallenge &challenge,
                                    const std::function<void(const QString &username, const QString &password)> &retryWith) override;

        void vpnChallengesCleared() override;

        void settingsNeedTunRestart() override;

        bool confirmRestartProgram() override;

        void testSessionBusy(const QString &text) override;

        void exitProgram(ExitReason reason, bool unattended) override;
    };
} // namespace Engine
