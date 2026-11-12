#include "include/engine/headless/HeadlessPolicy.h"

#include <QCoreApplication>
#include <QPointer>
#include <QTimer>

#include "include/engine/EngineHost.h"
#include "include/engine/app/ExitCodes.h"
#include "include/engine/headless/HeadlessRuntime.h"
#include "include/engine/vpn/VpnAuthController.h"
#include "include/engine/vpn/VpnEndpoints.h"
#include "include/global/Configs.hpp"
#include "include/sys/KillSwitch.hpp"

namespace Engine {
    namespace {
        // A sign-in prompt nothing here can answer is cancelled at its deadline, or after this when it names none.
        constexpr int kHeadlessPolicyChallengeCapSec = 300;
    } // namespace

    void HeadlessPolicy::geoAssetMissingCategory(const QString &contextName, const QString &needed, const QString &whichFile) {
        MW_show_log(QStringLiteral("[Geo assets] The Xray config \"%1\" needs \"%2\", which the installed %3 does not contain; "
                                   "point the GeoIP/GeoSite asset URL at a source that provides it.")
                        .arg(contextName, needed, whichFile));
    }

    bool HeadlessPolicy::confirmGeoAssetDownload(const QString &contextName) {
        MW_show_log(QStringLiteral("[Geo assets] Downloading geoip.dat / geosite.dat for the Xray config \"%1\".").arg(contextName));
        return true;
    }

    void HeadlessPolicy::geoAssetDownloadFinished(const QString &error) {
        if (error.isEmpty()) {
            MW_show_log(QStringLiteral("[Geo assets] Installed; a start that failed for them is retried once."));
        } else {
            MW_show_log(QStringLiteral("[Geo assets] Download failed: %1").arg(error.trimmed()));
        }
    }

    void HeadlessPolicy::killSwitchProblem() {
        const auto *killSwitch = Sys::KillSwitch::instance();
        if (killSwitch->state() != Sys::KillSwitch::State::Failed) return;
        // failIdentity already logged "Profiles refused"; the guard still blocks, so "Not active" would be wrong.
        if (killSwitch->failedForIdentity()) return;
        QString hint;
#if defined(Q_OS_WIN)
        if (killSwitch->failedForPrivileges()) hint = QStringLiteral("run Throne as a service or elevated");
#else
        if (killSwitch->failedForPrivileges()) hint = QStringLiteral("it needs root (euid 0)");
#endif
#ifdef Q_OS_LINUX
        // The core's guard reports a capabilities-only install as unsupported.
        if (killSwitch->failureCode() == QStringLiteral("unsupported") && !killSwitch->identityUnsupported())
            hint = QStringLiteral("a unit that grants capabilities instead of root cannot use it");
#endif
        MW_show_log(QStringLiteral("[Kill switch] Not active (%1): %2%3")
                        .arg(killSwitch->failureCode(), killSwitch->failureText().simplified(),
                             hint.isEmpty() ? QString() : QStringLiteral(" Hint: %1.").arg(hint)));
    }

    void HeadlessPolicy::killSwitchProblemResolved() {}

    void HeadlessPolicy::extraCoreNotPermitted() {
        MW_show_log(QStringLiteral("[Kill switch] The profile was not started: its extra core could not be allowed through the kill switch."));
    }

    void HeadlessPolicy::startFailed(const StartRequest &request, StartOutcome, const QString &title, const QString &error) {
        MW_show_log(QStringLiteral("Profile %1 failed to start: %2: %3").arg(request.profileId).arg(title, error));
    }

    void HeadlessPolicy::strictRouteUnavailable(const QString &error) {
        MW_show_log(QStringLiteral("Windows could not enable strict routing; disable Strict Route in the Tun settings. Error: %1").arg(error));
    }

    bool HeadlessPolicy::offerCoreReset(const QString &error) {
        MW_show_log(QStringLiteral("Tun did not come up, the core is left running: %1").arg(error));
        return false;
    }

    bool HeadlessPolicy::offerLeftoverTunAdapterRetry(const QString &error) {
        MW_show_log(QStringLiteral("Windows still held the Tun adapter of an earlier session; start the profile again: %1").arg(error));
        return false;
    }

