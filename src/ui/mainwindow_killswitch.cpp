#include "include/ui/mainwindow.h"

#include "include/global/Configs.hpp"
#include "include/sys/KillSwitchController.hpp"
#include "include/sys/Process.hpp"

#ifdef Q_OS_WIN
#include "3rdparty/WinCommander.hpp"
#include "include/sys/windows/WindowsWfpKillSwitchBackend.h"
#endif

#include <QApplication>
#include <QMessageBox>
#include <QThread>
#include <QTimer>

#include <optional>
#include <vector>

namespace {

class ActivityLockGuard final
{
public:
    ~ActivityLockGuard()
    {
        for (auto iterator = locked_.rbegin(); iterator != locked_.rend(); ++iterator) {
            (*iterator)->release();
        }
    }

    bool tryLock(QSemaphore &semaphore)
    {
        if (!semaphore.tryAcquire()) {
            return false;
        }
        locked_.push_back(&semaphore);
        return true;
    }

private:
    std::vector<QSemaphore *> locked_;
};

void assignError(QString *target, const QString &error)
{
    if (target != nullptr) {
        *target = error;
    }
}

#ifdef Q_OS_WIN
std::optional<Configs_sys::KillSwitchTunInterface> currentTunInterface()
{
    if (!Configs::dataManager->settingsRepo->spmode_vpn) {
        return std::nullopt;
    }
    return Configs_sys::KillSwitchTunInterface{
        WindowsWfpKillSwitchBackend::tunInterfaceAlias(),
        0,
        true,
        Configs::dataManager->settingsRepo->vpn_ipv6,
    };
}
#endif

} // namespace

bool MainWindow::initializeKillSwitch()
{
#ifdef Q_OS_WIN
    killSwitchBackend = std::make_unique<WindowsWfpKillSwitchBackend>();
    killSwitchController =
        std::make_unique<Configs_sys::KillSwitchController>(*killSwitchBackend);

    Configs_sys::KillSwitchTrustedCorePlan corePlan;
    corePlan.executablePaths << Configs::FindCoreRealPath();
    const auto initialized = killSwitchController->initialize(
        Configs::dataManager->settingsRepo->kill_switch_enabled,
        std::move(corePlan));

    if (initialized && initialized.enabled &&
        !Configs::dataManager->settingsRepo->kill_switch_enabled) {
        Configs::dataManager->settingsRepo->kill_switch_enabled = true;
        Configs::dataManager->settingsRepo->Save();
    }
    if (initialized.recoveredStaleProtection) {
        MW_show_log(tr("Recovered persistent Throne kill-switch state."));
    }
    if (!initialized) {
        MW_show_log(tr("Failed to initialize kill switch: %1")
                        .arg(initialized.result.error));
        QMessageBox::critical(
            this,
            tr("Kill switch unavailable"),
            tr("Throne could not establish or recover fail-closed protection. "
               "Throne will exit before starting its core.\n\n%1\n\n"
               "Run Throne as Administrator and try again, or use "
               "--disable-kill-switch to recover connectivity.")
                .arg(initialized.result.error));
        return false;
    }
    if (initialized.enabled) {
        MW_show_log(tr("Kill switch enabled (persistent IPv4 and IPv6 blocking active)."));
    }
#endif
    return true;
}

bool MainWindow::killSwitchActive() const
{
#ifdef Q_OS_WIN
    return killSwitchController && killSwitchController->snapshot().enabled;
#else
    return false;
#endif
}

