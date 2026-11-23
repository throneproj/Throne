#include "include/engine/connection/ConnectionManager.h"

#include "include/database/DatabaseManager.h"
#include "include/engine/connection/EngineState.h"
#include "include/engine/connection/StartCoordinator.h"
#include "include/engine/supervisor/CoreSupervisor.h"
#include "include/global/Utils.hpp"
#include "include/sys/KillSwitch.hpp"

#include <memory>

namespace Engine {
    ConnectionManager::ConnectionManager(EnginePolicy *policy, CoreSupervisor *coreSupervisor, StatsService *stats,
                                         const RuntimeOptions &options, QObject *parent)
        : QObject(parent), m_policy(policy), m_state(std::make_shared<EngineState>()), m_options(options) {
        m_coordinator = new StartCoordinator(m_state, policy, coreSupervisor, stats, options,
                                             {.applySystemProxy = [this](bool enable) { applySystemProxy(enable); }}, this);
        // Direct, so every subscriber sees a signal inside the coordinator's emit, as when this object emitted it itself.
        connect(m_coordinator, &StartCoordinator::startFinished, this, &ConnectionManager::startFinished, Qt::DirectConnection);
        connect(m_coordinator, &StartCoordinator::startDispatching, this, &ConnectionManager::startDispatching, Qt::DirectConnection);
        connect(m_coordinator, &StartCoordinator::stopFinished, this, &ConnectionManager::stopFinished, Qt::DirectConnection);
        connect(m_coordinator, &StartCoordinator::connectionStateChanged, this, [this](ConnectionState state) {
            // Whether a profile runs decides the dns-in port probe, and a Tun of ours may just have gone.
            m_tunBlockedAtMs = 0;
            emit connectionStateChanged(state);
        }, Qt::DirectConnection);
        connect(m_coordinator, &StartCoordinator::statusChanged, this, &ConnectionManager::statusChanged, Qt::DirectConnection);
        connect(m_coordinator, &StartCoordinator::busyChanged, this, &ConnectionManager::busyChanged, Qt::DirectConnection);
        connect(m_coordinator, &StartCoordinator::profileRowsChanged, this, &ConnectionManager::profileRowsChanged, Qt::DirectConnection);
        connect(m_coordinator, &StartCoordinator::runMarker, this, &ConnectionManager::runMarker, Qt::DirectConnection);
        connect(m_coordinator, &StartCoordinator::restartReasonsChanged, this, &ConnectionManager::restartReasonsChanged, Qt::DirectConnection);
        connect(coreSupervisor, &CoreSupervisor::coreStarted, this, &ConnectionManager::onCoreStarted);
        connect(coreSupervisor, &CoreSupervisor::coreGaveUp, m_coordinator, &StartCoordinator::onCoreGaveUp);
        m_state->lastTarget = Configs::dataManager->settingsRepo->remember_id;
    }

    void ConnectionManager::attach(TestRunner *tests, AutoSelectorController *autoSelector, VpnAuthController *vpnAuth) {
        m_coordinator->attach(tests, autoSelector, vpnAuth);
    }

    void ConnectionManager::start(const StartRequest &request) {
        m_coordinator->requestStart(request);
    }

    void ConnectionManager::stop(const StopOptions &options) {
        m_coordinator->requestStop(options);
    }

    bool ConnectionManager::stopPending() const {
        return m_coordinator->stopPending();
    }

    void ConnectionManager::applyPendingRestart() {
        m_coordinator->applyPendingRestart();
    }

    void ConnectionManager::setBootStart(const StartRequest &request) {
        m_coordinator->setBootStart(request);
    }

    void ConnectionManager::onCoreStarted(int profileId) {
        auto &settings = Configs::dataManager->settingsRepo;
        m_coordinator->noteCoreUp();
        // The core may just have been given root, which the guard needs too.
        if (Sys::KillSwitch::instance()->failedForPrivileges()) Sys::KillSwitch::instance()->apply();
        QString tunHoldWhy;
        if (m_options.headless) {
            const bool wantTun = settings->remember_tun || settings->flag_restart_tun_on;
            if (wantTun) {
                setTun(true, ModeChange{.save = false, .interactive = false, .restart = false});
                settings->flag_restart_tun_on = false;
            }
            // setTun's return value is not success, so the hold is judged by the resulting state.
            m_tunRestoreHeld = m_options.tunRequired && wantTun && !settings->spmode_vpn;
            if (m_tunRestoreHeld) {
                tunHoldWhy = tunRestoreHoldReason();
                MW_show_log(QStringLiteral("[Tun] The remembered Tun mode could not be restored: %1 (degraded: tun_unavailable).")
                                .arg(tunHoldWhy));
            }
        } else {
            if (settings->remember_enable && settings->remember_system_proxy) {
                setSystemProxy(true, ModeChange{.save = false});
            }
            if ((settings->remember_enable && settings->remember_tun) || settings->flag_restart_tun_on) {
                setTun(true, ModeChange{.save = settings->flag_restart_tun_on});
                settings->flag_restart_tun_on = false;
            }
        }
        m_coordinator->takeCoreStart(profileId, m_tunRestoreHeld, tunHoldWhy);
        emit statusChanged();
    }

    int ConnectionManager::resolveLastProfile() const {
        return m_coordinator->resolveLastProfile();
    }

    int ConnectionManager::lastProfileId() const {
        return m_state->lastTarget;
    }

    ConnectionState ConnectionManager::connectionState() const {
        return m_state->connectionState();
    }

    std::shared_ptr<Configs::Profile> ConnectionManager::runningSnapshot() const {
        return m_state->running();
    }

    bool ConnectionManager::runningTun() const {
        return m_state->runningTun();
    }

    quint64 ConnectionManager::nextStartSerial() {
        return m_coordinator->nextStartSerial();
    }

    std::shared_ptr<EngineState> ConnectionManager::state() const {
        return m_state;
    }
} // namespace Engine
