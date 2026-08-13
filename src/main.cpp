#include <csignal>
#include <memory>

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QTranslator>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLocalSocket>
#include <QLocalServer>
#include <QThread>
#include <QDateTime>
#include <3rdparty/WinCommander.hpp>


#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

#include "include/ui/mainwindow_interface.h"
#include "include/stats/traffic/TrafficStatsManager.hpp"
#include "include/api/RPC.h"

#ifdef Q_OS_WIN
#include "include/sys/windows/MiniDump.h"
#include "include/sys/windows/eventHandler.h"
#include "include/sys/windows/WinVersion.h"
#include "include/sys/windows/WindowsWfpKillSwitchBackend.h"
#include "include/sys/windows/guihelper.h"
#include <qfontdatabase.h>
#include <windows.h>
#endif
#ifdef Q_OS_LINUX
#include <include/sys/linux/coreDump.h>
#include <qfontdatabase.h>
#endif
#ifdef Q_OS_MACOS
#include <QFileOpenEvent>

// On macOS the OS reuses the running app and delivers throne:// URLs, as well as
// files opened with the app, as a QFileOpenEvent to the application object (never
// via argv). This filter feeds both into the common pipelines.
class MacOpenEventFilter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::FileOpen) {
            const auto openEvent = static_cast<QFileOpenEvent *>(event);
            const QString url = openEvent->url().toString();
            if (url.startsWith("throne://")) {
                Deeplink_Submit(url);
                return true;
            }
            const QString file = openEvent->file().isEmpty() ? openEvent->url().toLocalFile() : openEvent->file();
            if (!file.isEmpty()) {
                LaunchFiles_Submit({file});
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }
};
#endif

void signal_handler(int signum) {
    Q_UNUSED(signum)
    auto *window = GetMainWindow();
    if (window == nullptr || window->prepare_exit()) {
        qApp->quit();
    }
}

QTranslator* trans = nullptr;
QTranslator* trans_qt = nullptr;

void loadTranslate(const QString& locale) {
    QT_TRANSLATE_NOOP("QPlatformTheme", "Cancel");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Apply");
    QT_TRANSLATE_NOOP("QPlatformTheme", "Yes");
    QT_TRANSLATE_NOOP("QPlatformTheme", "No");
    QT_TRANSLATE_NOOP("QPlatformTheme", "OK");
    if (trans != nullptr) {
        trans->deleteLater();
    }
    if (trans_qt != nullptr) {
        trans_qt->deleteLater();
    }
    trans = new QTranslator;
    trans_qt = new QTranslator;
    QLocale::setDefault(QLocale(locale));
    //
    const QString diskPath = QCoreApplication::applicationDirPath()+"/translations/" + locale + ".qm";
    const QString qrcPath = ":/translations/" + locale + ".qm";
    bool loadOK=false;
    if (QFileInfo::exists(diskPath)) {
        loadOK = trans->load(diskPath);
    }
    if (!loadOK) {
        loadOK = trans->load(qrcPath);
    }
    if (loadOK) {
        QCoreApplication::installTranslator(trans);
    }
}

namespace {
    constexpr auto FALLBACK_MARKER = "config/.install-dir-unwritable";

