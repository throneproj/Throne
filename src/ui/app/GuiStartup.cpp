#include "include/ui/app/GuiStartup.h"

#include <QApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QLocalSocket>
#include <QTranslator>
#include <QUrl>

#include "include/engine/app/InstanceServer.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/global/Notice.hpp"
#include "include/sys/HostPresence.hpp"

#ifdef Q_OS_WIN
#include <3rdparty/WinCommander.hpp>
#include "include/sys/windows/eventHandler.h"
#include "include/sys/windows/WinVersion.h"
#endif
#ifdef Q_OS_MACOS
#include <QFileOpenEvent>
#endif

namespace {
#ifdef Q_OS_MACOS
    // macOS reuses the running app and delivers throne:// URLs and opened files as a QFileOpenEvent, never via argv.
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

    QTranslator *guiStartupTranslator = nullptr;

    void guiStartupLoadTranslate(const QString &locale) {
        QT_TRANSLATE_NOOP("QPlatformTheme", "Cancel");
        QT_TRANSLATE_NOOP("QPlatformTheme", "Apply");
        QT_TRANSLATE_NOOP("QPlatformTheme", "Yes");
        QT_TRANSLATE_NOOP("QPlatformTheme", "No");
        QT_TRANSLATE_NOOP("QPlatformTheme", "OK");
        if (guiStartupTranslator != nullptr) {
            guiStartupTranslator->deleteLater();
        }
        guiStartupTranslator = new QTranslator;
        QLocale::setDefault(QLocale(locale));
        const QString diskPath = QCoreApplication::applicationDirPath()+"/translations/" + locale + ".qm";
        const QString qrcPath = ":/translations/" + locale + ".qm";
        bool loadOK=false;
        if (QFileInfo::exists(diskPath)) {
            loadOK = guiStartupTranslator->load(diskPath);
        }
        if (!loadOK) {
            loadOK = guiStartupTranslator->load(qrcPath);
        }
        if (loadOK) {
            QCoreApplication::installTranslator(guiStartupTranslator);
        }
    }

#ifdef Q_OS_WIN
    // Flags carry over; the deeplink and files go last as absolute paths, since the working directory has moved since launch.
    QStringList ElevatedRelaunchArguments(const QStringList &arguments, const QString &deeplink, const QStringList &files) {
        QStringList out;
        for (qsizetype i = 1; i < arguments.size(); i++) {
            const auto &arg = arguments.at(i);
            if (!arg.startsWith('-')) continue;
            out << arg;
            if (arg == "-appdata" && i + 1 < arguments.size() && !arguments.at(i + 1).startsWith('-')) out << arguments.at(++i);
        }
        out << QString("-wait_pid=%1").arg(QCoreApplication::applicationPid());
        if (!deeplink.isEmpty()) out << deeplink;
        return out + files;
    }
#endif
} // namespace

namespace GuiStartup {
    void InstallMacOpenEventFilter([[maybe_unused]] QApplication &app) {
#ifdef Q_OS_MACOS
        // Install before the event loop so launch-by-deeplink FileOpen events are caught.
        app.installEventFilter(new MacOpenEventFilter(&app));
#endif
    }

    void LoadEmojiFont() {
#if !defined(Q_OS_MACOS) && (QT_VERSION >= QT_VERSION_CHECK(6,9,0))
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
    }

    void HandleLaunchLine([[maybe_unused]] QLocalSocket *socket, const QString &line) {
        if (line.startsWith("throne://")) {
            Deeplink_Submit(line);
        } else if (line.startsWith("file://")) {
            LaunchFiles_Submit({QUrl(line).toLocalFile()});
        }
    }

    void SetupIconTheme() {
        QIcon::setFallbackSearchPaths(QStringList{
            ":/icon",
        });

        if (QIcon::themeName().isEmpty()) {
            QIcon::setThemeName("breeze");
        }
    }

#ifdef Q_OS_WIN
    bool RelaunchElevatedIfNeeded(Engine::InstanceServer &server, const QStringList &arguments, const QString &launchDeeplink,
                                  const QStringList &launchFiles) {
        if (Configs::dataManager->settingsRepo->windows_set_admin && !Configs::IsAdmin() && !Configs::dataManager->settingsRepo->disable_run_admin)
        {
            Configs::dataManager->settingsRepo->windows_set_admin = false; // so that if permission denied, we will run as user on the next run
            Configs::dataManager->settingsRepo->Save();
            // Hand-offs already accepted are read into the pending buffers first; closing the server would drop them.
            const QDeadlineTimer pump(200);
            while (!pump.hasExpired()) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            // Not listening during the UAC prompt, so a new launch waits for the elevated child; a declined prompt carries on as user.
            server.close();
            const QString pendingDeeplink = Deeplink_TakePending();
            const QStringList pendingFiles = LaunchFiles_TakePending();
            if (!launchDeeplink.isEmpty() && !pendingDeeplink.isEmpty()) LOG_WARN("dropped the handed-off deeplink: " + pendingDeeplink);
            const auto elevatedArguments = ElevatedRelaunchArguments(arguments, launchDeeplink.isEmpty() ? pendingDeeplink : launchDeeplink,
                                                                     launchFiles + pendingFiles);
            if (WinCommander::runProcessElevated(QApplication::applicationFilePath(), elevatedArguments, "", 1, false) == 0) return true;
            if (!server.listen()) LOG_WARN("could not restart the instance server: " + server.errorString());
            Deeplink_Submit(pendingDeeplink);
            LaunchFiles_Submit(pendingFiles);
        }
        return false;
    }
#endif

    void LoadTranslator() {
        QString locale;
        switch (Configs::dataManager->settingsRepo->language) {
            case 1: // English
                break;
            case 2:
                locale = "zh_CN";
                break;
            case 3:
                locale = "fa_IR";
                break;
            case 4:
                locale = "ru_RU";
                break;
            default:
                locale = QLocale().name();
        }
        QGuiApplication::tr("QT_LAYOUT_DIRECTION");
        guiStartupLoadTranslate(locale);
    }

    void InstallSessionEndHooks([[maybe_unused]] QApplication &app, [[maybe_unused]] void (*onSessionEnd)(int)) {
#ifdef Q_OS_WIN
        auto eventFilter = new PowerOffTaskkillFilter(onSessionEnd);
        app.installNativeEventFilter(eventFilter);
#endif

#ifdef Q_OS_MACOS
        QObject::connect(qApp, &QGuiApplication::commitDataRequest, [onSessionEnd](QSessionManager &manager)
        {
            Q_UNUSED(manager);
            onSessionEnd(0);
        });
#endif
    }

    void WarnAboutHeadless() {
        const auto holder = Sys::HostPresence::OtherHeadless();
        if (!holder) return;
        const QString text = QCoreApplication::translate("GuiStartup", "A background Throne (PID %1) is running on this computer (data directory: %2). "
                                                                       "Tun mode is unavailable here while it runs; stop it or use thronectl.")
                                 .arg(QString::number(holder->pid), QDir::toNativeSeparators(holder->dataDir));
        MW_show_log("[Warn] " + text);
        // An autostart must not leave a parentless box behind; setTun's own notice covers an actual Tun attempt.
        if (!Configs::dataManager->settingsRepo->flag_tray)
            PostNotice({NoticeLevel::Warning, QCoreApplication::translate("GuiStartup", "Throne is running in the background"), text});
    }
} // namespace GuiStartup
