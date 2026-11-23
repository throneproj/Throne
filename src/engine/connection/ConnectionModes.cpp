#include "include/engine/connection/ConnectionManager.h"

#include "include/api/RPC.h"
#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/entities/Profile.h"
#include "include/engine/EditLocks.h"
#include "include/engine/EnginePolicy.h"
#include "include/engine/EngineTr.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/connection/EngineState.h"
#include "include/engine/connection/StartCoordinator.h"
#include "include/global/Configs.hpp"
#include "include/global/LocalNetwork.hpp"
#include "include/global/Notice.hpp"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/sys/HostPresence.hpp"
#include "include/sys/HostTun.hpp"
#include "include/sys/KillSwitch.hpp"
#include "include/sys/SystemProxy.hpp"

#include <QThread>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace {
    constexpr qint64 kConnectionModesTunBlockedCacheMs = 5000;
    constexpr int kConnectionModesCapsRefreshMs = 1000;

    // networksetup and gsettings runs are slow, and profile start/stop call in from their own threads.
    QThread *connectionModesSystemProxyThread() {
        static auto *thread = [] {
            auto *t = new QThread;
            t->start();
            return t;
        }();
        return thread;
    }

    // From the core's capability report, why Tun fails whatever is granted: a privileged core that still lacks the device or
    // CAP_NET_ADMIN, or (canElevate false) any core lacking them. An unprivileged core may still gain them through elevation.
    QString connectionModesTunCapsBlocker(bool canElevate) {
        const auto caps = Configs::KnownCapabilities();
        if (!caps || (caps->tunDevice != false && caps->netAdmin != false)) return {};
        if (canElevate && !caps->privileged) return {};
        if (!caps->detail.isEmpty()) return caps->detail;
        return caps->tunDevice == false ? QStringLiteral("the core cannot open /dev/net/tun")
                                        : QStringLiteral("the core lacks CAP_NET_ADMIN");
    }
}

namespace Engine {
    bool ConnectionManager::ensureTunPrivileges(bool interactive) {
        if (Configs::dataManager->settingsRepo->disable_privilege_req)
        {
            MW_show_log(MainWindowTr::tr("User opted for no privilege req, some features may not work"));
            return true;
        }
        if (const auto blocker = connectionModesTunCapsBlocker(interactive); !blocker.isEmpty()) {
            MW_show_log(QStringLiteral("[Tun] The core cannot use Tun: %1").arg(blocker));
            if (interactive && !m_options.headless) PostNotice({NoticeLevel::Warning, MainWindowTr::tr("Tun mode"), blocker});
            return false;
        }
        if (tunPrivileged()) return true;
        if (!interactive) return false;
        return policy()->requestCorePrivileges();
    }

    bool ConnectionManager::tunPrivileged() {
        ENGINE_CHECK_MAIN_THREAD();
        if (const auto known = Configs::KnownAdmin()) return *known;
        return m_options.blockingPrivilegeCheck && Configs::IsAdmin();
    }

    QString ConnectionManager::tunBlockedReason() {
        ENGINE_CHECK_MAIN_THREAD();
        const auto &settings = Configs::dataManager->settingsRepo;
        if (settings->spmode_vpn || runningTun()) return {};
        // Our own core's Tun may still be coming up or going down, and would read as another owner's.
        if (const auto state = connectionState(); state == ConnectionState::Connecting || state == ConnectionState::Stopping) return {};
        const qint64 now = MonotonicMs();
        const int dnsProbePort = runningSnapshot() ? 0 : settings->core_dns_in_port;
        const auto key = QStringLiteral("%1|%2").arg(settings->vpn_tun_ipv4_cidr).arg(dnsProbePort);
        if (m_tunBlockedAtMs != 0 && key == m_tunBlockedKey && now - m_tunBlockedAtMs < kConnectionModesTunBlockedCacheMs) {
            return m_tunBlockedCache;
        }
        QString reason;
        if (m_options.headless) {
            reason = Sys::HostTun::Conflict(settings->vpn_tun_ipv4_cidr, dnsProbePort);
        } else if (const auto holder = Sys::HostPresence::OtherHeadless()) {
            reason = tr("A background Throne (PID %1) owns Tun on this computer; stop it or use thronectl.").arg(holder->pid);
        }
        m_tunBlockedCache = reason;
        m_tunBlockedKey = key;
        m_tunBlockedAtMs = std::max<qint64>(now, 1);
        return reason;
    }

    QString ConnectionManager::tunRestoreHoldReason() {
        // setTun's order: the host conflict refuses before the privilege and capability checks.
        if (auto why = tunBlockedReason(); !why.isEmpty()) return why;
        if (auto why = connectionModesTunCapsBlocker(false); !why.isEmpty()) return why;
        return QStringLiteral("the core lacks the privileges Tun needs");
    }

    void ConnectionManager::applySystemProxy(bool enable, bool wait) {
        const auto &settings = Configs::dataManager->settingsRepo;
        const auto host = LocalNetwork::InboundConnectHost();
        const auto port = settings->inbound_socks_port;
        const auto format = settings->proxy_scheme;
        runOnThread([=] {
            QString error;
            if (!enable) {
                error = SystemProxy_Clear();
            } else if (Configs::dataManager->settingsRepo->spmode_system_proxy) {
                // Rechecked: a profile start can queue this after the box was unchecked.
                error = SystemProxy_Apply(host, port, format);
            }
            if (!error.isEmpty()) MW_show_log(MainWindowTr::tr("System proxy: %1").arg(error));
        }, connectionModesSystemProxyThread(), wait);
    }

