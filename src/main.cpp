#include <csignal>
#include <cstdio>
#include <memory>

#include <QApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QTranslator>
#include <QMessageBox>
#include <QLocalSocket>
#include <QLocalServer>
#include <QThread>
#include <QDateTime>
#include <3rdparty/WinCommander.hpp>


#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/global/Notice.hpp"

#include "include/engine/EngineHost.h"
#include "include/engine/app/Bootstrap.h"
#include "include/engine/app/ConfigDump.h"
#include "include/engine/app/DataDirLock.h"
#include "include/engine/app/ExitCodes.h"
#include "include/engine/app/InstanceServer.h"
#include "include/engine/headless/HeadlessMain.h"
#include "include/sys/Process.hpp"
#include "include/sys/ServiceManager.hpp"
#include "include/ui/app/GuiStartup.h"
#include "include/ui/mainwindow.h"
#include "include/stats/traffic/TrafficStatsManager.hpp"
#include "include/api/RPC.h"

#ifdef Q_OS_WIN
#include "include/sys/windows/MiniDump.h"
#include "include/sys/windows/eventHandler.h"
#include "include/sys/windows/WinVersion.h"
#include <qfontdatabase.h>
#include <windows.h>
#endif
#ifdef Q_OS_LINUX
#include <include/sys/linux/coreDump.h>
#include <qfontdatabase.h>
#endif
#ifdef Q_OS_UNIX
#include <QSocketNotifier>
#include <cerrno>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#ifdef Q_OS_MACOS
#include <QFileOpenEvent>
#endif

void signal_handler(int signum) {
    Q_UNUSED(signum)
    // The running teardown quits on its own; re-entering it from one of its nested loops would deadlock on mu_exit.
    if (Engine::Bootstrap::ExitInProgress()) return;
    if (auto *h = Engine::host()) h->shutdown();
    QCoreApplication::quit();
}

