#include "include/sys/linux/LinuxCap.h"

#include <QDebug>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QFile>
#include <QTextStream>

int Linux_Run_Command(const QString &commandName, const QString &args) {
    auto command = QString("pkexec %1 %2").arg(Linux_FindCapProgsExec(commandName)).arg(args);
    return system(command.toStdString().c_str());
}

bool Linux_HavePkexec() {
    QProcess p;
    p.setProgram("pkexec");
    p.setArguments({"--help"});
    p.setProcessChannelMode(QProcess::SeparateChannels);
    p.start();
    p.waitForFinished(500);
    return (p.exitStatus() == QProcess::NormalExit ? p.exitCode() : -1) == 0;
}

QString Linux_FindCapProgsExec(const QString &name) {
    QString exec = QStandardPaths::findExecutable(name);
    if (exec.isEmpty())
        exec = QStandardPaths::findExecutable(name, {"/usr/sbin", "/sbin"});

    if (exec.isEmpty())
        qDebug() << "Executable" << name << "could not be resolved";
    else
        qDebug() << "Found exec" << name << "at" << exec;

    return exec.isEmpty() ? name : exec;
}

int Linux_Run_RootScript(const QStringList &commands) {
    QTemporaryFile scriptFile;
    scriptFile.setAutoRemove(true);
    if (!scriptFile.open()) return -1;
    QTextStream out(&scriptFile);
    out << "#!/bin/sh\n";
    for (const auto &cmd : commands) out << cmd << "\n";
    scriptFile.flush();
    scriptFile.setPermissions(QFile::ExeUser | QFile::ReadUser | QFile::WriteUser);
    scriptFile.close();
    QString pkexecCmd = QString("pkexec sh %1").arg(scriptFile.fileName());
    return system(pkexecCmd.toStdString().c_str());
}