    void HeadlessPolicy::stopFailed(const QString &error) {
        MW_show_log(QStringLiteral("Stop return error: %1").arg(error));
    }

    SlowOperation HeadlessPolicy::slowOperation(SlowOperationKind kind, int delayMs) {
        const auto what = kind == SlowOperationKind::Start ? QStringLiteral("start") : QStringLiteral("stop");
        auto *timer = new QTimer(QCoreApplication::instance());
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, timer, [what, delayMs] {
            MW_show_log(QStringLiteral("The profile %1 has not finished after %2 ms.").arg(what).arg(delayMs));
        });
        timer->start(delayMs);
        return SlowOperation([guard = QPointer<QTimer>(timer)] {
            if (!guard) return;
            guard->stop();
            guard->deleteLater();
        });
    }

    bool HeadlessPolicy::requestCorePrivileges() {
#if defined(Q_OS_WIN)
        MW_show_log(QStringLiteral("The core lacks the privileges Tun needs: run Throne as a service or from an elevated prompt."));
#elif defined(Q_OS_MACOS)
        MW_show_log(QStringLiteral("The core lacks the privileges Tun needs: run Throne as a LaunchDaemon (root) or install a setuid core."));
#else
        MW_show_log(QStringLiteral("The core lacks the privileges Tun needs: run the unit as root, grant it CAP_NET_ADMIN, "
                                   "or install a setuid core."));
#endif
        return false;
    }

    void HeadlessPolicy::systemProxyNeedsMixedInbound() {
        MW_show_log(QStringLiteral("Cannot set system proxy when mixed inbound is disabled."));
    }

    bool HeadlessPolicy::vpnPromptOpen() const {
        return false;
    }

    void HeadlessPolicy::vpnChallenge(const VpnAuthChallenge &challenge) {
        MW_show_log(QStringLiteral("[VPN] %1 asks for a %2 sign-in that nothing here can answer; it is cancelled at its deadline "
                                   "(at most %3 s).")
                        .arg(Stats::VpnEndpointDisplayName(challenge.endpointTag), challenge.kind)
                        .arg(kHeadlessPolicyChallengeCapSec));
        if (auto *engine = host()) engine->vpnAuth()->cancelAtDeadline(challenge, kHeadlessPolicyChallengeCapSec);
    }

    void HeadlessPolicy::vpnCredentialsRejected(int profileId, const VpnAuthChallenge &challenge,
                                                const std::function<void(const QString &, const QString &)> &) {
        MW_show_log(QStringLiteral("[VPN] %1 (profile %2) rejected the saved credentials; update them in the profile (status: vpn_auth_failed).")
                        .arg(Stats::VpnEndpointDisplayName(challenge.endpointTag), QString::number(profileId)));
    }

    void HeadlessPolicy::vpnChallengesCleared() {}

    void HeadlessPolicy::settingsNeedTunRestart() {
        MW_show_log(QStringLiteral("Tun settings changed; they take effect when Tun restarts."));
    }

    bool HeadlessPolicy::confirmRestartProgram() {
        MW_show_log(QStringLiteral("A changed setting needs a program restart; it takes effect after the next restart."));
        return false;
    }

    void HeadlessPolicy::testSessionBusy(const QString &text) {
        MW_show_log(text);
    }

    void HeadlessPolicy::exitProgram(ExitReason reason, bool) {
        switch (reason) {
            case ExitReason::None:
                HeadlessRuntime::RequestExit(ExitCode::Ok, QStringLiteral("shutdown requested"));
                return;
            case ExitReason::Restart:
                HeadlessRuntime::RequestExit(ExitCode::Restart, QStringLiteral("restart requested"));
                return;
            case ExitReason::RunUpdater:
                MW_show_log(QStringLiteral("Running the updater is not available in headless mode; ignored."));
                return;
            case ExitReason::RestartWithTun:
                MW_show_log(QStringLiteral("Restarting with Tun is not available in headless mode; ignored."));
                return;
            case ExitReason::RestartElevated:
                MW_show_log(QStringLiteral("Restarting elevated is not available in headless mode; ignored."));
                return;
        }
    }
} // namespace Engine
