#include "include/engine/services/BackupArchive.h"

#include <cstring>
#include <exception>
#include <string>

#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSaveFile>
#include <QScopeGuard>
#include <QSysInfo>
#include <QUrl>
#include <QUuid>

#include "NkrVersion.h"
#include "include/configs/sub/GroupUpdater.hpp"
#include "include/database/ProfilesRepo.h"
#include "include/engine/ThreadCheck.h"
#include "include/global/Configs.hpp"
#include "include/global/Validation.hpp"
#include "include/scanner/ScanManager.h"

// Error texts are English and equal to the settings dialog's tr() sources, which translates them for display.
namespace Engine::BackupArchive {
    namespace {
        // v1 archives carry no "parts" metadata and are read back as full snapshots.
        constexpr quint32 BACKUP_FORMAT_VERSION = 2;
        constexpr int BACKUP_CONTENT_VERSION = 2;

        Configs::BackupParts BackupPartsFromMeta(quint32 formatVersion, const QJsonObject &meta,
                                                 const QMap<QString, QByteArray> &files) {
            Configs::BackupParts p;
            bool hasIcons = false;
            for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
                if (it.key().startsWith("icons/")) { hasIcons = true; break; }
            }
            if (formatVersion >= 2 && meta.contains("parts")) {
                const QJsonObject po = meta["parts"].toObject();
                p.profiles = po["profiles"].toBool() && files.contains("database");
                p.routes = po["routes"].toBool() && files.contains("database");
                p.settings = po["settings"].toBool() && files.contains("database");
                p.otp = po["otp"].toBool() && files.contains("database");
                p.ipLists = po["ipLists"].toBool() && files.contains("database");
                p.icons = po["icons"].toBool() && hasIcons;
            } else {
                p.profiles = p.routes = p.settings = files.contains("database");
                p.icons = hasIcons;
            }
            return p;
        }

        QString ReadArchive(const QString &path, quint32 *formatVersion, QJsonObject *meta, QMap<QString, QByteArray> *files) {
            QFile inFile(path);
            if (!inFile.open(QIODevice::ReadOnly)) return QStringLiteral("Cannot open backup file: %1").arg(path);

            QDataStream stream(&inFile);
            stream.setByteOrder(QDataStream::LittleEndian);
            stream.setVersion(QDataStream::Qt_6_0);

            char magic[4];
            if (stream.readRawData(magic, 4) != 4 || strncmp(magic, "THRN", 4) != 0) return QStringLiteral("Not a valid Throne backup file.");

            stream >> *formatVersion;
            if (stream.status() != QDataStream::Ok) return QStringLiteral("Not a valid Throne backup file.");
            if (*formatVersion < 1 || *formatVersion > BACKUP_FORMAT_VERSION) {
                return QStringLiteral("Unsupported backup format version: %1.\nThis backup may have been created with a newer version of the application.")
                    .arg(*formatVersion);
            }

            QString metaJson;
            stream >> metaJson;
            stream >> *files;
            // A truncated file would otherwise read as a partial or empty map.
            if (stream.status() != QDataStream::Ok) return QStringLiteral("The backup file is damaged or incomplete.");

            const auto doc = QJsonDocument::fromJson(metaJson.toUtf8());
            if (!metaJson.isEmpty() && !doc.isObject()) return QStringLiteral("The backup file is damaged or incomplete.");
            *meta = doc.object();
            return {};
        }

        // SQLite may leave its journal files next to a temp database it had open.
        void RemoveTempDatabase(const QString &path) {
            for (const auto *suffix : {"", "-wal", "-shm", "-journal"}) QFile::remove(path + suffix);
        }

        // The blob is untrusted: SQLite must read it as a sound database before the live one is touched.
        QString CheckDatabase(const QString &path) {
            try {
                SQLite::Database check(path.toStdString(), SQLite::OPEN_READWRITE);
                SQLite::Statement query(check, "PRAGMA quick_check");
                QStringList problems;
                while (problems.size() < 3 && query.executeStep()) problems << QString::fromUtf8(query.getColumn(0).getText());
                if (problems == QStringList{QStringLiteral("ok")}) return {};
                return problems.isEmpty() ? QStringLiteral("the integrity check returned nothing") : problems.join("; ");
            } catch (std::exception &e) {
                return QString::fromUtf8(e.what());
            }
        }

