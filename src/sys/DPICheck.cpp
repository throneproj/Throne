#include "include/sys/DPICheck.hpp"

#include "include/global/Configs.hpp"

#include <QApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace
{
    bool startDetachedHidden(const QString &program, const QString &workingDirectory)
    {
        QProcess process;
        process.setProgram(program);
        process.setWorkingDirectory(workingDirectory);
#ifdef Q_OS_WIN
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif
        return process.startDetached();
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
            || !Configs::dataManager->settingsRepo->dpi_consent)
            return;

        const QString checkerDir = QApplication::applicationDirPath() + "/dpi-checker";
#ifdef Q_OS_WIN
        const QString checkerPath = checkerDir + "/dpi_launch.exe";
#else
        const QString checkerPath = checkerDir + "/dpi_launch";
#endif

        if (!QFile::exists(checkerPath))
            return;

        const bool started = startDetachedHidden(
            QFileInfo(checkerPath).absoluteFilePath(),
            QFileInfo(checkerDir).absoluteFilePath());

        if (started)
            saveRunDate();
    }
}