    // QFileInfo::isWritable reports the read-only attribute, not what a UAC-filtered
    // token may actually do under Program Files.
    bool DirIsWritable(const QDir &dir) {
        if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) return false;
        QFile probe(dir.absoluteFilePath(".throne-write-test"));
        if (!probe.open(QIODevice::WriteOnly)) return false;
        probe.close();
        probe.remove();
        return true;
    }

    bool ConfigDirIsUsable(const QDir &configDir) {
        if (!DirIsWritable(configDir)) return false;
        const QString db = configDir.absoluteFilePath("throne.db");
        if (!QFile::exists(db)) return true;
        QFile file(db);
        return file.open(QIODevice::ReadWrite);
    }

    void CopyDirContents(const QString &from, const QString &to) {
        QDir().mkpath(to);
        QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            it.next();
            const QString target = QDir(to).absoluteFilePath(it.fileName());
            if (it.fileInfo().isDir()) CopyDirContents(it.filePath(), target);
            else if (!QFile::exists(target)) QFile::copy(it.filePath(), target);
        }
    }

    // An elevated relaunch finds the install dir writable again, so the fallback is
    // pinned by a marker or the two runs land on different databases.
    bool AdoptUserConfigDir(const QDir &installWd, const QDir &userWd) {
        QFile marker(userWd.absoluteFilePath(FALLBACK_MARKER));
        if (marker.open(QIODevice::ReadOnly)) {
            const bool pinnedHere = QString::fromUtf8(marker.readAll()).trimmed() == installWd.absolutePath();
            marker.close();
            if (pinnedHere) return true;
        }

        const QString installConfig = installWd.absoluteFilePath("config");
        if (ConfigDirIsUsable(QDir(installConfig))) return false;

        const QString userConfig = userWd.absoluteFilePath("config");
        QDir().mkpath(userConfig);
        if (!QFile::exists(userConfig + "/throne.db") && QFile::exists(installConfig + "/throne.db")) {
            CopyDirContents(installConfig, userConfig);
            LOG_WARN(QString("copied existing config from %1").arg(installConfig));
        }
        if (marker.open(QIODevice::WriteOnly)) {
            marker.write(installWd.absolutePath().toUtf8());
            marker.close();
        }
        LOG_WARN(QString("%1 is not writable, using %2").arg(installConfig, userConfig));
        return true;
    }
} // namespace

#define LOCAL_SERVER_PREFIX "throne-"

int main(int argc, char* argv[]) {
    Logging::InstallQtMessageHandler();

    // Core dump
#ifdef Q_OS_WIN
    Windows_SetCrashHandler();
#endif
#ifdef Q_OS_LINUX
    enable_core_dumps();
#endif

    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication a(argc, argv);

#ifdef Q_OS_MACOS
    // Install before the event loop so launch-by-deeplink FileOpen events are caught.
    a.installEventFilter(new MacOpenEventFilter(&a));
#endif

#if !defined(Q_OS_MACOS) && (QT_VERSION >= QT_VERSION_CHECK(6,9,0))
    // Load the emoji fonts
#ifdef Q_OS_WIN
    int fontId = QFontDatabase::addApplicationFont(WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_11_22H2) ? ":/font/notoEmoji" : ":/font/Twemoji");
#else
    int fontId = QFontDatabase::addApplicationFont(":/font/notoEmoji");
#endif
    if (fontId >= 0)
    {
        QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
        QFontDatabase::setApplicationEmojiFontFamilies(fontFamilies);
    } else
    {
        qDebug() << "could not load emoji font!";
    }
#endif

    QStringList arguments = QApplication::arguments();
    // A throne:// URL may be passed as a launch argument (Windows/Linux), and so may
    // config files opened with the app. Both are delivered after the window is up, or
    // forwarded to the primary instance via the socket below. Files are resolved
    // before the working directory moves, since their paths may be relative to it.
    const QString launchDeeplink = Deeplink_ExtractFromArgs(arguments);
    const QStringList launchFiles = LaunchFiles_ExtractFromArgs(arguments, QDir::current());

    // Clean
    QDir::setCurrent(QApplication::applicationDirPath());
    if (QFile::exists("updater.old")) {
        QFile::remove("updater.old");
    }

