#include "include/sys/Process.hpp"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

#include <QTimer>
#include <QDir>
#include <QApplication>



#include "include/ui/mainwindow.h"

namespace {
    // Must match egressForwardingMarker in core/internal/rpc/forwarding_windows.go.
    constexpr char kEgressForwardingMarker[] = "IPv4 forwarding is enabled on the Tun egress adapter: ";

    void warnEgressForwarding(const QByteArray &log) {
        const auto at = log.indexOf(kEgressForwardingMarker);
        if (at < 0) return;
        const auto from = at + qsizetype(sizeof(kEgressForwardingMarker) - 1);
        const auto end = log.indexOf('\n', from);
        const auto adapter = QString::fromUtf8(log.mid(from, end < 0 ? -1 : end - from)).trimmed();
        PostPassiveWarning(QObject::tr("IPv4 forwarding breaks Tun mode"),
                           QObject::tr("IPv4 forwarding is on for the network adapter \"%1\", usually because Mobile Hotspot or Internet Connection Sharing is sharing it.\n\n"
                                       "Windows then ignores the adapter binding that keeps Throne's own connections out of the Tun, so they loop back into it and fail.\n\n"
                                       "To fix this, share the hotspot from the throne-tun adapter instead of \"%1\" (Settings > Mobile hotspot > Share my internet connection from), or turn the hotspot off while using Tun mode.")
                               .arg(adapter));
    }
}

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
            warnEgressForwarding(log);
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

        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("THRONE_CORE_SOCKET", m_socketName);
        // Turns an unrecovered Go panic into a real abort, so all goroutine stacks are dumped and WER captures the core too.
        env.insert("GOTRACEBACK", "crash");
        if (m_debugMode) env.insert("THRONE_CORE_DEBUG", "1");
        // Points Xray's asset loader at our writable config dir, so a geoip.dat/geosite.dat downloaded later is found with no core restart.
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
