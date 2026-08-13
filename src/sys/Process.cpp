#include "include/sys/Process.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

#include <QTimer>
#include <QDir>
#include <QApplication>



#include "include/ui/mainwindow.h"

namespace Configs_sys {
    CoreProcess::~CoreProcess() {
    }

    void CoreProcess::Kill() {
        kill();
        waitForFinished();
    }

    CoreProcess::CoreProcess(const QString &core_path, const QString &socketName, bool debugMode)
        : m_socketName(socketName), m_debugMode(debugMode) {
        program = core_path;

        connect(this, &QProcess::readyReadStandardOutput, this, [&]() {
            auto log = readAllStandardOutput();
            if (log.contains("Extra process exited unexpectedly"))
            {
                MW_show_log("Extra Core exited, stopping profile...");
                MW_dialog_message(MwMessage::CoreCrashed, {});
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
            }
            LOG_ERROR(QString("core process error %1: %2").arg(static_cast<int>(error)).arg(errorString()));
        });
        connect(this, &QProcess::finished, this, [&](int exitCode, ExitStatus exitStatus) {
            const bool crashed = exitStatus == CrashExit || exitCode != 0;
            Logging::Write(crashed ? Logging::Level::Error : Logging::Level::Info,
                           QString("core process exited: code=%1 status=%2%3")
                               .arg(exitCode)
                               .arg(exitStatus == CrashExit ? "crash" : "normal")
                               .arg(Configs::dataManager->settingsRepo->prepare_exit ? " (during shutdown)" : ""));
        });
        connect(this, &QProcess::stateChanged, this, [&](ProcessState state) {
            if (state == NotRunning) {
                Configs::dataManager->settingsRepo->core_running = false;
                qDebug() << "Core stated changed to not running";
                const bool reconnectPlanned =
                    !Configs::dataManager->settingsRepo->prepare_exit &&
                    !failed_to_start;
                if (GetMainWindow() != nullptr) {
                    GetMainWindow()->killSwitchCoreTerminated(reconnectPlanned);
                }
            }

            if (!Configs::dataManager->settingsRepo->prepare_exit && state == NotRunning) {
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

        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("THRONE_CORE_SOCKET", m_socketName);
        // Turns an unrecovered Go panic into a real abort, so it dumps all
        // goroutine stacks and WER captures a minidump of the core too.
        env.insert("GOTRACEBACK", "crash");
        if (m_debugMode) env.insert("THRONE_CORE_DEBUG", "1");
        // Point Xray-core's asset loader at our writable config dir so full Xray
        // configs whose routing uses geoip:/geosite: tags can find geoip.dat /
        // geosite.dat there. Setting it here (rather than in the Go core) means a
        // later on-demand download lands in this same dir and is picked up on the
        // next profile start with no core restart.
        env.insert("XRAY_LOCATION_ASSET", Configs::GetBasePath());
        setProcessEnvironment(env);
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

} // namespace Configs_sys