#ifdef Q_OS_WIN
    // An elevated replacement launched while enabling the kill switch waits
    // here, before opening the settings database or checking the singleton.
    // This avoids racing the original process's final settings save and local
    // server teardown while the persistent WFP baseline is already active.
    const int waitForProcessIndex = arguments.indexOf("--wait-for-process");
    if (waitForProcessIndex >= 0) {
        if (waitForProcessIndex + 1 >= arguments.size()) {
            QMessageBox::critical(nullptr, "Throne kill switch",
                                  "Missing process ID for the protected restart.");
            return 1;
        }
        bool processIdOk = false;
        const DWORD processId =
            arguments.at(waitForProcessIndex + 1).toULong(&processIdOk);
        if (!processIdOk || processId == 0) {
            QMessageBox::critical(nullptr, "Throne kill switch",
                                  "Invalid process ID for the protected restart.");
            return 1;
        }
        const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, processId);
        if (process != nullptr) {
            const DWORD waitResult = WaitForSingleObject(process, 60000);
            CloseHandle(process);
            if (waitResult != WAIT_OBJECT_0) {
                QMessageBox::critical(
                    nullptr, "Throne kill switch",
                    "The previous Throne instance did not exit in time. Fail-closed "
                    "rules remain active; start Throne as Administrator to recover.");
                return 1;
            }
        } else if (GetLastError() != ERROR_INVALID_PARAMETER) {
            // ERROR_INVALID_PARAMETER means the original PID has already
            // disappeared. Any other error leaves singleton/settings ordering
            // unknown, so keep the persistent block and require recovery.
            QMessageBox::critical(
                nullptr, "Throne kill switch",
                "The previous Throne instance could not be monitored. Fail-closed "
                "rules remain active; start Throne as Administrator to recover.");
            return 1;
        }
        arguments.removeAt(waitForProcessIndex + 1);
        arguments.removeAt(waitForProcessIndex);
    }

    const bool earlyDisableKillSwitch =
        arguments.contains("--disable-kill-switch");
    const bool earlyPrepareKillSwitch =
        arguments.contains("--prepare-kill-switch");
    if (earlyDisableKillSwitch && earlyPrepareKillSwitch) {
        QMessageBox::critical(nullptr, "Throne kill switch",
                              "Conflicting kill-switch maintenance options.");
        return 2;
    }

    // Connectivity recovery must not depend on a healthy settings database.
    // Remove only the marked Throne WFP objects first; a non-quiet invocation
    // continues below and also persists the disabled preference if the DB can
    // be opened. Quiet callers are the uninstaller or a parent Throne process,
    // which either deletes the DB or performs that save itself.
    if (earlyDisableKillSwitch) {
        if (!Windows_IsInAdmin()) {
            auto elevatedArguments = arguments;
            elevatedArguments.removeFirst();
            const uint result = WinCommander::runProcessElevated(
                QApplication::applicationFilePath(), elevatedArguments,
                QApplication::applicationDirPath(), WinCommander::WindowHidden, true);
            return result == 0 ? 0 : 1;
        }
        WindowsWfpKillSwitchBackend recoveryBackend;
        const auto recoveryResult = recoveryBackend.disable();
        if (!recoveryResult) {
            qCritical() << "Failed to remove the Throne kill switch:"
                        << recoveryResult.error;
            if (!arguments.contains("--quiet")) {
                QMessageBox::critical(nullptr, "Throne kill-switch recovery",
                                      "Failed to remove Throne's kill-switch rules.\n\n" +
                                          recoveryResult.error);
            }
            return 1;
        }
        if (arguments.contains("--quiet")) {
            qInfo() << "Throne kill-switch rules were removed.";
            return 0;
        }
    }
#endif
    // dirs & clean
    auto wd = QDir(QApplication::applicationDirPath());
    bool useAppdata = false;
    QString appdataDir;
    if (arguments.contains("-appdata")) {
        useAppdata = true;
        int appdataIndex = arguments.indexOf("-appdata");
        if (arguments.size() > appdataIndex + 1 && !arguments.at(appdataIndex + 1).startsWith("-")) {
            appdataDir = arguments.at(appdataIndex + 1);
        }
    }
#ifdef NKR_CPP_USE_APPDATA
    useAppdata = true; // Example: Package & MacOS
