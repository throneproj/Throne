#include "include/sys/Process.hpp"
#include "include/global/NekoGui.hpp"

#include <QTimer>
#include <QDir>
#include <QApplication>
#include <QElapsedTimer>

namespace NekoGui_sys {

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

// 
QElapsedTimer coreRestartTimer;

CoreProcess::CoreProcess(const QString &core_path, const QStringList &args) : QProcess() {
    this->env = QProcessEnvironment::systemEnvironment().toStringList();
    program = core_path;
    arguments = args;

    connect(this, &QProcess::readyReadStandardOutput, this, &CoreProcess::handleOutput);
    connect(this, &QProcess::readyReadStandardError, this, &CoreProcess::handleError);
    connect(this, &QProcess::errorOccurred, this, &CoreProcess::handleErrorOccurred);
    connect(this, &QProcess::stateChanged, this, &CoreProcess::handleStateChanged);
}

void CoreProcess::Start() {
    show_stderr = false;
    if (started) return;
    started = true;

    setEnvironment(env);
    if (!start(program, arguments)) {
        MW_show_log("Failed to start process: " + errorString());
        return;
    }

    write((NekoGui::dataStore->core_token + "\n").toUtf8());
}

void CoreProcess::Restart() {
    restarting = true;
    kill();
    if (!waitForFinished(500)) {
        MW_show_log("Failed to wait for process to finish.");
    }
    started = false;
    Start();
    restarting = false;
}

void CoreProcess::handleOutput() {
    auto log = readAllStandardOutput();
    if (!NekoGui::dataStore->core_running) {
        if (log.contains("Core listening at")) {
            NekoGui::dataStore->core_running = true;
            if (start_profile_when_core_is_up >= 0) {
                MW_dialog_message("ExternalProcess", "CoreStarted," + Int2String(start_profile_when_core_is_up));
                start_profile_when_core_is_up = -1;
            }
        } else if (log.contains("failed to serve")) {
            kill();
        }
    }
    if (logCounter.fetchAndAddRelaxed(log.count("\n")) > NekoGui::dataStore->max_log_line) return;
    MW_show_log(log);
}

void CoreProcess::handleError() {
    auto log = readAllStandardError().trimmed();
    MW_show_log(log);
}

void CoreProcess::handleErrorOccurred(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        failed_to_start = true;
        MW_show_log("start core error occurred: " + errorString() + "\n");
    }
}

void CoreProcess::handleStateChanged(QProcess::ProcessState state) {
    if (state == QProcess::NotRunning) {
        NekoGui::dataStore->core_running = false;
    }

    if (!NekoGui::dataStore->prepare_exit && state == QProcess::NotRunning) {
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
        start_profile_when_core_is_up = NekoGui::dataStore->started_id;
        MW_show_log("[ERROR] " + QObject::tr("Core exited, restarting."));
        setTimeout([=] { Restart(); }, this, 1000);
    }
}

} // namespace NekoGui_sys