bool MainWindow::setKillSwitchEnabled(const bool enable, QString *error)
{
#ifdef Q_OS_WIN
    if (!killSwitchController) {
        assignError(error, tr("The kill-switch manager is not initialized."));
        return false;
    }
    if (enable && Configs::dataManager->settingsRepo->flag_many) {
        assignError(error,
                    tr("The kill switch cannot be enabled in multiple-instance mode. "
                       "Restart Throne without -many first."));
        return false;
    }

    // A profile/test config built before this preference changes may contain
    // direct routes or local DNS that are forbidden under fail-closed policy.
    // Hold every activity gate across the OS transaction and settings update,
    // so such a config cannot become active after the policy changes (and a
    // disable cannot remove protection midway through a transition).
    ActivityLockGuard activityLocks;
    if (!activityLocks.tryLock(mu_starting) ||
        !activityLocks.tryLock(mu_stopping) ||
        !activityLocks.tryLock(testActivityGate)) {
        assignError(
            error,
            tr("Wait for the current profile transition or connectivity test to "
               "finish before changing the kill switch."));
        return false;
    }
    if (enable && running != nullptr) {
        assignError(
            error,
            tr("Disconnect the current profile before enabling the kill switch. "
               "The profile must be rebuilt under fail-closed DNS and routing rules."));
        return false;
    }

    const auto runMaintenanceHelper = [](const QString &operation) {
        auto helperArguments = Configs::dataManager->settingsRepo->argv;
        if (!helperArguments.isEmpty()) {
            helperArguments.removeFirst();
        }
        helperArguments.removeAll("--disable-kill-switch");
        helperArguments.removeAll("--prepare-kill-switch");
        helperArguments.removeAll("--quiet");
        helperArguments << operation << "--quiet";
        return WinCommander::runProcessElevated(
            QApplication::applicationFilePath(), helperArguments,
            QApplication::applicationDirPath(), WinCommander::WindowHidden, true);
    };

    if (enable && !Configs::IsAdmin()) {
        const uint helperResult = runMaintenanceHelper("--prepare-kill-switch");
        if (helperResult != 0) {
            assignError(error,
                        tr("Administrator permission is required to install the kill switch."));
            return false;
        }

        // The helper installed the baseline before persisting true. From this
        // point until the elevated replacement starts, losing connectivity is
        // intentional fail-closed behavior.
        Configs::dataManager->settingsRepo->kill_switch_enabled = true;
        Configs::dataManager->settingsRepo->Save();

        auto restartArguments = Configs::dataManager->settingsRepo->argv;
        if (!restartArguments.isEmpty()) {
            restartArguments.removeFirst();
        }
        restartArguments.removeAll("--disable-kill-switch");
        restartArguments.removeAll("--prepare-kill-switch");
        restartArguments.removeAll("--quiet");
        restartArguments << "--wait-for-process"
                         << QString::number(QCoreApplication::applicationPid());
        const uint restartResult = WinCommander::runProcessElevated(
            QApplication::applicationFilePath(), restartArguments,
            QApplication::applicationDirPath(), WinCommander::WindowNormal, false);
        if (restartResult == static_cast<uint>(-1)) {
            const uint rollbackResult =
                runMaintenanceHelper("--disable-kill-switch");
            if (rollbackResult == 0) {
                Configs::dataManager->settingsRepo->kill_switch_enabled = false;
                Configs::dataManager->settingsRepo->Save();
            }
            assignError(
                error,
                rollbackResult == 0
                    ? tr("The elevated Throne restart failed; the newly installed kill "
                         "switch was rolled back safely.")
                    : tr("Fail-closed rules are active, but the elevated Throne restart "
                         "and automatic rollback failed. Start Throne as Administrator "
                         "or run --disable-kill-switch."));
            return false;
        }

        MW_show_log(tr("Kill switch enabled; restarting Throne with Administrator privileges."));
        // Let the settings dialog finish applying and saving all fields first.
        // The elevated replacement waits for this process before reading them.
        QTimer::singleShot(0, this, [this] {
            if (prepare_exit()) {
                QCoreApplication::quit();
            }
        });
        return true;
    }

    if (!enable && !Configs::IsAdmin()) {
        const uint helperResult = runMaintenanceHelper("--disable-kill-switch");
        if (helperResult != 0) {
            assignError(error,
                        tr("Administrator permission is required to remove the kill switch."));
            return false;
        }
        Configs::dataManager->settingsRepo->kill_switch_enabled = false;
        Configs::dataManager->settingsRepo->Save();
        MW_show_log(tr("Kill switch disabled."));
        assignError(error, {});
        return true;
    }

    if (enable) {
        const auto enabled = killSwitchController->enable();
        if (!enabled) {
            const auto snapshot = killSwitchController->snapshot();
            if (snapshot.backend.baselineActive) {
                // The OS is already blocking. Persist that security-relevant
                // reality so the next launch reconciles it instead of assuming
                // the user's network is unprotected.
                Configs::dataManager->settingsRepo->kill_switch_enabled = true;
                Configs::dataManager->settingsRepo->Save();
            } else {
                const auto rolledBack = killSwitchController->disable();
                if (!rolledBack) {
                    // A failed rollback means the backend can no longer prove
                    // that no Throne policy remains. Keep the preference in
                    // sync with that conservative controller state so the
                    // checkbox and next startup both expose a recovery path.
                    Configs::dataManager->settingsRepo->kill_switch_enabled = true;
                    Configs::dataManager->settingsRepo->Save();
                    assignError(error,
                                tr("%1; automatic rollback also failed: %2")
                                    .arg(enabled.error, rolledBack.error));
                    return false;
                }
            }
            assignError(error, enabled.error);
            return false;
        }

        Configs::dataManager->settingsRepo->kill_switch_enabled = true;
        Configs::dataManager->settingsRepo->Save();

        MW_show_log(tr("Kill switch enabled."));
        assignError(error, {});
        return true;
    }

    const auto disabled = killSwitchController->disable();
    if (!disabled) {
        assignError(error, disabled.error);
        MW_show_log(tr("Failed to disable kill switch: %1").arg(disabled.error));
        return false;
    }
    Configs::dataManager->settingsRepo->kill_switch_enabled = false;
    Configs::dataManager->settingsRepo->Save();
    MW_show_log(tr("Kill switch disabled."));
    assignError(error, {});
    return true;
#else
    Q_UNUSED(enable)
    assignError(error, tr("Kill switch is currently supported on Windows only."));
    return false;
#endif
}

