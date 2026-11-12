#pragma once

#include <QString>
#include <QStringList>

// Values for process_name / process_path / process_path_regex, spelled the way the core's matcher compares them.
namespace Configs::ProcessRules {
    // Go RE2 quoting: QRegularExpression::escape also escapes non-ASCII letters, which RE2 rejects.
    QString QuoteMeta(const QString& text);

    // A process_path_regex for every program under the app's install folder (the outermost .app on macOS,
    // the folder above a versioned one elsewhere); empty for a path without a folder.
    QString FolderPattern(const QString& path, QString* folder = nullptr);

    bool HasVersionedFolder(const QString& path);

    // Interpreters and hosts that run many unrelated programs under one name.
    bool IsSharedHost(const QString& name);

    // The OS's own programs: services, daemons and the Windows directory.
    bool IsSystemProgram(const QString& path);

    // A folder such as System32 or /usr/bin that holds many unrelated programs.
    bool IsSharedFolder(const QString& folder);

    void Learn(const QStringList& names, const QStringList& paths);

    // The spelling the core reports for value when they differ only in case, else empty. attr: process_name or process_path.
    QString KnownSpelling(const QString& attr, const QString& value);
}