#endif
    QApplication::setApplicationName("Throne");
    if(useAppdata) {
        if (!appdataDir.isEmpty()) {
            wd.setPath(appdataDir);
        } else {
            wd.setPath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        }
    } else {
        const QDir userWd(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
        if (AdoptUserConfigDir(wd, userWd)) {
            wd = userWd;
            useAppdata = true;
        }
    }
    if (!wd.exists()) wd.mkpath(wd.absolutePath());
    if (!wd.exists("config")) wd.mkdir("config");
    const QString configDir = wd.absoluteFilePath("config");
    QDir::setCurrent(configDir);
    QDir("temp").removeRecursively();

    // Record app start for the Runtime Stats uptime readout.
    appStartEpoch = QDateTime::currentSecsSinceEpoch();

    // Load database
    Configs::initDB(QString(QDir::currentPath() + QDir::separator() + "throne.db").toStdString());

    Logging::SetLevel(Logging::LevelFromString(Configs::dataManager->settingsRepo->log_file_level));

    // Start traffic-statistics maintenance (startup downsample + background rollup).
    Stats::trafficStatsManager->Init();

    // Store Flags
    Configs::dataManager->settingsRepo->argv = arguments;
    if (Configs::dataManager->settingsRepo->argv.contains("-many")) Configs::dataManager->settingsRepo->flag_many = true;
    if (Configs::dataManager->settingsRepo->argv.contains("-tray")) Configs::dataManager->settingsRepo->flag_tray = true;
    if (Configs::dataManager->settingsRepo->argv.contains("-debug")) Configs::dataManager->settingsRepo->flag_debug = true;
    if (Configs::dataManager->settingsRepo->argv.contains("-flag_restart_tun_on")) Configs::dataManager->settingsRepo->flag_restart_tun_on = true;
    if (Configs::dataManager->settingsRepo->argv.contains("-flag_restart_dns_set")) Configs::dataManager->settingsRepo->flag_dns_set = true;
    Configs::dataManager->settingsRepo->flag_use_appdata = useAppdata;
    if(useAppdata && !appdataDir.isEmpty()) Configs::dataManager->settingsRepo->appdataDir = appdataDir;
#ifdef NKR_CPP_DEBUG
    Configs::dataManager->settingsRepo->flag_debug = true;
#endif

#ifdef Q_OS_WIN
    // Recovery runs before the core, instance server, or any network client.
    // It removes only Throne's deterministic WFP objects and is also suitable
    // for an unattended uninstaller invocation.
    const bool disableKillSwitch = arguments.contains("--disable-kill-switch");
    const bool prepareKillSwitch = arguments.contains("--prepare-kill-switch");
    if (disableKillSwitch || prepareKillSwitch) {
        if (disableKillSwitch && prepareKillSwitch) {
            qCritical() << "Conflicting kill-switch maintenance options.";
            return 2;
        }
        if (!Configs::IsAdmin()) {
            auto elevatedArguments = arguments;
            elevatedArguments.removeFirst();
            const uint result = WinCommander::runProcessElevated(
                QApplication::applicationFilePath(), elevatedArguments,
                QApplication::applicationDirPath(), WinCommander::WindowHidden, true);
            return result == 0 ? 0 : 1;
        }

        WindowsWfpKillSwitchBackend recoveryBackend;
        // A non-quiet disable was already performed before DB initialization,
        // so only its persisted preference remains to be updated here.
        const auto maintenanceResult = disableKillSwitch
                                           ? Configs_sys::KillSwitchResult::Success()
                                           : recoveryBackend.ensureBaseline();
        if (!maintenanceResult) {
            qCritical() << "Failed to update the Throne kill switch:"
                        << maintenanceResult.error;
            if (!arguments.contains("--quiet")) {
                QMessageBox::critical(nullptr, "Throne kill-switch recovery",
                                      "Failed to update Throne's kill-switch rules.\n\n" +
                                          maintenanceResult.error);
            }
            return 1;
        }
        // Persist only after the requested WFP transaction was committed.
        Configs::dataManager->settingsRepo->kill_switch_enabled = prepareKillSwitch;
        Configs::dataManager->settingsRepo->Save();
        qInfo() << (disableKillSwitch
                        ? "Throne kill-switch rules were removed."
                        : "Throne kill-switch rules were installed.");
        if (!arguments.contains("--quiet")) {
            QMessageBox::information(nullptr, "Throne kill-switch recovery",
                                     disableKillSwitch
                                         ? "The Throne kill switch was disabled and its rules were removed."
                                         : "The Throne kill switch was installed. Start Throne as Administrator to connect.");
        }
        return 0;
    }

    // Persistent rules intentionally outlive both processes. Detect them even
    // when the settings database was lost or a save was interrupted, and gain
    // the rights required to retain/reconcile them before ThroneCore starts.
    WindowsWfpKillSwitchBackend startupProbe;
    const auto baselineStatus = startupProbe.queryBaseline();
    const bool persistentProtectionPresent = baselineStatus.mayBeActive();
    const bool protectionRequested =
        Configs::dataManager->settingsRepo->kill_switch_enabled ||
        persistentProtectionPresent;

    if (protectionRequested && arguments.contains("-many")) {
        QMessageBox::critical(nullptr, "Throne kill switch",
                              "Multiple Throne instances are not supported while the kill switch is active.");
        return 1;
    }
    if (protectionRequested && !Configs::IsAdmin()) {
        auto elevatedArguments = arguments;
        elevatedArguments.removeFirst();
        const uint result = WinCommander::runProcessElevated(
            QApplication::applicationFilePath(), elevatedArguments,
            QApplication::applicationDirPath(), WinCommander::WindowNormal, false);
        if (result == static_cast<uint>(-1)) {
            QMessageBox::critical(nullptr, "Throne kill switch",
                                  "Administrator permission is required to restore fail-closed protection.");
            return 1;
        }
        return 0;
    }
#endif

#ifdef Q_OS_LINUX
    QApplication::addLibraryPath(QApplication::applicationDirPath() + "/usr/plugins");
#endif

    // dispatchers
    DS_cores = new QThread;
    DS_cores->start();

    LogThread = new QThread;
    LogThread->start();

// icons
    QIcon::setFallbackSearchPaths(QStringList{
        ":/icon",
    });

    // icon for no theme
    if (QIcon::themeName().isEmpty()) {
        QIcon::setThemeName("breeze");
    }

#ifdef Q_OS_WIN
    if (Configs::dataManager->settingsRepo->windows_set_admin && !Configs::IsAdmin() && !Configs::dataManager->settingsRepo->disable_run_admin)
    {
        Configs::dataManager->settingsRepo->windows_set_admin = false; // so that if permission denied, we will run as user on the next run
        Configs::dataManager->settingsRepo->Save();
        WinCommander::runProcessElevated(QApplication::applicationFilePath(), {}, "", 1, false);
        QApplication::quit();
        return 0;
    }
#endif

    // dataManager->settingsRepo & Flags
    if (Configs::dataManager->settingsRepo->start_minimal) Configs::dataManager->settingsRepo->flag_tray = true;

    // Translate
    QString locale;
    switch (Configs::dataManager->settingsRepo->language) {
        case 1: // English
            break;
        case 2:
            locale = "zh_CN";
            break;
        case 3:
            locale = "fa_IR"; // farsi(iran)
            break;
        case 4:
            locale = "ru_RU"; // Russian
            break;
        default:
            locale = QLocale().name();
    }
    QGuiApplication::tr("QT_LAYOUT_DIRECTION");
    loadTranslate(locale);

    // Check if another instance is running
    QByteArray hashBytes = QCryptographicHash::hash(wd.absolutePath().toUtf8(), QCryptographicHash::Md5).toBase64(QByteArray::OmitTrailingEquals);
    hashBytes.replace('+', '0').replace('/', '1');
    auto serverName = LOCAL_SERVER_PREFIX + QString::fromUtf8(hashBytes);
    qDebug() << "server name: " << serverName;
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (socket.waitForConnected(250))
    {
        qDebug() << "Another instance is running, let's wake it up and quit";
        // Hand off whatever we were launched with so the primary instance handles it:
        // one item per line, a throne:// url or a file:// url. Paths go over as urls
        // so that a name containing a newline cannot break the framing.
        QStringList payload;
        if (!launchDeeplink.isEmpty()) payload << launchDeeplink;
        for (const auto &file : launchFiles) payload << QUrl::fromLocalFile(file).toString();
        if (!payload.isEmpty()) {
            socket.write(payload.join('\n').toUtf8());
            socket.flush();
            socket.waitForBytesWritten(250);
        }
        socket.disconnectFromServer();
        return 0;
    }

    // Must follow the single-instance check: opening the log earlier truncates
    // the running instance's file and leaves a marker it would report as a crash.
    Logging::Init(configDir);
    LOG_INFO(QString("appdata mode: %1").arg(useAppdata ? "yes" : "no"));
#ifdef Q_OS_WIN
    Windows_SetCrashDumpPath();
    Windows_ConfigureWER();
#endif

    // QLocalServer
    QLocalServer server(qApp);
    server.setSocketOptions(QLocalServer::WorldAccessOption);
    if (!server.listen(serverName)) {
        qWarning() << "Failed to start QLocalServer! Error:" << server.errorString();
        Logging::Shutdown();
        return 1;
    }
    QObject::connect(&server, &QLocalServer::newConnection, qApp, [&] {
        auto s = server.nextPendingConnection();
        qDebug() << "Another instance tried to wake us up on " << serverName << s;
        // The waking instance may forward deeplinks and opened files as payload, one
        // url per line. Only whole lines are handled as they arrive; the tail, which
        // carries no trailing newline, is flushed once the peer is done.
        auto pending = std::make_shared<QByteArray>();
        auto handleLine = [](const QString &line) {
            if (line.startsWith("throne://")) {
                Deeplink_Submit(line);
            } else if (line.startsWith("file://")) {
                LaunchFiles_Submit({QUrl(line).toLocalFile()});
            }
        };
        auto readPayload = [s, pending, handleLine](bool last) {
            pending->append(s->readAll());
            while (true) {
                const auto at = pending->indexOf('\n');
                if (at < 0) break;
                handleLine(QString::fromUtf8(pending->first(at)).trimmed());
                pending->remove(0, at + 1);
            }
            if (last) {
                handleLine(QString::fromUtf8(*pending).trimmed());
                pending->clear();
            }
        };
        QObject::connect(s, &QLocalSocket::readyRead, s, [readPayload] { readPayload(false); });
        QObject::connect(s, &QLocalSocket::disconnected, s, [readPayload] { readPayload(true); });
        QObject::connect(s, &QLocalSocket::disconnected, s, &QLocalSocket::deleteLater);
        readPayload(false); // in case the payload already arrived
        // raise main window
        if (MW_dialog_message) MW_dialog_message(MwMessage::Raise, {});
    });
    QObject::connect(qApp, &QApplication::aboutToQuit, [&]
    {
        server.close();
        QLocalServer::removeServer(serverName);
        // Every quit path lands here; missing it is reported as a crash next start.
        Logging::Shutdown();
    });

#ifdef Q_OS_LINUX
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
#endif

#ifdef Q_OS_WIN
    auto eventFilter = new PowerOffTaskkillFilter(signal_handler);
    a.installNativeEventFilter(eventFilter);
#endif

#ifdef Q_OS_MACOS
    QObject::connect(qApp, &QGuiApplication::commitDataRequest, [&](QSessionManager &manager)
    {
        Q_UNUSED(manager);
        signal_handler(0);
    });
#endif

    API::defaultClient = new API::Client();

    if (!UI_InitMainWindow()) {
        Logging::Shutdown();
        return 1;
    }

    Configs::dataManager->RunDeferredMaintenance();

    if (Logging::PreviousSessionCrashed()) {
        MW_show_log(QObject::tr("[Warn] Throne did not shut down cleanly last time. "
                                "Diagnostics were saved to: %1").arg(Logging::LogDir()));
    }

    // Deliver a deeplink and any files passed on the command line (cold start), then
    // replay whatever arrived during startup (e.g. a macOS FileOpen event before the
    // window existed).
    if (!launchDeeplink.isEmpty()) Deeplink_Submit(launchDeeplink);
    Deeplink_FlushPending();
    LaunchFiles_Submit(launchFiles);
    LaunchFiles_FlushPending();

    return QApplication::exec();
}