bool MainWindow::prepareKillSwitchProfileStart(const bool switching,
                                               quint64 *operationId,
                                               QString *error)
{
    if (operationId != nullptr) {
        *operationId = 0;
    }
#ifdef Q_OS_WIN
    if (!killSwitchController || !killSwitchController->snapshot().enabled) {
        return true;
    }

    const auto snapshot = killSwitchController->snapshot();
    killSwitchPreviousProfileUsedTun = snapshot.backend.tunAllowanceActive;
    killSwitchPreviousTunIpv6 = snapshot.allowedTun.ipv6;
    auto intent = switching ? Configs_sys::KillSwitchController::StartIntent::Switch
                            : Configs_sys::KillSwitchController::StartIntent::Connect;
    // CoreProcess::Restart keeps the logical running profile until the fresh
    // core is ready. Treat that path as a reconnect even though `running` is
    // still non-null; the nested stop will simply clear the stale instance.
    if (snapshot.state ==
        Configs_sys::KillSwitchController::State::Reconnecting) {
        intent = Configs_sys::KillSwitchController::StartIntent::Reconnect;
    }
    const auto prepared = killSwitchController->prepareForProfileStart(intent);
    if (!prepared) {
        assignError(error, prepared.error);
        MW_show_log(tr("Kill switch refused an unsafe profile transition: %1")
                        .arg(prepared.error));
        return false;
    }
    if (operationId != nullptr) {
        *operationId = prepared.operationId;
    }
    MW_show_log(switching
                    ? tr("Switching profile while kill switch remains active.")
                    : tr("Connecting while kill switch blocks direct traffic."));
#else
    Q_UNUSED(switching)
#endif
    assignError(error, {});
    return true;
}

bool MainWindow::finishKillSwitchProfileStart(const quint64 operationId,
                                              QString *error)
{
#ifdef Q_OS_WIN
    if (!killSwitchController || operationId == 0) {
        return true;
    }

    Configs_sys::KillSwitchResult ready;
    // RPC Start returns after the Box and TUN startup, but interface discovery
    // can lag very briefly. Retry without ever removing the persistent block.
    for (int attempt = 0; attempt < 40; ++attempt) {
        ready = killSwitchController->profileBecameReady(
            operationId, currentTunInterface());
        if (ready) {
            killSwitchPreviousProfileUsedTun = false;
            MW_show_log(tr("Proxy became ready; kill switch remains active."));
            assignError(error, {});
            return true;
        }
        if (!Configs::dataManager->settingsRepo->spmode_vpn ||
            !ready.error.contains("interface", Qt::CaseInsensitive)) {
            break;
        }
        QThread::msleep(50);
    }
    assignError(error, ready.error);
    return false;
#else
    Q_UNUSED(operationId)
    assignError(error, {});
    return true;
#endif
}

