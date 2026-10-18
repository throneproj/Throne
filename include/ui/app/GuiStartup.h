#pragma once

#include <QString>
#include <QStringList>

class QApplication;
class QLocalSocket;

namespace Engine {
    class InstanceServer;
}

// The window-only half of startup, called from main() in order.
namespace GuiStartup {
    // No-op off macOS.
    void InstallMacOpenEventFilter(QApplication &app);

    void LoadEmojiFont();

    void HandleLaunchLine(QLocalSocket *socket, const QString &line);

    void SetupIconTheme();

#ifdef Q_OS_WIN
    // True when an elevated copy was started and this process must return 0.
    bool RelaunchElevatedIfNeeded(Engine::InstanceServer &server, const QStringList &arguments, const QString &launchDeeplink,
                                  const QStringList &launchFiles);
#endif

    void LoadTranslator();

    void InstallSessionEndHooks(QApplication &app, void (*onSessionEnd)(int));

    // After the engine init: a headless Throne running on this host (another data dir) gets a log line and a passive notice.
    void WarnAboutHeadless();
} // namespace GuiStartup
