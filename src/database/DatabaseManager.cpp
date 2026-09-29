#include "include/database/GroupsRepo.h"
#include "include/database/MarkersRepo.h"
#include "include/database/OtpProfilesRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/database/TrafficStatsRepo.h"

#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

#include <3rdparty/SQLiteCpp/include/sqlite3.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QThread>

#include <algorithm>

namespace Configs {
    std::string DatabaseManager::deriveStatsDbPath(const std::string& dbPath) {
        const QFileInfo fi(QString::fromStdString(dbPath));
        return QDir(fi.absolutePath()).filePath("throne_stats.db").toStdString();
    }

    // Empty when usable. Only file-level verdicts count; anything else is left for the real open to report.
    QString DatabaseManager::statsDbUnusableReason(const std::string& path) {
        try {
            SQLite::Database probe(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            probe.exec("PRAGMA schema_version");
            // SQLite downgrades to read-only without complaint when the file is not writable.
            if (sqlite3_db_readonly(probe.getHandle(), "main") == 1) return "the file is not writable";
        } catch (const std::exception& e) {
            if (IsFatalDbError(DescribeDbError(e))) return QString::fromUtf8(e.what());
        }
        return {};
    }

    // Renamed rather than deleted, so the file can still be handed over for inspection.
    void DatabaseManager::quarantineDbFile(const std::string& path) {
        const QString base = QString::fromStdString(path);
        const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
        for (const char* suffix : {"", "-wal", "-shm"}) {
            const QString from = base + suffix;
            if (!QFile::exists(from)) continue;
            if (!QFile::rename(from, base + ".broken-" + stamp + suffix) && !QFile::remove(from)) {
                LOG_WARN(QString("could not quarantine %1").arg(from));
            }
        }
    }

    std::string DatabaseManager::prepareStatsDb(const std::string& path) {
        const QString marker = QString::fromStdString(DbRebuildMarkerPath(path));
        const QString reason = QFile::exists(marker) ? QString("the previous session asked for a rebuild")
                                                     : statsDbUnusableReason(path);
        if (reason.isEmpty()) return path;
        LOG_WARN(QString("recreating %1: %2").arg(QString::fromStdString(path), reason));
        quarantineDbFile(path);
        QFile::remove(marker);
        return path;
    }

    DatabaseManager::DatabaseManager(const std::string& dbPath)
        : db(dbPath), statsDb(prepareStatsDb(deriveStatsDbPath(dbPath)), true) {
        // entity_ids must exist before the repos are constructed.
        createEntityIdsTable(db);

        initializeRepos();
        applyMigrations();
    }

    bool DatabaseManager::entityIdsColumnExists(Database& db, const char* columnName) {
        auto pragma = db.query("PRAGMA table_info(entity_ids)");
        if (!pragma) return false;
        while (pragma->executeStep()) {
            if (pragma->getColumn(1).getText() == std::string(columnName)) return true;
        }
        return false;
    }

    void DatabaseManager::createEntityIdsTable(Database& db) {
        // Single row, one column per entity type.
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS entity_ids (
                profile_last_id INTEGER NOT NULL DEFAULT 0,
                group_last_id INTEGER NOT NULL DEFAULT 0,
                route_profile_last_id INTEGER NOT NULL DEFAULT 0,
                otp_profile_last_id INTEGER NOT NULL DEFAULT 0
            )
        )");

        // CREATE IF NOT EXISTS skips existing databases, so each added counter needs its own ALTER.
        if (!entityIdsColumnExists(db, "otp_profile_last_id"))
            db.exec("ALTER TABLE entity_ids ADD COLUMN otp_profile_last_id INTEGER NOT NULL DEFAULT 0");

        auto checkQuery = db.query("SELECT COUNT(*) FROM entity_ids");
        int count = 0;
        if (checkQuery && checkQuery->executeStep()) {
            count = checkQuery->getColumn(0).getInt();
        }

        if (count == 0) {
            db.exec(R"(
                INSERT INTO entity_ids (profile_last_id, group_last_id, route_profile_last_id)
                VALUES (0, 0, 0)
            )");
        }
    }

    void DatabaseManager::RunDeferredMaintenance() {
        runOnNewThread([this] {
            QThread::msleep(MAINTENANCE_DELAY_MS);
            db.RunMaintenance();
            statsDb.RunMaintenance();
        });
    }

    void DatabaseManager::initializeRepos() {
        profilesRepo = std::make_unique<ProfilesRepo>(db);
        groupsRepo = std::make_unique<GroupsRepo>(db);
        routesRepo = std::make_unique<RoutesRepo>(db);
        otpProfilesRepo = std::make_unique<OtpProfilesRepo>(db);
        settingsRepo = std::make_unique<SettingsRepo>(db);
        trafficStatsRepo = std::make_unique<TrafficStatsRepo>(statsDb);
        markersRepo = std::make_unique<MarkersRepo>(db);
    }

    void DatabaseManager::applyMigrations() {
        if (!markersRepo->IsMarked(Markers::TunPrivateRangesIPv6)) {
            auto &ranges = settingsRepo->vpn_private_ranges;
            // An emptied list opts out of the bypass entirely.
            if (!ranges.isEmpty()) {
                const auto before = ranges.size();
                for (const auto &range : {"fc00::/7", "fe80::/10", "ff00::/8"}) {
                    const auto subnet = QHostAddress::parseSubnet(range);
                    if (std::none_of(ranges.cbegin(), ranges.cend(),
                                     [&subnet](const QString &it) { return QHostAddress::parseSubnet(it) == subnet; }))
                        ranges << range;
                }
                if (ranges.size() != before) settingsRepo->Save();
            }
            markersRepo->Mark(Markers::TunPrivateRangesIPv6);
        }
    }
}
