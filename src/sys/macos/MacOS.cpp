#include "include/sys/macos/MacOS.h"

#include <QProcess>
#include <QString>
#include <QStringList>

// Wrap a string in single quotes for a POSIX shell, escaping any embedded
// single quote with the '\'' idiom. After this the value cannot break out of
// the shell command, regardless of metacharacters in the path.
static QString shellSingleQuote(const QString &s) {
    QString out = s;
    out.replace("'", "'\\''");
    return "'" + out + "'";
}

// Escape a string so it can be safely embedded inside an AppleScript
// double-quoted string literal.
static QString appleScriptStringEscape(const QString &s) {
    QString out = s;
    out.replace("\\", "\\\\");
    out.replace("\"", "\\\"");
    return out;
}

int Mac_Run_Command(const QString &corePath) {
    // Build the privileged shell command with the path shell-quoted, then escape
    // the whole thing for the AppleScript "do script" string. osascript is invoked
    // through QProcess with each -e as a separate argument, so there is no outer
    // /bin/sh to inject into either.
    const QString quotedPath = shellSingleQuote(corePath);
    const QString shellCommand =
        QString("sudo chown root:wheel %1 && sudo chmod u+s %1").arg(quotedPath);
    const QString doScript = appleScriptStringEscape(shellCommand);

    return QProcess::execute("osascript", QStringList{
        "-e", "tell application \"Terminal\" to activate",
        "-e", QString("tell application \"Terminal\" to do script \"%1\"").arg(doScript),
    });
}
