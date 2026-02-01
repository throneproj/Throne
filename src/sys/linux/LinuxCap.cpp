#include "include/sys/linux/LinuxCap.h"

#include <QDebug>
#include <QProcess>
#include <QStandardPaths>

int Linux_Run_Command(const QString &commandName, const QString &args) {
    auto command = QString("pkexec %1 %2").arg(Linux_FindCapProgsExec(commandName)).arg(args);
    return system(command.toStdString().c_str());
}

int Linux_ElevateCorePermissions(const QString &corePath) {
    // Single pkexec so user is asked for password only once (chown + chmod).
    // Path is passed as separate argument ($1), so no shell escaping is needed.
    return QProcess::execute("pkexec", QStringList() << "sh" << "-c"
                             << "chown root:root \"$1\" && chmod u+s \"$1\""
                             << "sh" << corePath);
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
