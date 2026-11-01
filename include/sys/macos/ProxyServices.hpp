#pragma once
#include <QStringList>
#include <QProcess>

namespace MacSystemProxy {
    // Keep command failures separate from an empty list. Never treat a failed networksetup as a clean snapshot.
    inline QString enabledServiceNames(bool succeeded, const QByteArray &output, QStringList &names) {
        names.clear();
        if (!succeeded) return "networksetup -listallnetworkservices failed";
        const auto lines = QString::fromUtf8(output).split('\n', Qt::SkipEmptyParts);
        if (lines.isEmpty()) return "networksetup returned no service list";
        for (qsizetype i = 1; i < lines.size(); ++i) {
            if (!lines[i].startsWith('*')) names << lines[i];
        }
        return {};
    }

    inline QString discoverServices(const QString &program, QStringList &names, int timeoutMs = 10000) {
        QProcess process;
        process.start(program, {"-listallnetworkservices"});
        const bool finished = process.waitForFinished(timeoutMs);
        if (!finished) { process.kill(); process.waitForFinished(1000); }
        return enabledServiceNames(finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0,
                                   process.readAllStandardOutput(), names);
    }
}
