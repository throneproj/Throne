#include "include/sys/Process.hpp"
#include "include/global/Configs.hpp"

#include <QTimer>
#include <QDir>
#include <QApplication>
#include <QRegularExpression>

#include "include/ui/mainwindow.h"

namespace Configs_sys {

    static inline QString sanitizeLog(const QByteArray &raw) {
        QString s = QString::fromUtf8(raw);
        static const QRegularExpression ansiRe(QStringLiteral("\x1B\\[[0-9;]*[A-Za-z]"));
        s.remove(ansiRe);
        static const QRegularExpression escRe(QStringLiteral("\x1B[^\\[]?"));
        s.remove(escRe);
        static const QRegularExpression ctrlRe(
            QStringLiteral("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F]"));
        s.remove(ctrlRe);
        return s;
    }
    CoreProcess::~CoreProcess() {
    }

    void CoreProcess::Kill() {
        kill();
        waitForFinished();
    }

    CoreProcess::CoreProcess(const QString &core_path, const QStringList &args) {
        program = core_path;
        arguments = args;

        connect(this, &QProcess::readyReadStandardOutput, this, [&]() {
            auto log = readAllStandardOutput();
            if (!Configs::dataManager->settingsRepo->core_running) {
                if (log.contains("Core listening at")) {
                    Configs::dataManager->settingsRepo->core_running = true;
                    MW_dialog_message("ExternalProcess", "CoreStarted," + Int2String(start_profile_when_core_is_up));
                    start_profile_when_core_is_up = -1;
                } else if (log.contains("failed to serve")) {
                    kill();
                }
            }
            if (log.contains("Extra process exited unexpectedly"))
            {
                MW_show_log("Extra Core exited, stopping profile...");
                MW_dialog_message("ExternalProcess", "Crashed");
            }
            if (logCounter.fetchAndAddRelaxed(log.count("\n")) > Configs::dataManager->settingsRepo->max_log_line) return;
            MW_show_log(sanitizeLog(log));
        });
        connect(this, &QProcess::readyReadStandardError, this, [&]() {
            auto log = readAllStandardError().trimmed();
            MW_show_log(sanitizeLog(log));
        });
        connect(this, &QProcess::errorOccurred, this, [&](ProcessError error) {
            if (error == FailedToStart) {
                failed_to_start = true;
                MW_show_log("start core error occurred: " + errorString() + "\n");
            }
        });
        connect(this, &QProcess::stateChanged, this, [&](ProcessState state) {
            if (state == NotRunning) {
                Configs::dataManager->settingsRepo->core_running = false;
                qDebug() << "Core stated changed to not running";
            }

            if (!Configs::dataManager->settingsRepo->prepare_exit && state == NotRunning) {
                if (failed_to_start) return;
                if (restarting) return;

                MW_show_log("[Fatal] " + QObject::tr("Core exited, cleaning up..."));

                GetMainWindow()->profile_stop(true, true);

                if (coreRestartTimer.isValid()) {
                    if (coreRestartTimer.restart() < 10 * 1000) {
                        coreRestartTimer = QElapsedTimer();
                        MW_show_log("[ERROR] " + QObject::tr("Core exits too frequently, stop automatic restart this profile."));
                        return;
                    }
                } else {
                    coreRestartTimer.start();
                }

                start_profile_when_core_is_up = Configs::dataManager->settingsRepo->started_id;
                MW_show_log("[Warn] " + QObject::tr("Restarting the core ..."));
                setTimeout([=,this] { Restart(); }, this, 200);
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
