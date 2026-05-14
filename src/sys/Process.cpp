#include "include/sys/Process.hpp"
#include "include/global/Configs.hpp"

#include <QTimer>
#include <QDir>
#include <QApplication>



#include "include/ui/mainwindow.h"

namespace Configs_sys {
    CoreProcess::~CoreProcess() {
    }

    void CoreProcess::Kill(int timeoutMs) {
        if (state() == NotRunning) return;
        kill();
        if (!waitForFinished(timeoutMs)) {
            MW_show_log("[Warn] " + QObject::tr("Core process did not exit within %1 ms. Continuing shutdown.").arg(timeoutMs));
        }
    }

    CoreProcess::CoreProcess(const QString &core_path, const QString &socketName, bool debugMode)
        : m_socketName(socketName), m_debugMode(debugMode) {
        program = core_path;

        connect(this, &QProcess::readyReadStandardOutput, this, [&]() {
            auto log = readAllStandardOutput();
            if (log.contains("Extra process exited unexpectedly"))
            {
                MW_show_log("Extra Core exited, stopping profile...");
                MW_dialog_message("ExternalProcess", "Crashed");
            }
            if (logCounter.fetchAndAddRelaxed(log.count("\n")) > Configs::dataManager->settingsRepo->max_log_line) return;
            MW_show_log(log);
        });
        connect(this, &QProcess::readyReadStandardError, this, [&]() {
            auto log = readAllStandardError().trimmed();
            MW_show_log(log);
        });
        connect(this, &QProcess::errorOccurred, this, [&](ProcessError error) {
            if (error == FailedToStart) {
                failed_to_start = true;
                MW_show_log("start core error occurred: " + errorString() + "\n");
                if (last_start_used_pkexec && !root_start_failed_reported) {
                    root_start_failed_reported = true;
                    MW_dialog_message("ExternalProcess", "RootStartFailed");
                }
            }
        });
        connect(this, &QProcess::stateChanged, this, [&](ProcessState state) {
            if (state == NotRunning) {
                Configs::dataManager->settingsRepo->core_running = false;
                qDebug() << "Core stated changed to not running";
            }

            if (!Configs::dataManager->settingsRepo->prepare_exit && state == NotRunning) {
                if (last_start_used_pkexec && !core_reported_started) {
                    failed_to_start = true;
                    MW_show_log("[Error] " + QObject::tr("Root core start was cancelled or failed; TUN was not enabled."));
                    if (!root_start_failed_reported) {
                        root_start_failed_reported = true;
                        MW_dialog_message("ExternalProcess", "RootStartFailed");
                    }
                    return;
                }
                if (failed_to_start) return; // no retry
                if (restarting) return;

                MW_show_log("[Fatal] " + QObject::tr("Core exited, cleaning up..."));

                GetMainWindow()->profile_stop(true, true);

                // Retry rate limit
                if (coreRestartTimer.isValid()) {
                    if (coreRestartTimer.restart() < 10 * 1000) {
                        coreRestartTimer = QElapsedTimer();
                        MW_show_log("[ERROR] " + QObject::tr("Core exits too frequently, stop automatic restart this profile."));
                        return;
                    }
                } else {
                    coreRestartTimer.start();
                }

                // Restart
                start_profile_when_core_is_up = Configs::dataManager->settingsRepo->started_id;
                MW_show_log("[Warn] " + QObject::tr("Restarting the core ..."));
                setTimeout([=,this] { Restart(); }, this, 200);
            }
        });
    }

    void CoreProcess::Start() {
        if (started) return;
        started = true;
        failed_to_start = false;
        core_reported_started = false;
        root_start_failed_reported = false;
        last_start_used_pkexec = false;

        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("THRONE_CORE_SOCKET", m_socketName);
        if (m_debugMode) env.insert("THRONE_CORE_DEBUG", "1");
        setProcessEnvironment(env);
#ifdef Q_OS_LINUX
        if (use_pkexec) {
            QStringList pkexecArgs;
            pkexecArgs << "env";
            pkexecArgs << "THRONE_CORE_SOCKET=" + m_socketName;
            pkexecArgs << "THRONE_CORE_PKEXEC=1";
            if (m_debugMode) pkexecArgs << "THRONE_CORE_DEBUG=1";
            pkexecArgs << program;
            last_start_used_pkexec = true;
            start("pkexec", pkexecArgs);
            return;
        }
#endif
        start(program, {});
    }

    void CoreProcess::Restart() {
        restarting = true;
        kill();
        waitForFinished(500);
        started = false;
        Start();
        restarting = false;
    }

    void CoreProcess::SetUsePkexec(bool enable) {
        use_pkexec = enable;
    }

    bool CoreProcess::IsUsingPkexec() const {
        return use_pkexec;
    }

    void CoreProcess::MarkCoreReportedStarted() {
        core_reported_started = true;
    }

} // namespace Configs_sys
