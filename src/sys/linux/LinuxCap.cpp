#include "include/sys/linux/LinuxCap.h"

#include <QDebug>
#include <QProcess>
#include <QStandardPaths>

int Linux_Run_Command(const QString &commandName, const QStringList &args) {
    // Run the privileged command through pkexec with an explicit argument list.
    // Never build a shell string: passing arguments as a list means metacharacters
    // in the resolved exec path or in args (e.g. an install dir like ".../x;touch /etc/evil")
    // are treated as literal data and cannot inject into the root command.
    QStringList pkexecArgs;
    pkexecArgs << Linux_FindCapProgsExec(commandName);
    pkexecArgs << args;
    return QProcess::execute("pkexec", pkexecArgs);
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
