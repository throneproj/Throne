#include "include/engine/headless/RuntimeStatus.h"

#include <QCoreApplication>

#include "include/api/RPC.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineHost.h"
#include "include/engine/app/ExitController.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/supervisor/CoreSupervisor.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/global/Utils.hpp"
#include "include/sys/KillSwitch.hpp"

namespace Engine {
    namespace {
        constexpr quint8 kRuntimeStatusAllSteps = RuntimeStatus::ApiStep | RuntimeStatus::CoreStep | RuntimeStatus::KillSwitchStep |
                                                  RuntimeStatus::ResumeStep;

        // sd_notify STATUS and the log are line-oriented; profile names may carry anything.
        QString runtimeStatusOneLine(QString text) {
            for (auto &ch : text) {
                const auto category = ch.category();
                if (category == QChar::Other_Control || category == QChar::Separator_Line || category == QChar::Separator_Paragraph) ch = u' ';
            }
            return text;
        }

        QString runtimeStatusProfileName(const std::shared_ptr<Configs::Profile> &profile, int fallbackId) {
            auto ent = profile;
            if (ent == nullptr && fallbackId >= 0) ent = Configs::dataManager->profilesRepo->GetProfile(fallbackId);
            if (ent == nullptr) return fallbackId >= 0 ? QStringLiteral("profile %1").arg(fallbackId) : QStringLiteral("profile");
            const QString name = ent->outbound != nullptr ? ent->outbound->DisplayName() : ent->name;
            return name.isEmpty() ? QStringLiteral("profile %1").arg(ent->id) : name;
        }
    } // namespace

    RuntimeStatus::RuntimeStatus(EngineHost *host, qint64 bootMonoMs, QObject *parent)
        : QObject(parent), m_host(host), m_bootMonoMs(bootMonoMs) {
        auto *connection = host->connection();
        auto *supervisor = host->supervisor();
        connect(connection, &ConnectionManager::connectionStateChanged, this, [this](ConnectionState state) {
            m_transitionState = state;
            m_transitionStartMs = state == ConnectionState::Connecting || state == ConnectionState::Stopping ? MonotonicMs() : 0;
            refreshSummary();
        }, Qt::DirectConnection);
        connect(connection, &ConnectionManager::statusChanged, this, &RuntimeStatus::refreshSummary, Qt::DirectConnection);
        connect(supervisor, &CoreSupervisor::stateChanged, this, [this] {
            // Each core state is bounded on its own (connect timeout, backoff cap); only one that never moves on counts.
            m_coreUnusableSinceMs = 0;
            refreshSummary();
        }, Qt::DirectConnection);
        connect(supervisor, &CoreSupervisor::rpcConnected, this, &RuntimeStatus::refreshSummary, Qt::DirectConnection);
        // After the ConnectionManager's slot: by now the boot start was dispatched or held.
        connect(supervisor, &CoreSupervisor::coreStarted, this, [this] {
            mark(CoreStep);
            mark(ResumeStep);
        }, Qt::DirectConnection);
        connect(host->exitController(), &ExitController::shutdownStarted, this, [this] {
            m_shuttingDown = true;
            refreshSummary();
        }, Qt::DirectConnection);
        connect(Sys::KillSwitch::instance(), &Sys::KillSwitch::stateChanged, this, &RuntimeStatus::refreshSummary);
        m_summary = summary();
    }

    void RuntimeStatus::mark(Step step) {
        if ((m_steps & step) != 0) return;
        const bool wasReady = ready();
        m_steps |= step;
        if (!wasReady && ready()) emit readyChanged();
        refreshSummary();
    }

    bool RuntimeStatus::ready() const {
        return (m_steps & kRuntimeStatusAllSteps) == kRuntimeStatusAllSteps;
    }

    QStringList RuntimeStatus::pendingSteps() const {
        QStringList pending;
        if ((m_steps & ApiStep) == 0) pending << QStringLiteral("api");
        if ((m_steps & CoreStep) == 0) pending << QStringLiteral("core");
        if ((m_steps & KillSwitchStep) == 0) pending << QStringLiteral("kill_switch");
        if ((m_steps & ResumeStep) == 0) pending << QStringLiteral("resume");
        return pending;
    }

    bool RuntimeStatus::coreUsable() const {
        return m_host->supervisor()->state() == CoreState::Connected && API::defaultClient != nullptr && API::defaultClient->IsConnected();
    }

