#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "include/database/Database.h"

namespace Engine::BackupArchive {
    struct Info {
        quint32 formatVersion = 0;
        // As the archive wrote it: local time, Qt::TextDate.
        QString createdAt;
        QString platform;
        Configs::BackupParts available;
    };

    // Main thread, before Build: flushes the settings when they are part of the backup.
    void Prepare(const Configs::BackupParts &parts);

    // Any thread. Writes a .thrbackup archive of the parts to path; *written gets the parts it really holds (icons only
    // when there was an icon file). Returns an error text, empty on success.
    QString Build(const Configs::BackupParts &parts, const QString &path, Configs::BackupParts *written = nullptr);

    // Any thread. Reads and checks the archive at path. Returns an error text, empty on success.
    QString Inspect(const QString &path, Info *info);

    struct ApplyResult {
        Configs::BackupParts restored;
        int skippedRules = 0;
        // The archive was refused as a whole (see untrusted), not a failure of Throne.
        bool refused = false;
    };

    // Main thread, with no profile running. Replaces the chosen parts with the archive's; a settings restore keeps
    // preservedSettingKeys (DB key names). Ids that no longer resolve are reset. The program must restart afterwards:
    // settings are no longer saved. Returns an error text, empty on success.
    // untrusted: the archive came from a remote client, so it must not bring anything an import refuses (profiles that
    // run programs or read local files, non-http subscription URLs); such an archive is refused before any change.
    QString Apply(const QString &path, const Configs::BackupParts &parts, const QStringList &preservedSettingKeys, ApplyResult *result,
                  bool untrusted = false);

    // Any thread. Apply's untrusted check on its own, so a refusal can come before anything is stopped. *refused tells a
    // refused archive from a failure to read it. Returns the reason, empty when the parts pass.
    QString CheckUntrusted(const QString &path, const Configs::BackupParts &parts, bool *refused);

    // API names: profiles, routes, settings, otp, ip_lists, icons.
    QStringList PartNames(const Configs::BackupParts &parts);

    // False with *unknown set to the first unknown name.
    bool PartsFromNames(const QStringList &names, Configs::BackupParts *parts, QString *unknown);

    // A unique path under <configDir>/temp, which Throne wipes at start.
    QString TempPath(const QString &prefix);
} // namespace Engine::BackupArchive
