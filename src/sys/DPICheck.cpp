#include "include/sys/DPICheck.hpp"

#include "include/global/Configs.hpp"

#include <QApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace
{
    bool startDetachedHidden(const QString &program, const QString &workingDirectory, const QStringList &arguments = {})
    {
        QProcess process;
        process.setProgram(program);
        process.setArguments(arguments);
        process.setWorkingDirectory(workingDirectory);
#ifdef Q_OS_WIN
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif
        return process.startDetached();
    }

    QString buildMixedProxyUrl()
    {
        const auto *repo = Configs::dataManager->settingsRepo;
        QString host = repo->inbound_address;
        if (host == "::" || host.isEmpty())
            host = QStringLiteral("127.0.0.1");
        else if (host == QStringLiteral("0.0.0.0"))
            host = QStringLiteral("127.0.0.1");

        QString auth;
        if (repo->inbound_auth) {
            auth = QStringLiteral("%1:%2@")
                       .arg(QString::fromUtf8(QUrl::toPercentEncoding(repo->inbound_user)),
                            QString::fromUtf8(QUrl::toPercentEncoding(repo->inbound_pass)));
        }

        return QStringLiteral("socks5://%1%2:%3").arg(auth, host).arg(repo->inbound_socks_port);
    }

    bool canRunThroughSingBox()
    {
        if (Configs::dataManager == nullptr || Configs::dataManager->settingsRepo == nullptr)
            return false;

        const auto *repo = Configs::dataManager->settingsRepo;
        return repo->core_running && repo->started_id >= 0 && !repo->disable_mixed_inbound;
    }

    QString getStateFilePath()
    {
        const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dirPath);
        return dirPath + "/last_dpi_check.txt";
    }

    bool wasRunToday()
    {
        QFile file(getStateFilePath());

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;

        const QString lastRunDate = QString::fromUtf8(file.readAll()).trimmed();
        return lastRunDate == QDate::currentDate().toString(Qt::ISODate);
    }

    void saveRunDate()
    {
        QFile file(getStateFilePath());

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return;

        file.write(QDate::currentDate().toString(Qt::ISODate).toUtf8());
    }
}

namespace DpiCheck
{
    void TryRunDaily()
    {
        if (wasRunToday())
            return;

        if (Configs::dataManager == nullptr
            || Configs::dataManager->settingsRepo == nullptr
            || !Configs::dataManager->settingsRepo->dpi_consent
            || !canRunThroughSingBox())
            return;

        const QString checkerDir = QApplication::applicationDirPath() + "/dpi-checker";
#ifdef Q_OS_WIN
        const QString checkerPath = checkerDir + "/dpi_launch.exe";
#else
        const QString checkerPath = checkerDir + "/dpi_launch";
#endif

        if (!QFile::exists(checkerPath))
            return;

        QStringList arguments;
        const QString proxyUrl = buildMixedProxyUrl();
        if (!proxyUrl.isEmpty())
            arguments << QStringLiteral("--proxy") << proxyUrl;

        const bool started = startDetachedHidden(
            QFileInfo(checkerPath).absoluteFilePath(),
            QFileInfo(checkerDir).absoluteFilePath(),
            arguments);

        if (started)
            saveRunDate();
    }
}