void MainWindow::failKillSwitchProfileStart(const quint64 operationId,
                                            const QString &error,
                                            const bool coreInstanceMayBeRunning)
{
#ifdef Q_OS_WIN
    if (!killSwitchController || operationId == 0) {
        return;
    }
    if (killSwitchController->snapshot().activeOperationId != operationId) {
        // A failed Stop may already have restored the previous profile and
        // cancelled the enclosing switch. Do not turn that successful rollback
        // into a spurious stale-notification error.
        return;
    }
    const auto handled =
        killSwitchController->profileStartFailed(operationId, error);
    if (coreInstanceMayBeRunning) {
        MW_show_log(tr("Stopping an unusable core instance while kill-switch blocking remains active."));
        runOnThread(
            [this] {
                core_process->start_profile_when_core_is_up = -1;
                core_process->Restart();
            },
            DS_cores);
    }
    killSwitchPreviousProfileUsedTun = false;
    if (!handled) {
        MW_show_log(tr("Kill-switch error after profile start failure: %1")
                        .arg(handled.error));
    } else {
        MW_show_log(tr("Profile failed; direct Internet remains blocked."));
    }
#else
    Q_UNUSED(operationId)
    Q_UNUSED(error)
#endif
}

bool MainWindow::prepareKillSwitchProfileStop(QString *error)
{
#ifdef Q_OS_WIN
    if (!killSwitchController || !killSwitchController->snapshot().enabled) {
        return true;
    }
    const auto before = killSwitchController->snapshot();
    if (before.backend.tunAllowanceActive) {
        killSwitchPreviousProfileUsedTun = true;
        killSwitchPreviousTunIpv6 = before.allowedTun.ipv6;
    }
    const auto prepared = killSwitchController->prepareForProfileStop();
    if (!prepared) {
        assignError(error, prepared.error);
        MW_show_log(tr("Kill switch refused an unsafe profile stop: %1")
                        .arg(prepared.error));
        return false;
    }
#endif
    assignError(error, {});
    return true;
}

void MainWindow::finishKillSwitchProfileStop()
{
#ifdef Q_OS_WIN
    if (!killSwitchController || !killSwitchController->snapshot().enabled) {
        return;
    }
    const auto stopped = killSwitchController->profileStopped();
    killSwitchPreviousProfileUsedTun = false;
    if (!stopped) {
        MW_show_log(tr("Kill-switch stop-state error: %1").arg(stopped.error));
    } else {
        MW_show_log(tr("Profile stopped; direct Internet remains blocked."));
    }
#endif
}

void MainWindow::failKillSwitchProfileStop(const QString &error)
{
#ifdef Q_OS_WIN
    if (!killSwitchController || !killSwitchController->snapshot().enabled) {
        return;
    }
    std::optional<Configs_sys::KillSwitchTunInterface> previousTun;
    if (killSwitchPreviousProfileUsedTun) {
        previousTun = Configs_sys::KillSwitchTunInterface{
            WindowsWfpKillSwitchBackend::tunInterfaceAlias(), 0, true,
            killSwitchPreviousTunIpv6};
    }
    const auto restored =
        killSwitchController->profileStopFailed(previousTun, error);
    if (restored) {
        killSwitchPreviousProfileUsedTun = false;
    }
    if (!restored) {
        MW_show_log(tr("Profile stop failed; traffic remains fail-closed: %1")
                        .arg(restored.error));
    }
#else
    Q_UNUSED(error)
#endif
}

void MainWindow::killSwitchCoreTerminated(const bool reconnectPlanned)
{
#ifdef Q_OS_WIN
    if (!killSwitchController || !killSwitchController->snapshot().enabled) {
        return;
    }
    const auto handled =
        killSwitchController->coreTerminatedUnexpectedly(reconnectPlanned);
    killSwitchPreviousProfileUsedTun = false;
    if (!handled) {
        MW_show_log(tr("Kill switch failed to reconcile after core exit: %1")
                        .arg(handled.error));
    } else {
        MW_show_log(tr("Core exited; persistent kill switch is still blocking direct traffic."));
    }
#else
    Q_UNUSED(reconnectPlanned)
#endif
}

bool MainWindow::prepareKillSwitchExit(QString *error)
{
#ifdef Q_OS_WIN
    if (!killSwitchController) {
        return true;
    }
    const auto prepared = killSwitchController->prepareForExit();
    if (!prepared) {
        assignError(error, prepared.error);
        return false;
    }
#endif
    assignError(error, {});
    return true;
}
