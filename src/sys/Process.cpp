#include "include/sys/Process.hpp"
#include "include/global/Configs.hpp"

#include <QTimer>
#include <QDir>
#include <QApplication>

namespace Configs_sys {
    CoreProcess::~CoreProcess() {
    }

    void CoreProcess::Kill() {
        if (killed) return;
        killed = true;

        if (!crashed) {
            kill();
            waitForFinished(500);
        }
    }

    CoreProcess::CoreProcess(const QString &core_path, const QStringList &args) {
        program = core_path;
        arguments = args;

        connect(this, &QProcess::readyReadStandardOutput, this, [&]() {
            auto log = readAllStandardOutput();
            if (!Configs::dataStore->core_running) {
                if (log.contains("Core listening at")) {
                    // The core really started
                    Configs::dataStore->core_running = true;
                    MW_dialog_message("ExternalProcess", "CoreStarted," + Int2String(start_profile_when_core_is_up));
                    start_profile_when_core_is_up = -1;
                } else if (log.contains("failed to serve")) {
                    // The core failed to start
                    kill();
                }
            }
            if (log.contains("Extra process exited unexpectedly"))
            {
                MW_show_log("Extra Core exited, stopping profile...");
                MW_dialog_message("ExternalProcess", "Crashed");
            }
            if (logCounter.fetchAndAddRelaxed(log.count("\n")) > Configs::dataStore->max_log_line) return;
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
        });
        connect(this, &QProcess::stateChanged, this, [&](ProcessState state) {
            if (state == NotRunning) {
                Configs::dataStore->core_running = false;
                qDebug() << "Core stated changed to not running";
            }

            if (!Configs::dataStore->prepare_exit && state == NotRunning) {
                if (failed_to_start) return; // no retry
                if (restarting) return;

                MW_dialog_message("ExternalProcess", "CoreCrashed");

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
                start_profile_when_core_is_up = Configs::dataStore->started_id;
                MW_show_log("[Fatal] " + QObject::tr("Core exited, restarting."));
                setTimeout([=] { Restart(); }, this, 200);
            }
        });
    }

    void CoreProcess::Start() {
        if (started) return;
        started = true;

        setEnvironment(QProcessEnvironment::systemEnvironment().toStringList());
        start(program, arguments);
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