    bool ConnectionManager::setSystemProxy(bool enable, const ModeChange &change) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto &settings = Configs::dataManager->settingsRepo;
        if (!m_options.systemProxy) {
            if (enable) {
                MW_show_log(QStringLiteral("System proxy is not available in headless mode; use Tun or point clients at the "
                                           "mixed inbound."));
                emit statusChanged();
            }
            return false;
        }
        if (enable && settings->disable_mixed_inbound) {
            if (change.interactive) {
                runOnUiThread([=, this] {
                   policy()->systemProxyNeedsMixedInbound();
                });
            }
            emit statusChanged();
            return false;
        }
        settings->spmode_system_proxy = enable;
        bool restarted = false;
        if (const auto running = runningSnapshot()) {
            applySystemProxy(enable);
            if (!enable && settings->reset_proxy_on_disable_sp && change.restart) {
                restarted = m_coordinator->restart(running->id, change.interactive, change.restartSerial);
            }
        }

        if (change.save) {
            settings->remember_system_proxy = enable;
            settings->Save();
        }

        emit statusChanged();
        return restarted;
    }

    bool ConnectionManager::setTun(bool enable, const ModeChange &change) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto &settings = Configs::dataManager->settingsRepo;
        if (!enable && std::exchange(m_tunRestoreHeld, false)) {
            // A held restore left spmode_vpn off, so the early return below would keep the remembered Tun for the next boot.
            if (change.save) {
                settings->remember_tun = false;
                settings->Save();
            }
            emit statusChanged();
        }
        if (enable == settings->spmode_vpn) return false;

        if (enable) {
            // Before any elevation prompt: nothing granted here clears another owner's Tun.
            if (const auto reason = tunBlockedReason(); !reason.isEmpty()) {
                MW_show_log(QStringLiteral("[Tun] Not enabling Tun: %1").arg(reason));
                // Headless has no notice sink, so a notice there would only repeat the line above.
                if (!m_options.headless) PostNotice({NoticeLevel::Warning, MainWindowTr::tr("Tun mode"), reason});
                emit statusChanged();
                return false;
            }
            Configs::RefreshStaleTunDevice(kConnectionModesCapsRefreshMs);
        }
        if (enable && (!tunPrivileged() || !connectionModesTunCapsBlocker(true).isEmpty()) &&
            !ensureTunPrivileges(change.interactive)) {
            emit statusChanged();
            return false;
        }
        m_tunRestoreHeld = false;
        m_tunBlockedAtMs = 0;

        if (change.save) {
            // Written here, after the elevation check, so a failed enable is not remembered.
            settings->remember_tun = enable;
            settings->Save();
        }

        settings->spmode_vpn = enable;
        emit statusChanged();

        if (!change.restart) return false;
        return m_coordinator->restartResumeTarget(change.interactive, change.restartSerial);
    }

    bool ConnectionManager::chooseRoute(int routeId, bool interactive, quint64 restartSerial) {
        ENGINE_CHECK_MAIN_THREAD();
        const auto &settings = Configs::dataManager->settingsRepo;
        if (settings->current_route_id == routeId) return false;
        // The modeless editor writes current_route_id back on OK.
        if (IsBeingEdited(EditTarget::Routes)) {
            MW_show_log(MainWindowTr::tr("Close Routing Settings before switching the routing profile."));
            return false;
        }
        settings->current_route_id = routeId;
        settings->Save();
        if (m_state->resumeTarget() < 0) return false;
        if (const auto running = runningSnapshot(); running != nullptr && Configs::RunningRouteId() >= 0 &&
                                                    Configs::RouteIdFor(running->gid) == Configs::RunningRouteId()) {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(running->gid);
            MW_show_log(MainWindowTr::tr("The group \"%1\" uses its own routing profile, so the running profile keeps it.")
                            .arg(group != nullptr ? group->name : QString::number(running->gid)));
            return false;
        }
        return m_coordinator->restartResumeTarget(interactive, restartSerial);
    }

    void ConnectionManager::attachKillSwitch() {
        connect(Sys::KillSwitch::instance(), &Sys::KillSwitch::stateChanged, this, [this] { onKillSwitchStateChanged(); });
        QTimer::singleShot(0, this, [this] { onKillSwitchStateChanged(); });
    }

    void ConnectionManager::onKillSwitchStateChanged() {
        const auto state = Sys::KillSwitch::instance()->state();
        const bool failed = state == Sys::KillSwitch::State::Failed;
        const bool armed = state == Sys::KillSwitch::State::Armed;
        const bool enteredFailed = failed && !m_killSwitchWasFailed;
        const bool enteredArmed = armed && !m_killSwitchWasArmed;
        m_killSwitchWasFailed = failed;
        m_killSwitchWasArmed = armed;

        emit statusChanged();
        if (!failed) policy()->killSwitchProblemResolved();
        if (enteredFailed) policy()->killSwitchProblem();
        if (enteredArmed) m_coordinator->onGuardArmed();
    }

    void ConnectionManager::disableKillSwitch() {
        Configs::dataManager->settingsRepo->kill_switch = false;
        Configs::dataManager->settingsRepo->Save();
        Sys::KillSwitch::instance()->apply();
    }

    void ConnectionManager::noteRestartNeeded(RestartReason reason) {
        m_coordinator->noteRestartNeeded(reason);
    }

    void ConnectionManager::clearRestartNeeded() {
        m_coordinator->clearRestartNeeded();
    }

    QList<RestartReason> ConnectionManager::restartReasons() const {
        return m_coordinator->restartReasons();
    }

    bool ConnectionManager::restartPending() const {
        return m_coordinator->restartPending();
    }
} // namespace Engine