    qint64 RuntimeStatus::stuckSeconds() const {
        qint64 overMs = 0;
        if (const qint64 connecting = connectingMs(); connecting > 0) {
            overMs = connecting - kStuckConnectingMs;
        } else if (const qint64 stopping = stoppingMs(); stopping > 0) {
            overMs = stopping - kStuckStoppingMs;
        }
        return overMs > 0 ? (overMs + 999) / 1000 : 0;
    }

    qint64 RuntimeStatus::connectingMs() const {
        if (m_transitionStartMs == 0 || m_transitionState != ConnectionState::Connecting) return 0;
        return qMax<qint64>(1, MonotonicMs() - m_transitionStartMs);
    }

    qint64 RuntimeStatus::stoppingMs() const {
        if (m_transitionStartMs == 0 || m_transitionState != ConnectionState::Stopping) return 0;
        return qMax<qint64>(1, MonotonicMs() - m_transitionStartMs);
    }

    qint64 RuntimeStatus::coreUnusableMs() {
        const CoreState state = m_host->supervisor()->state();
        const bool settled = state == CoreState::GaveUp || state == CoreState::ListenFailed || state == CoreState::ShuttingDown;
        if (settled || coreUsable()) {
            m_coreUnusableSinceMs = 0;
            return 0;
        }
        const qint64 now = MonotonicMs();
        if (m_coreUnusableSinceMs == 0) m_coreUnusableSinceMs = now;
        return now - m_coreUnusableSinceMs;
    }

    bool RuntimeStatus::healthy() const {
        const auto *killSwitch = Sys::KillSwitch::instance();
        // A restart cannot fix a persistent kill switch failure, so it stays in degraded only; a probe must not loop on it.
        const bool killSwitchOk = killSwitch->state() != Sys::KillSwitch::State::Failed || killSwitch->failedPersistently();
        // A teardown is never healthy, whatever the core still reports.
        return !m_shuttingDown && ready() && coreUsable() && stuckSeconds() == 0 && killSwitchOk &&
               m_host->supervisor()->state() != CoreState::GaveUp;
    }

    QString RuntimeStatus::summary() const {
        if (m_shuttingDown) return QStringLiteral("Stopping");
        if (!ready()) return QStringLiteral("Starting");
        const auto *supervisor = m_host->supervisor();
        if (supervisor->state() == CoreState::GaveUp) return QStringLiteral("Degraded: core_gave_up");
        if (!coreUsable()) {
            const int failures = supervisor->consecutiveFailures();
            if (failures > 0) return QStringLiteral("Core restarting %1/%2").arg(failures).arg(CoreSupervisor::kGiveUpAfter);
            return QStringLiteral("Starting core");
        }
        const auto running = m_host->runningSnapshot();
        const int targetId = m_host->connection()->lastProfileId();
        switch (m_host->connectionState()) {
            case ConnectionState::Connecting:
                return runtimeStatusOneLine(QStringLiteral("Connecting ") + runtimeStatusProfileName(nullptr, targetId));
            case ConnectionState::Stopping:
                return runtimeStatusOneLine(QStringLiteral("Stopping ") + runtimeStatusProfileName(running, targetId));
            case ConnectionState::Running: {
                QString line = QStringLiteral("Connected ") + runtimeStatusProfileName(running, targetId);
                if (Configs::dataManager->settingsRepo->spmode_vpn) line += QStringLiteral(" (Tun)");
                return runtimeStatusOneLine(line);
            }
            case ConnectionState::Idle:
                break;
        }
        const QString reason = m_host->degradedReason();
        return reason.isEmpty() ? QStringLiteral("Idle") : QStringLiteral("Degraded: ") + reason;
    }

    QJsonObject RuntimeStatus::healthFields() const {
        return {
            {QStringLiteral("ok"), healthy()},
            {QStringLiteral("mode"), QStringLiteral("headless")},
            {QStringLiteral("ready"), ready()},
            {QStringLiteral("pid"), QCoreApplication::applicationPid()},
            {QStringLiteral("uptime_s"), (MonotonicMs() - m_bootMonoMs) / 1000},
            {QStringLiteral("stuck_s"), stuckSeconds()},
            {QStringLiteral("core_restarts"), m_host->supervisor()->restartCount()},
            {QStringLiteral("previous_crash"), Logging::PreviousSessionCrashed()},
            {QStringLiteral("headless_dialogs"), HeadlessDialogCount()},
        };
    }

    void RuntimeStatus::refreshSummary() {
        auto current = summary();
        if (current == m_summary) return;
        m_summary = std::move(current);
        emit summaryChanged(m_summary);
    }
} // namespace Engine