        void RestoreIcons(const QMap<QString, QByteArray> &files) {
            // Icon.cpp also loads Connecting.png, which the icon picker does not offer.
            const QStringList allowed = Configs::Information::iconNames + QStringList{QStringLiteral("Connecting.png")};
            QDir iconsDir("icons");
            iconsDir.mkpath(".");
            for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
                if (!it.key().startsWith("icons/")) continue;
                const QString iconName = it.key().mid(6);
                if (iconName.isEmpty()) continue;
                // Archive keys are untrusted: a name such as "../throne.db" would write outside the icons folder.
                if (QFileInfo(iconName).fileName() != iconName || iconName.contains("..") || !allowed.contains(iconName)) {
                    MW_show_log(QStringLiteral("[Backup] Skipped icon \"%1\": not one of Throne's icon files.").arg(iconName.left(80).simplified()));
                    continue;
                }
                QSaveFile iconFile(iconsDir.filePath(iconName));
                if (!iconFile.open(QIODevice::WriteOnly) || iconFile.write(it.value()) != it.value().size() || !iconFile.commit()) {
                    MW_show_log(QStringLiteral("[Backup] Could not restore icon %1: %2").arg(iconName, iconFile.errorString()));
                }
            }
        }

        QString BackupShownName(const QString &name, const QString &fallback) {
            const QString shown = name.simplified().left(60);
            return shown.isEmpty() ? fallback : QStringLiteral("\"%1\"").arg(shown);
        }

        QString BackupDeviceName(const QString &configured) {
            const QString name = configured.trimmed();
            return name.isEmpty() ? QSysInfo::machineHostName() : name;
        }

        bool BackupHttpUrl(const QString &url) {
            const QString scheme = QUrl(url.trimmed(), QUrl::TolerantMode).scheme();
            return scheme.compare(QLatin1String("http"), Qt::CaseInsensitive) == 0 ||
                   scheme.compare(QLatin1String("https"), Qt::CaseInsensitive) == 0;
        }

        QStringList BackupTableColumns(SQLite::Database &db, const char *table) {
            QStringList columns;
            SQLite::Statement query(db, std::string("PRAGMA table_info(") + table + ")");
            while (query.executeStep()) columns << QString::fromUtf8(query.getColumn(1).getText());
            return columns;
        }

        // Rows of the parts about to be restored that Throne would not accept from a remote client; empty when none.
        QString BackupUntrustedRefusal(const QString &dbPath, const Configs::BackupParts &parts) {
            QStringList shown;
            qsizetype count = 0;
            const auto offend = [&shown, &count](const QString &what) {
                if (++count <= 3) shown << what;
            };
            try {
                SQLite::Database bak(dbPath.toStdString(), SQLite::OPEN_READONLY);
                if (parts.profiles && !BackupTableColumns(bak, "profiles").isEmpty()) {
                    SQLite::Statement rows(bak, "SELECT id, type, name, outbound_json FROM profiles");
                    while (rows.executeStep()) {
                        // Read as ProfilesRepo::profileFromRow reads them, so the check sees what a load would build.
                        const QString type = QString::fromStdString(rows.getColumn(1).getText());
                        QString name = QString::fromStdString(rows.getColumn(2).getText());
                        const QString fallback = QStringLiteral("profile %1").arg(rows.getColumn(0).getInt());
                        const auto profile = Configs::ProfilesRepo::NewProfile(type);
                        const auto outbound = QJsonDocument::fromJson(QString::fromStdString(rows.getColumn(3).getText()).toUtf8());
                        if (outbound.isObject()) profile->outbound->ParseFromJson(outbound.object());
                        if (!profile->outbound->name.isEmpty()) name = profile->outbound->name;
                        if (type == QLatin1String("extracore")) {
                            offend(QStringLiteral("Extra Core profile %1").arg(BackupShownName(name, fallback)));
                            continue;
                        }
                        Validation::Issues issues;
                        profile->outbound->StripForeignState(issues);
                        if (issues.isEmpty()) continue;
                        const QString field = issues.first().field;
                        offend(field.isEmpty() ? QStringLiteral("profile %1").arg(BackupShownName(name, fallback))
                                               : QStringLiteral("profile %1 (it sets %2)").arg(BackupShownName(name, fallback), field));
                    }
                }
                if (parts.profiles && BackupTableColumns(bak, "groups").contains(QStringLiteral("url"))) {
                    SQLite::Statement rows(bak, "SELECT id, name, url FROM groups");
                    while (rows.executeStep()) {
                        const QString url = QString::fromStdString(rows.getColumn(2).getText());
                        if (url.trimmed().isEmpty() || BackupHttpUrl(url)) continue;
                        const QString name = QString::fromStdString(rows.getColumn(1).getText());
                        offend(QStringLiteral("group %1 (its subscription URL is not http or https)")
                                   .arg(BackupShownName(name, QStringLiteral("group %1").arg(rows.getColumn(0).getInt()))));
                    }
                }
                if (const auto columns = BackupTableColumns(bak, "route_profiles");
                    parts.routes && columns.contains(QStringLiteral("is_remote")) && columns.contains(QStringLiteral("remote_url"))) {
                    SQLite::Statement rows(bak, "SELECT id, name, is_remote, remote_url FROM route_profiles");
                    while (rows.executeStep()) {
                        // RoutesRepo reads is_remote as getInt() != 0.
                        if (rows.getColumn(2).getInt() == 0) continue;
                        const QString url = QString::fromStdString(rows.getColumn(3).getText());
                        if (url.trimmed().isEmpty() || BackupHttpUrl(url)) continue;
                        const QString name = QString::fromStdString(rows.getColumn(1).getText());
                        offend(QStringLiteral("routing profile %1 (its remote URL is not http or https)")
                                   .arg(BackupShownName(name, QStringLiteral("routing profile %1").arg(rows.getColumn(0).getInt()))));
                    }
                }
            } catch (std::exception &e) {
                return QStringLiteral("The archive's database could not be checked (%1); restore this archive from Throne's window instead")
                    .arg(QString::fromUtf8(e.what()));
            }
            if (count == 0) return {};
            QString list = shown.join(QStringLiteral(", "));
            if (count > shown.size()) list += QStringLiteral(" and %1 more").arg(count - shown.size());
            return QStringLiteral("The archive holds what Throne does not accept from a remote client: %1; "
                                  "restore this archive from Throne's window instead").arg(list);
        }
    } // namespace

    Device ThisDevice() {
        ENGINE_CHECK_MAIN_THREAD();
        auto &settings = *Configs::dataManager->settingsRepo;
        if (settings.device_id.isEmpty()) {
            settings.device_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            settings.Save();
        }
        return {settings.device_id, BackupDeviceName(settings.device_name)};
    }

    const QStringList &DeviceLocalSettingKeys() {
        static const QStringList keys = {
            QStringLiteral("webdav_server"), QStringLiteral("webdav_user"), QStringLiteral("webdav_pass"), QStringLiteral("webdav_path"),
            QStringLiteral("webdav_use_proxy"), QStringLiteral("device_id"), QStringLiteral("device_name"),
        };
        return keys;
    }

    void Prepare(const Configs::BackupParts &parts) {
        ENGINE_CHECK_MAIN_THREAD();
        ThisDevice();
        if (parts.settings) Configs::dataManager->settingsRepo->Save();
    }

    QString Build(const Configs::BackupParts &parts, const QString &path, Configs::BackupParts *written) {
        if (written != nullptr) *written = {};
        if (!parts.any()) return QStringLiteral("Select at least one part to include in the backup.");

        QMap<QString, QByteArray> files;
        Configs::BackupParts held = parts;
        held.icons = false;

        if (parts.anyDb()) {
            const QString tempDbPath = TempPath(QStringLiteral("backup-"));
            const auto removeTemp = qScopeGuard([&tempDbPath] { RemoveTempDatabase(tempDbPath); });

            try {
                Configs::dataManager->getDatabase().backupSelective(tempDbPath.toStdString(), parts, DeviceLocalSettingKeys());
            } catch (std::exception &e) {
                return QStringLiteral("Failed to create database snapshot: %1").arg(e.what());
            }

            QFile tempDbFile(tempDbPath);
            if (!tempDbFile.open(QIODevice::ReadOnly)) return QStringLiteral("Failed to read database snapshot.");
            files["database"] = tempDbFile.readAll();
            if (tempDbFile.error() != QFileDevice::NoError) return QStringLiteral("Failed to read database snapshot.");
        }

        if (parts.icons) {
            QDir iconsDir("icons");
            if (iconsDir.exists()) {
                for (const QFileInfo &entry : iconsDir.entryInfoList(QDir::Files)) {
                    QFile iconFile(entry.absoluteFilePath());
                    if (iconFile.open(QIODevice::ReadOnly)) {
                        files["icons/" + entry.fileName()] = iconFile.readAll();
                        held.icons = true;
                    }
                }
            }
        }
        if (!held.any()) return QStringLiteral("There is nothing to back up");

        // QSaveFile leaves an existing file at path untouched unless the whole archive was written.
        QSaveFile outFile(path);
        if (!outFile.open(QIODevice::WriteOnly)) return QStringLiteral("Cannot write to: %1").arg(path);

        QDataStream stream(&outFile);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.setVersion(QDataStream::Qt_6_0);

        if (stream.writeRawData("THRN", 4) != 4) return QStringLiteral("Cannot write to: %1").arg(path);
        stream << BACKUP_FORMAT_VERSION;

        QJsonObject partsObj;
        partsObj["profiles"] = held.profiles;
        partsObj["routes"] = held.routes;
        partsObj["settings"] = held.settings;
        partsObj["otp"] = held.otp;
        partsObj["ipLists"] = held.ipLists;
        partsObj["icons"] = held.icons;

        QJsonObject meta;
        meta["backup_version"] = BACKUP_CONTENT_VERSION;
        meta["created_at"] = QDateTime::currentDateTime().toString(Qt::TextDate);
        meta["platform"] = QSysInfo::kernelType();
        meta["parts"] = partsObj;
        meta["app_version"] = QStringLiteral(NKR_VERSION);
        {
            const auto &settings = *Configs::dataManager->settingsRepo;
            QJsonObject device;
            device["id"] = settings.device_id;
            device["name"] = BackupDeviceName(settings.device_name);
            meta["device"] = device;
        }
        stream << QString::fromUtf8(QJsonDocument(meta).toJson(QJsonDocument::Compact));

        stream << files;
        if (stream.status() != QDataStream::Ok || !outFile.commit()) return QStringLiteral("Cannot write to: %1").arg(path);
        if (written != nullptr) *written = held;
        return {};
    }

    QString Inspect(const QString &path, Info *info) {
        quint32 formatVersion = 0;
        QJsonObject meta;
        QMap<QString, QByteArray> files;
        if (auto error = ReadArchive(path, &formatVersion, &meta, &files); !error.isEmpty()) return error;

        const Configs::BackupParts available = BackupPartsFromMeta(formatVersion, meta, files);
        if (!available.any()) return QStringLiteral("This backup file does not contain any restorable data.");

        if (info != nullptr) {
            info->formatVersion = formatVersion;
            info->createdAt = meta["created_at"].toString();
            info->platform = meta["platform"].toString();
            info->available = available;
            info->deviceName = meta["device"].toObject()["name"].toString();
            info->appVersion = meta["app_version"].toString();
        }
        return {};
    }

    QString CheckUntrusted(const QString &path, const Configs::BackupParts &parts, bool *refused) {
        *refused = false;
        if (!parts.profiles && !parts.routes) return {};
        quint32 formatVersion = 0;
        QJsonObject meta;
        QMap<QString, QByteArray> files;
        if (auto error = ReadArchive(path, &formatVersion, &meta, &files); !error.isEmpty()) return error;
        const QString tempDbPath = TempPath(QStringLiteral("check-"));
        const auto removeTemp = qScopeGuard([&tempDbPath] { RemoveTempDatabase(tempDbPath); });
        {
            const QByteArray database = files.take("database");
            QFile tempDbFile(tempDbPath);
            if (!tempDbFile.open(QIODevice::WriteOnly) || tempDbFile.write(database) != database.size() || !tempDbFile.flush()) {
                return QStringLiteral("Failed to create temporary file for restore.");
            }
        }
        if (const auto problem = CheckDatabase(tempDbPath); !problem.isEmpty()) {
            return QStringLiteral("The backup's database is damaged: %1").arg(problem);
        }
        const auto refusal = BackupUntrustedRefusal(tempDbPath, parts);
        *refused = !refusal.isEmpty();
        return refusal;
    }

    QString Apply(const QString &path, const Configs::BackupParts &parts, const QStringList &preservedSettingKeys, ApplyResult *result,
                  bool untrusted) {
        ENGINE_CHECK_MAIN_THREAD();
        if (result != nullptr) *result = {};
        // An update writes through repo caches that the restore leaves stale.
        if (!Subscription::updater()->IsIdle()) return QStringLiteral("A subscription update is running; try again when it finishes.");
        quint32 formatVersion = 0;
        QJsonObject meta;
        QMap<QString, QByteArray> files;
        if (auto error = ReadArchive(path, &formatVersion, &meta, &files); !error.isEmpty()) return error;

        const Configs::BackupParts available = BackupPartsFromMeta(formatVersion, meta, files);
        Configs::BackupParts lacking;
        lacking.profiles = parts.profiles && !available.profiles;
        lacking.routes = parts.routes && !available.routes;
        lacking.settings = parts.settings && !available.settings;
        lacking.otp = parts.otp && !available.otp;
        lacking.ipLists = parts.ipLists && !available.ipLists;
        lacking.icons = parts.icons && !available.icons;
        if (lacking.any()) return QStringLiteral("The backup does not contain: %1").arg(PartNames(lacking).join(", "));
        if (!parts.any()) return QStringLiteral("Select at least one part to restore.");

        int skippedRules = 0;
        if (parts.anyDb()) {
            const QString tempDbPath = TempPath(QStringLiteral("restore-"));
            const auto removeTemp = qScopeGuard([&tempDbPath] { RemoveTempDatabase(tempDbPath); });
            {
                const QByteArray database = files.take("database");
                QFile tempDbFile(tempDbPath);
                if (!tempDbFile.open(QIODevice::WriteOnly) || tempDbFile.write(database) != database.size() || !tempDbFile.flush()) {
                    return QStringLiteral("Failed to create temporary file for restore.");
                }
            }
            if (const auto problem = CheckDatabase(tempDbPath); !problem.isEmpty()) {
                return QStringLiteral("The backup's database is damaged: %1").arg(problem);
            }
            if (untrusted) {
                if (const auto refusal = BackupUntrustedRefusal(tempDbPath, parts); !refusal.isEmpty()) {
                    if (result != nullptr) result->refused = true;
                    return refusal;
                }
            }

            auto &settings = *Configs::dataManager->settingsRepo;
            // Preserved keys keep the database's values, so the live ones must reach it first.
            settings.Save();
            // A running scan would otherwise keep writing its progress and results over the restored rows.
            Scanner::ScanManager::instance()->StopAll(false);
            auto &db = Configs::dataManager->getDatabase();
            QStringList keptKeys = preservedSettingKeys;
            for (const auto &key : DeviceLocalSettingKeys()) {
                if (!keptKeys.contains(key)) keptKeys << key;
            }
            try {
                skippedRules = db.restoreSelective(tempDbPath.toStdString(), parts, keptKeys);
            } catch (std::exception &e) {
                return QStringLiteral("Failed to restore database: %1").arg(e.what());
            }
            // The exit path's settingsRepo->Save() would write the stale in-memory values back over the restore.
            settings.noSave = true;
        }

        if (parts.icons) RestoreIcons(files);

        MW_show_log(QStringLiteral("[Backup] Restored %1 from %2").arg(PartNames(parts).join(", "), QFileInfo(path).fileName()));
        if (result != nullptr) {
            result->restored = parts;
            result->skippedRules = skippedRules;
        }
        return {};
    }

    QStringList PartNames(const Configs::BackupParts &parts) {
        QStringList names;
        if (parts.profiles) names << QStringLiteral("profiles");
        if (parts.routes) names << QStringLiteral("routes");
        if (parts.settings) names << QStringLiteral("settings");
        if (parts.otp) names << QStringLiteral("otp");
        if (parts.ipLists) names << QStringLiteral("ip_lists");
        if (parts.icons) names << QStringLiteral("icons");
        return names;
    }

    bool PartsFromNames(const QStringList &names, Configs::BackupParts *parts, QString *unknown) {
        Configs::BackupParts chosen;
        for (const auto &name : names) {
            if (name == QLatin1String("profiles")) chosen.profiles = true;
            else if (name == QLatin1String("routes")) chosen.routes = true;
            else if (name == QLatin1String("settings")) chosen.settings = true;
            else if (name == QLatin1String("otp")) chosen.otp = true;
            else if (name == QLatin1String("ip_lists")) chosen.ipLists = true;
            else if (name == QLatin1String("icons")) chosen.icons = true;
            else {
                if (unknown != nullptr) *unknown = name;
                return false;
            }
        }
        if (parts != nullptr) *parts = chosen;
        return true;
    }

    QString TempPath(const QString &prefix) {
        // main.cpp wipes <configDir>/temp (the working directory's temp) at every start.
        const QString dir = QDir::current().absoluteFilePath(QStringLiteral("temp"));
        QDir().mkpath(dir);
        return QDir(dir).filePath(prefix + QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
} // namespace Engine::BackupArchive
