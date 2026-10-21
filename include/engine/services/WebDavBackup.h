#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

// Backups on a WebDAV server, shared with ThroneForAndroid: one folder of .thrbackup files plus a throne_backups.json
// index. Every function but FromSettings runs synchronously on a worker thread and returns an error text, empty on
// success.
namespace Engine::WebDavBackup {
    struct Config {
        QString server;
        QString user;
        QString pass;
        // '/'-separated folder under the server URL; empty means "Throne".
        QString path;
        bool useProxy = false;
        QString deviceId;
        QString deviceName;
    };

    // Main thread: the saved settings and this device's identity (generated on first use).
    Config FromSettings();

    struct Entry {
        QString file;
        // UTC; invalid when unknown.
        QDateTime createdAt;
        // -1 when unknown.
        qint64 size = -1;
        QString sha256;
        QString deviceId;
        QString deviceName;
        QString platform;
        QString appVersion;
        QStringList parts;
        // Described by the index; otherwise synthesized from the folder listing alone.
        bool indexed = false;
    };

    // Reaches the server and creates the folder when it is missing.
    QString Test(const Config &config);

    // Uploads the archive at localPath under a new name (*fileName), then records it in the index. *indexWarning gets
    // the reason when only the index update failed: the backup itself is uploaded.
    QString Upload(const Config &config, const QString &localPath, const QStringList &parts, QString *fileName,
                   QString *indexWarning);

    // The folder's backups, newest first. *indexWarning gets the reason when the index could not be read.
    QString List(const Config &config, QList<Entry> *entries, QString *indexWarning);

    // Writes the backup to localPath and checks it against the index's size and SHA-256; removes localPath on failure.
    QString Download(const Config &config, const Entry &entry, const QString &localPath);

    // Deletes the backup, then drops it from the index (*indexWarning as in Upload).
    QString Delete(const Config &config, const Entry &entry, QString *indexWarning);
} // namespace Engine::WebDavBackup
