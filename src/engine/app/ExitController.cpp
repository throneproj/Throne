#include "include/engine/app/ExitController.h"

#include "include/api/remote/Server.hpp"
#include "include/engine/EnginePolicy.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/supervisor/CoreSupervisor.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/scanner/ScanManager.h"
#include "include/sys/KillSwitch.hpp"

#include <QDebug>

namespace Engine {
    ExitController::ExitController(EnginePolicy *policy, ConnectionManager *connection, CoreSupervisor *coreSupervisor, QObject *parent)
        : QObject(parent), m_policy(policy), m_connection(connection), m_coreSupervisor(coreSupervisor) {}

    bool ExitController::requestExit(ExitReason reason, bool unattended) {
        ENGINE_CHECK_MAIN_THREAD();
        if (unattended && reason != Engine::ExitReason::None && reason != Engine::ExitReason::Restart) return false;
        if (m_exitQueued || m_exitInProgress || Configs::dataManager->settingsRepo->prepare_exit) return false;
        m_exitQueued = true;
        QMetaObject::invokeMethod(this, [this, reason, unattended] {
            m_policy->exitProgram(reason, unattended);
            // Turned away by a window exit waiting on UAC: once that one is declined, a new request may run.
            if (!Configs::dataManager->settingsRepo->prepare_exit) m_exitQueued = false;
        }, Qt::QueuedConnection);
        return true;
    }

    void ExitController::shutdown(bool interactive) {
        ENGINE_CHECK_MAIN_THREAD();
        // Re-entry from a nested loop of the running teardown: mu_exit is not recursive, and the flag is set before any such loop.
        if (Configs::dataManager->settingsRepo->prepare_exit) return;
        qDebug() << "prepare for exit...";
        mu_exit.lock();
        if (Configs::dataManager->settingsRepo->prepare_exit)
        {
            qDebug() << "prepare exit had already succeeded, ignoring...";
            mu_exit.unlock();
            return;
        }
        Configs::dataManager->settingsRepo->prepare_exit = true;
        emit shutdownStarted();
        LOG_INFO("prepare_exit started, tearing down proxy/tun/core");
        // Before the waits below: their nested event loops would dispatch API requests mid-teardown.
        RemoteApi::Server::instance()->shutdown();
        // Whatever the live mode says: an uncheck may still have its clear queued.
        if (m_shutdownOptions.clearSystemProxy) m_connection->applySystemProxy(false, true);
        emit sessionPersisting();
        persistSessionState();
        Scanner::ScanManager::instance()->StopAll(true);
        Configs::dataManager->settingsRepo->noSave = true; // don't change Configs::dataManager->settingsRepo after this line
        m_connection->stop({false, true, false, interactive, m_shutdownOptions.stopRpcTimeoutMs, !m_shutdownOptions.stopTests});

        m_coreSupervisor->shutdown(m_shutdownOptions.coreKillWaitMs);
        emit coreShutDown();
        // Only once the core is gone: until then the guard keeps blocking whatever would leak around the dying tunnel.
        Sys::KillSwitch::instance()->shutdown();

        mu_exit.unlock();
        qDebug() << "prepare exit done!";
    }

    void ExitController::persistSessionState() {
        ENGINE_CHECK_MAIN_THREAD();
        qDebug() << "Start of data save";

        auto* settings = Configs::dataManager->settingsRepo.get();

        // Backstop only: this runs on a graceful exit, so it must never be the sole write.
        if (settings->remember_enable && settings->started_id >= 0) settings->remember_id = settings->started_id;
        if (m_shutdownOptions.persistModes) {
            settings->remember_system_proxy = settings->spmode_system_proxy;
            settings->remember_tun = settings->spmode_vpn;
        }

        settings->Save();
        qDebug() << "End of data save";
    }
} // namespace Engine