int main(int argc, char* argv[]) {
    Logging::InstallQtMessageHandler();

    // Ahead of the crash handler: its MessageBoxW would hang a displayless run.
    auto launch = Engine::Bootstrap::PreScan(argc, argv);
#ifdef Q_OS_WIN
    if (launch.service) {
        if (!Sys::ServiceManager::StartServiceDispatcher()) {
            Engine::Bootstrap::AttachParentConsole();
            if (_fileno(stderr) >= 0) {
                std::fputs("--service: Throne was not started by the Windows Service Control Manager; install the service with "
                           "install-service.bat, or run without --service\n", stderr);
                std::fflush(stderr);
            }
            return Engine::ExitCode::Usage;
        }
        // Session 0 shows no window: --service alone runs headless too.
        if (launch.mode == Engine::Bootstrap::Mode::Gui) launch.mode = Engine::Bootstrap::Mode::Headless;
    }
#endif
    if (launch.mode == Engine::Bootstrap::Mode::DumpConfigs) return Engine::RunConfigDump(argc, argv);
    if (launch.mode == Engine::Bootstrap::Mode::Headless || launch.mode == Engine::Bootstrap::Mode::HeadlessClient) {
        const int code = Engine::RunHeadless(argc, argv, launch);
        // Last: once the SCM sees SERVICE_STOPPED it may kill this process and start the recovery instance.
        Sys::ServiceManager::NotifyStopped(code);
        return code;
    }

#ifdef Q_OS_WIN
    Windows_SetCrashHandler(Windows_WindowStationVisible());
#endif
#ifdef Q_OS_LINUX
    enable_core_dumps();
#endif

    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication a(argc, argv);

    SetNoticeSink([](const Notice &n) { ShowPassiveWarning(n.title, n.text); });
    Engine::Bootstrap::SetSoftwareNames();

    GuiStartup::InstallMacOpenEventFilter(a);
    GuiStartup::LoadEmojiFont();

    QStringList arguments = QApplication::arguments();
    const Engine::Bootstrap::RelaunchParent relaunchParent(arguments);
    // Must run before the working directory moves below: argument paths may be relative to it.
    const QString launchDeeplink = Deeplink_ExtractFromArgs(arguments);
    const QStringList launchFiles = LaunchFiles_ExtractFromArgs(arguments, QDir::current());

    QDir::setCurrent(QApplication::applicationDirPath());
    if (QFile::exists("updater.old")) {
        QFile::remove("updater.old");
    }

    const auto dataDir = Engine::Bootstrap::ResolveDataDir(arguments, QDir(QCoreApplication::applicationDirPath()));
    const QDir &wd = dataDir.wd;
    Configs::SetBasePath(wd.absolutePath());

    const auto serverName = Engine::Bootstrap::InstanceServerName(wd);
    qDebug() << "server name: " << serverName;
    QStringList launchPayload;
    if (!launchDeeplink.isEmpty()) launchPayload << launchDeeplink;
    for (const auto &file : launchFiles) launchPayload << QUrl::fromLocalFile(file).toString();

    // Before the temp wipe and initDB: the owner of the lock owns the data dir.
    Engine::DataDirLock dataDirLock;
    const auto claim = Engine::Bootstrap::ClaimDataDir(dataDirLock, dataDir, serverName, "gui", launchPayload, relaunchParent);
    if (claim.outcome == Engine::Bootstrap::ClaimOutcome::HandedOff) return 0;
    if (claim.outcome == Engine::Bootstrap::ClaimOutcome::HeldByHeadless) {
        const auto pid = claim.ownerPid;
        const QString text = pid > 0 ? QObject::tr("Throne is running in the background (PID %1). Stop the service or use thronectl.").arg(pid)
                                     : QObject::tr("Throne is running in the background. Stop the service or use thronectl.");
        std::fputs(qPrintable(QString("%1 Data directory: %2\n").arg(text, QDir::toNativeSeparators(wd.absolutePath()))), stderr);
        if (!arguments.contains("-tray")) QMessageBox::information(nullptr, "Throne", text);
        return 0;
    }
    if (claim.outcome == Engine::Bootstrap::ClaimOutcome::HeldByOther) {
        const auto pid = claim.ownerPid;
        // An autostart must not leave a box behind.
        if (!arguments.contains("-tray")) {
            const QString who = pid > 0 ? QObject::tr("Another Throne instance (PID %1)").arg(pid) : QObject::tr("Another Throne instance");
            QMessageBox::warning(nullptr, "Throne", QObject::tr("%1 is using this data directory:\n%2").arg(who, QDir::toNativeSeparators(wd.absolutePath())));
        }
        return 0;
    }

    // Listening before initDB lets a second launch hand off while migrations run; its payload waits in the pending buffers.
    Engine::InstanceServer server(serverName, QLocalServer::WorldAccessOption);
    if (!server.listen()) {
        // Logging::Init has not run yet, so the file log never sees this.
        const QString error = QString("Failed to start the instance server %1: %2").arg(serverName, server.errorString());
        std::fputs(qPrintable(error + '\n'), stderr);
        Engine::Bootstrap::WarnDroppedPayload(launchPayload);
        QMessageBox::critical(nullptr, "Throne", error);
        return 1;
    }
    // Not before this point: until the data dir is ours there is nothing to tear down, and the default action ends the waits above.
#ifdef Q_OS_UNIX
    Engine::Bootstrap::InstallTerminationHandlers(signal_handler);
#endif
    server.setHandlers(GuiStartup::HandleLaunchLine, [](QLocalSocket *) { MW_dialog_message(MwMessage::Raise, {}); });
    server.installQuitCleanup();

    Engine::Bootstrap::OpenDataDir(dataDir, arguments);
    Engine::Bootstrap::PrepareRuntime();

    LogThread = new QThread;
    LogThread->start();

    GuiStartup::SetupIconTheme();

#ifdef Q_OS_WIN
    if (GuiStartup::RelaunchElevatedIfNeeded(server, arguments, launchDeeplink, launchFiles)) return 0;
#endif

    if (Configs::dataManager->settingsRepo->start_minimal) Configs::dataManager->settingsRepo->flag_tray = true;

    GuiStartup::LoadTranslator();

    // Must follow the single-instance check: opening the log earlier truncates the running instance's file and fakes a crash marker.
    Engine::Bootstrap::StartLogging(dataDir);
    GuiStartup::InstallSessionEndHooks(a, signal_handler);

    API::defaultClient = new API::Client();

    Configs_sys::SetIsolatedChildren(true);
    Engine::EngineHost::create();
    auto *window = UI_InitMainWindow();
    Engine::host()->init(window, window);
    GuiStartup::WarnAboutHeadless();

    Configs::dataManager->RunDeferredMaintenance();

    if (Logging::PreviousSessionCrashed()) {
        MW_show_log(QObject::tr("[Warn] Throne did not shut down cleanly last time. "
                                "Diagnostics were saved to: %1").arg(Logging::LogDir()));
    }

    // The Flush calls replay whatever arrived before the window existed (e.g. a macOS FileOpen event).
    if (!launchDeeplink.isEmpty()) Deeplink_Submit(launchDeeplink);
    Deeplink_FlushPending();
    LaunchFiles_Submit(launchFiles);
    LaunchFiles_FlushPending();

#ifdef Q_OS_UNIX
    // Only right before exec(): a nested loop ahead of it would consume the signal while quit() is still a no-op.
    Engine::Bootstrap::ArmTerminationHandlers([] { signal_handler(0); });
#endif
    return QApplication::exec();
}
