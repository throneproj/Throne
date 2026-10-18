#include "include/engine/headless/SupervisorLink.h"

#include <limits>
#include <utility>

#include <QTimer>

#include "include/engine/app/ExitController.h"
#include "include/engine/headless/RuntimeStatus.h"
#include "include/global/Logger.hpp"
#include "include/sys/ServiceManager.hpp"

namespace Engine {
    namespace {
        constexpr int kSupervisorLinkExtendEveryMs = 20000;
        constexpr qint64 kSupervisorLinkExtendUsec = 60LL * 1000 * 1000;
        constexpr qint64 kSupervisorLinkMinWatchdogMs = 1000;
    } // namespace

    SupervisorLink::SupervisorLink(QObject *parent) : QObject(parent) {}

    void SupervisorLink::attach(RuntimeStatus *status, ExitController *exit) {
        m_status = status;
        if (status != nullptr) {
            connect(status, &RuntimeStatus::readyChanged, this, &SupervisorLink::onReady, Qt::DirectConnection);
            connect(status, &RuntimeStatus::summaryChanged, this, &SupervisorLink::onSummary, Qt::DirectConnection);
        }
        if (exit != nullptr) connect(exit, &ExitController::shutdownStarted, this, &SupervisorLink::onStopping, Qt::DirectConnection);
    }

    void SupervisorLink::start() {
        if (const qint64 usec = Sys::ServiceManager::WatchdogUsec(); usec > 0 && m_watchdog == nullptr) {
            const qint64 intervalMs = qMin<qint64>(qMax(kSupervisorLinkMinWatchdogMs, usec / 3000), std::numeric_limits<int>::max());
            m_watchdog = new QTimer(this);
            m_watchdog->setTimerType(Qt::PreciseTimer);
            m_watchdog->setInterval(static_cast<int>(intervalMs));
            connect(m_watchdog, &QTimer::timeout, this, [] { Sys::ServiceManager::NotifyWatchdog(); });
            m_watchdog->start();
            Sys::ServiceManager::NotifyWatchdog();
            LOG_INFO(QStringLiteral("[systemd] Watchdog of %1 ms; pinging every %2 ms.").arg(usec / 1000).arg(intervalMs));
        }
        if (!Sys::ServiceManager::NotifyEnabled() || m_ready || m_stopping || m_extend != nullptr) return;
        m_extend = new QTimer(this);
        m_extend->setInterval(kSupervisorLinkExtendEveryMs);
        connect(m_extend, &QTimer::timeout, this, [this] {
            if (m_ready || m_stopping) {
                m_extend->stop();
                return;
            }
            Sys::ServiceManager::NotifyExtendTimeout(kSupervisorLinkExtendUsec);
        });
        m_extend->start();
        Sys::ServiceManager::NotifyExtendTimeout(kSupervisorLinkExtendUsec);
    }

    void SupervisorLink::setInPlaceRestart(std::function<bool()> restarting) {
        m_inPlaceRestart = std::move(restarting);
    }

    void SupervisorLink::onReady() {
        if (std::exchange(m_ready, true) || m_stopping) return;
        if (m_extend != nullptr) m_extend->stop();
        m_lastStatus = m_status != nullptr ? m_status->summary() : QStringLiteral("Ready");
        Sys::ServiceManager::NotifyReady(m_lastStatus);
        // READY ends a reload this image inherited.
        m_reloadSent = false;
    }

    void SupervisorLink::onSummary(const QString &summary) {
        if (m_stopping || summary == m_lastStatus) return;
        m_lastStatus = summary;
        Sys::ServiceManager::NotifyStatus(summary);
    }

    void SupervisorLink::onStopping() {
        if (std::exchange(m_stopping, true)) return;
        if (m_extend != nullptr) m_extend->stop();
        if (m_inPlaceRestart && m_inPlaceRestart()) {
            m_lastStatus = QStringLiteral("Restarting");
            Sys::ServiceManager::NotifyReloading(m_lastStatus);
            // Before READY the unit is still starting, and systemd ignores RELOADING there; an inherited reload is still open.
            m_reloadSent = m_reloadSent || m_ready;
            return;
        }
        m_lastStatus = QStringLiteral("Stopping");
        Sys::ServiceManager::NotifyStopping(m_lastStatus);
    }

    // systemd <= 248 handles a main-process exit in the notify reload state only at the reload timeout; READY ends that state
    // and STOPPING then stops from running. Two datagrams: within one, the last of READY/RELOADING/STOPPING wins.
    void SupervisorLink::abandonReload() {
        if (!std::exchange(m_reloadSent, false)) return;
        m_lastStatus = QStringLiteral("Stopping");
        Sys::ServiceManager::NotifyReady(m_lastStatus);
        Sys::ServiceManager::NotifyStopping(m_lastStatus);
    }
} // namespace Engine
