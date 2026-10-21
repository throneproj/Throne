#include "include/database/Database.h"
#include "include/global/Const.hpp"
#include "include/global/Logger.hpp"
#include "include/global/Notice.hpp"
#include <3rdparty/SQLiteCpp/include/Backup.h>
#include <3rdparty/SQLiteCpp/include/sqlite3.h>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <unordered_map>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <unistd.h>
#endif

namespace Configs {
    namespace {
        constexpr int DB_ERROR_LOG_FIRST = 3;
        constexpr qint64 DB_ERROR_LOG_EVERY_MS = 60 * 1000;
        constexpr qint64 DB_ERROR_NOTICE_EVERY_MS = 10 * 60 * 1000;

        struct DbErrorTally {
            int count = 0;
            int suppressed = 0;
            qint64 lastLogMs = 0;
            qint64 lastNoticeMs = 0;
        };

        std::mutex g_dbErrorMu;
        std::unordered_map<std::string, DbErrorTally> g_dbErrorTallies;

        constexpr auto DB_SNAPSHOT_BUSY_DEADLINE = std::chrono::seconds(10);

        // The step answers BUSY at once, skipping the busy handler, while the source connection is inside a write transaction.
        void databaseSnapshotStep(SQLite::Backup& backup, std::recursive_mutex& writeMutex) {
            const auto deadline = std::chrono::steady_clock::now() + DB_SNAPSHOT_BUSY_DEADLINE;
            for (int attempt = 0;; ++attempt) {
                int rc;
                {
                    std::lock_guard lock(writeMutex);
                    rc = backup.executeStep(-1);
                }
                if (rc == SQLITE_DONE) return;
                if ((rc == SQLITE_BUSY || rc == SQLITE_LOCKED) && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(std::min(20 + 10 * attempt, 50)));
                    continue;
                }
                throw SQLite::Exception(sqlite3_errstr(rc), rc);
            }
        }
    }

    DbError DescribeDbError(const std::exception& e) {
        DbError err;
        err.what = e.what();
        if (const auto* s = dynamic_cast<const SQLite::Exception*>(&e)) {
            err.code = s->getErrorCode();
            err.extended = s->getExtendedErrorCode();
        }
        return err;
    }

    bool IsFatalDbError(const DbError& err) {
        const int code = err.extended > 0 ? err.extended : err.code;
        if (code < 0) return false;
        const int primary = code & 0xff;
        return primary == SQLITE_READONLY || primary == SQLITE_CORRUPT || primary == SQLITE_NOTADB;
    }

    // SQLite reports a file this user may neither read nor write as CANTOPEN; other causes (EMFILE, a late mount) can pass.
    bool DbFilesDenied(const QString& dbFile) {
        if (!QFileInfo::exists(dbFile)) return false;
        for (const QString& path : {dbFile, dbFile + QStringLiteral("-wal"), dbFile + QStringLiteral("-shm")}) {
            if (!QFileInfo::exists(path)) continue;
#ifdef Q_OS_UNIX
            if (::access(QFile::encodeName(path).constData(), R_OK | W_OK) != 0 && (errno == EACCES || errno == EPERM)) return true;
#else
#if defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
            // Without it Qt reports only the read-only attribute, never the ACL.
            const QNtfsPermissionCheckGuard ntfsCheck;
#endif
            const QFileInfo info(path);
            if (!info.isReadable() || !info.isWritable()) return true;
#endif
        }
        return false;
    }

    std::string DbRebuildMarkerPath(const std::string& dbPath) {
        return dbPath + ".rebuild";
    }

    void NotifyError(const std::string& query, const DbError& err) {
        const std::string key = query.length() > 200 ? query.substr(0, 200) : query;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool logIt = false;
        bool noticeIt = false;
        int count = 0;
        int suppressed = 0;
        {
            std::lock_guard<std::mutex> lk(g_dbErrorMu);
            auto& t = g_dbErrorTallies[key];
            count = ++t.count;
            if (t.count <= DB_ERROR_LOG_FIRST || now - t.lastLogMs >= DB_ERROR_LOG_EVERY_MS) {
                logIt = true;
                suppressed = t.suppressed;
                t.suppressed = 0;
                t.lastLogMs = now;
            } else {
                ++t.suppressed;
            }
            if (t.lastNoticeMs == 0 || now - t.lastNoticeMs >= DB_ERROR_NOTICE_EVERY_MS) {
                noticeIt = true;
                t.lastNoticeMs = now;
            }
        }

        QString detail = QString::fromStdString(err.what);
        if (err.code >= 0) detail += QString(" [sqlite %1/%2]").arg(err.code).arg(err.extended);
        if (logIt) {
            QString line = QString("DB error in %1: %2 (occurrence %3").arg(QString::fromStdString(key), detail).arg(count);
            if (suppressed > 0) line += QString(", %1 suppressed").arg(suppressed);
            line += ")";
            LOG_ERROR(line);
        }
        if (noticeIt) {
            PostNotice({NoticeLevel::Error, QObject::tr("Database error"),
                        QObject::tr("%1 failed: %2. Details are in the log file.").arg(QString::fromStdString(key), detail)});
        }
    }

    void Database::maybeCheckpoint(int count) {
        if (writeCount.fetch_add(count) >= WAL_CHECKPOINT_AFTER_WRITES) {
            writeCount = 0;
            checkpointWal();
        }
    }

    void Database::checkpointWal() {
        try {
            db.exec("PRAGMA wal_checkpoint(TRUNCATE)");
        } catch (std::exception& e) {
            LOG_WARN(QString("DB WAL checkpoint skipped: ") + e.what());
        }
    }

    void Database::RunMaintenance() {
        checkpointWal();
        maybeVacuum();
    }

    void Database::maybeVacuum() {
        try {
            const long long freePages = db.execAndGet("PRAGMA freelist_count").getInt64();
            const long long pageCount = db.execAndGet("PRAGMA page_count").getInt64();
            const long long pageSize  = db.execAndGet("PRAGMA page_size").getInt64();
            if (pageCount <= 0 || pageSize <= 0 || freePages <= 0) return;

            const long long freeBytes = freePages * pageSize;
            const double freeRatio = static_cast<double>(freePages) / static_cast<double>(pageCount);
            if (freeBytes < VACUUM_MIN_FREE_BYTES || freeRatio < VACUUM_MIN_FREE_RATIO) return;

            if (db.execAndGet("PRAGMA auto_vacuum").getInt64() == 2) {
                db.exec("PRAGMA incremental_vacuum(" + std::to_string(INCREMENTAL_VACUUM_PAGES) + ")");
            } else {
                db.exec("VACUUM");
            }
            // In WAL mode the on-disk shrink only lands once the WAL is checkpointed.
            checkpointWal();
        } catch (std::exception& e) {
            // A concurrent transaction on this connection fails VACUUM; next launch retries.
            LOG_WARN(QString("DB VACUUM check skipped: ") + e.what());
        }
    }

    bool Database::transaction(const std::string& op, const std::function<bool()>& body) {
        std::lock_guard lock(writeMutex_);
        try {
            db.exec("BEGIN IMMEDIATE");
        } catch (std::exception& e) {
            NotifyError(op, e);
            return false;
        }
        try {
            if (!body()) {
                db.exec("ROLLBACK");
                return false;
            }
            db.exec("COMMIT");
            return true;
        } catch (std::exception& e) {
            try { db.exec("ROLLBACK"); } catch (...) {}
            NotifyError(op, e);
            return false;
        } catch (...) {
            try { db.exec("ROLLBACK"); } catch (...) {}
            throw;
        }
    }

    void Database::execDeleteByIdInChunk(const std::string& table, const std::string& idColumn, const std::vector<int>& ids) {
        if (ids.empty()) return;
        std::string sql = "DELETE FROM " + table + " WHERE " + idColumn + " IN (";
        for (size_t i = 0; i < ids.size(); ++i) {
            if (i > 0) sql += ",";
            sql += std::to_string(ids[i]);
        }
        sql += ")";
        try {
            db.exec(sql);
            maybeCheckpoint(ids.size());
        } catch (std::exception& e) {
            NotifyError("execDeleteByIdIn for " + table, e);
        }
    }

    void Database::execDeleteByIdInThrow(const std::string& table, const std::string& idColumn, const std::vector<int>& ids) {
        for (size_t off = 0; off < ids.size(); off += BATCH_LIMIT_WRITE) {
            const size_t end = std::min(off + BATCH_LIMIT_WRITE, ids.size());
            std::string sql = "DELETE FROM " + table + " WHERE " + idColumn + " IN (";
            for (size_t i = off; i < end; ++i) {
                if (i > off) sql += ",";
                sql += std::to_string(ids[i]);
            }
            sql += ")";
            db.exec(sql);
            maybeCheckpoint(static_cast<int>(end - off));
        }
    }

    void Database::execBatchSettingsReplaceChunk(const std::vector<std::pair<std::string, std::string>>& keyValues) {
        if (keyValues.empty()) return;
        std::string sql = "INSERT OR REPLACE INTO settings (key, value) VALUES ";
        for (size_t i = 0; i < keyValues.size(); ++i) {
            if (i > 0) sql += ",";
            sql += "(?,?)";
        }
        try {
            SQLite::Statement stmt(db, sql);
            for (size_t i = 0; i < keyValues.size(); ++i) {
                stmt.bind(static_cast<int>(2 * i + 1), keyValues[i].first);
                stmt.bind(static_cast<int>(2 * i + 2), keyValues[i].second);
            }
            stmt.exec();
            maybeCheckpoint(1);
        } catch (std::exception& e) {
            NotifyError("execBatchSettingsReplace", e);
        }
    }

    void Database::execBatchInsertIntPairsChunk(const std::string& table, const std::string& colA, const std::string& colB,
                                                 const std::vector<int>& pairs) {
        if (pairs.size() < 2 || pairs.size() % 2 != 0) return;
        std::string sql = "INSERT INTO " + table + " (" + colA + "," + colB + ") VALUES ";
        const size_t n = pairs.size() / 2;
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) sql += ",";
            sql += "(?,?)";
        }
        try {
            SQLite::Statement stmt(db, sql);
            for (size_t i = 0; i < pairs.size(); ++i) {
                stmt.bind(static_cast<int>(i + 1), pairs[i]);
            }
            stmt.exec();
            maybeCheckpoint(pairs.size() / 2);
        } catch (std::exception& e) {
            NotifyError("execBatchInsertIntPairs for " + table, e);
        }
    }

    void Database::execBatchInsertProfilesChunk(const std::vector<ProfileInsertRow>& rows) {
        if (rows.empty()) return;
        const size_t n = rows.size();
        std::string sql = "INSERT INTO profiles (id, type, name, gid, latency, latency_at, dl_speed, ul_speed, test_country, ip_out, outbound_json, traffic_dl, traffic_up, endpoint_json) VALUES ";
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) sql += ",";
            sql += "(?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
        }
        try {
            SQLite::Statement stmt(db, sql);
            int idx = 1;
            for (const auto& r : rows) {
                stmt.bind(idx++, r.id);
                stmt.bind(idx++, r.type);
                stmt.bind(idx++, r.name);
                stmt.bind(idx++, r.gid);
                stmt.bind(idx++, r.latency);
                stmt.bind(idx++, static_cast<int64_t>(r.latency_at));
                stmt.bind(idx++, r.dl_speed);
                stmt.bind(idx++, r.ul_speed);
                stmt.bind(idx++, r.test_country);
                stmt.bind(idx++, r.ip_out);
                stmt.bind(idx++, r.outbound_json);
                stmt.bind(idx++, static_cast<int64_t>(r.traffic_dl));
                stmt.bind(idx++, static_cast<int64_t>(r.traffic_up));
                stmt.bind(idx++, r.endpoint_json);
            }
            stmt.exec();
            maybeCheckpoint(static_cast<int>(rows.size()));
        } catch (std::exception& e) {
            NotifyError("execBatchInsertProfiles", e);
        }
    }

    void Database::execBatchReplaceProfilesChunk(const std::vector<ProfileInsertRow>& rows) {
        if (rows.empty()) return;
        const size_t n = rows.size();
        std::string sql = "INSERT OR REPLACE INTO profiles (id, type, name, gid, latency, latency_at, dl_speed, ul_speed, test_country, ip_out, outbound_json, traffic_dl, traffic_up, endpoint_json) VALUES ";
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) sql += ",";
            sql += "(?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
        }
        try {
            SQLite::Statement stmt(db, sql);
            int idx = 1;
            for (const auto& r : rows) {
                stmt.bind(idx++, r.id);
                stmt.bind(idx++, r.type);
                stmt.bind(idx++, r.name);
                stmt.bind(idx++, r.gid);
                stmt.bind(idx++, r.latency);
                stmt.bind(idx++, static_cast<int64_t>(r.latency_at));
                stmt.bind(idx++, r.dl_speed);
                stmt.bind(idx++, r.ul_speed);
                stmt.bind(idx++, r.test_country);
                stmt.bind(idx++, r.ip_out);
                stmt.bind(idx++, r.outbound_json);
                stmt.bind(idx++, static_cast<int64_t>(r.traffic_dl));
                stmt.bind(idx++, static_cast<int64_t>(r.traffic_up));
                stmt.bind(idx++, r.endpoint_json);
            }
            stmt.exec();
            maybeCheckpoint(static_cast<int>(rows.size()));
        } catch (std::exception& e) {
            NotifyError("execBatchReplaceProfiles", e);
        }
    }

    void Database::backupTo(const std::string& destPath) {
        SQLite::Database destDb(destPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        SQLite::Backup backup(destDb, db);
        databaseSnapshotStep(backup, writeMutex_);
    }

    void Database::restoreFrom(const std::string& srcPath) {
        SQLite::Database srcDb(srcPath, SQLite::OPEN_READONLY);
        SQLite::Backup restore(db, srcDb);
        restore.executeStep(-1);
    }

    namespace {
        // Child-first order: delete order matters once foreign keys are re-enabled.
        const std::vector<std::string> kProfileTables = {"profiles", "groups_order", "groups"};
        const std::vector<std::string> kRouteTables = {"route_rules", "route_profiles"};
        const std::vector<std::string> kSettingsTables = {"settings", "markers"};
        const std::vector<std::string> kOtpTables = {"otp_profiles"};
        const std::vector<std::string> kIpListTables = {"ip_list_entries", "ip_scans", "ip_lists"};

        std::vector<std::string> tableColumns(SQLite::Database& d, const std::string& schema, const std::string& table) {
            std::vector<std::string> cols;
            // schema/table are internal constants, not user input -> safe to inline.
            SQLite::Statement q(d, "PRAGMA " + schema + ".table_info(" + table + ")");
            while (q.executeStep()) cols.emplace_back(q.getColumn(1).getText());
            return cols;
        }

        bool tableExists(SQLite::Database& d, const std::string& schema, const std::string& table) {
            SQLite::Statement q(d, "SELECT 1 FROM " + schema + ".sqlite_master WHERE type='table' AND name=?");
            q.bind(1, table);
            return q.executeStep();
        }

        bool columnExists(SQLite::Database& d, const std::string& schema, const std::string& table,
                          const std::string& column) {
            if (!tableExists(d, schema, table)) return false;
            const auto cols = tableColumns(d, schema, table);
            return std::find(cols.begin(), cols.end(), column) != cols.end();
        }

        // Backup-only column names come from the file, so embedded quotes are doubled.
        std::string quoteIdent(const std::string& name) {
            std::string out = "\"";
            for (const char ch : name) {
                if (ch == '"') out += '"';
                out += ch;
            }
            return out + "\"";
        }

        // A route_rules row holding a value in a column this schema lacks (another app's condition) is skipped whole:
        // copied without it, the rule would match more. Returns the number of skipped rows.
        // Rows whose "key" column is one of keepKeys keep main's version on both sides of the copy.
        int copyTable(SQLite::Database& d, const std::string& table, const QStringList& keepKeys = {}) {
            if (!tableExists(d, "main", table) || !tableExists(d, "bak", table)) return 0;

            const auto mainCols = tableColumns(d, "main", table);
            const auto bakColsVec = tableColumns(d, "bak", table);
            const std::set<std::string> bakCols(bakColsVec.begin(), bakColsVec.end());
            const std::set<std::string> mainColSet(mainCols.begin(), mainCols.end());

            std::string keepRow;
            if (table == "route_rules") {
                for (const auto& c : bakColsVec) {
                    if (mainColSet.count(c) != 0) continue;
                    if (!keepRow.empty()) keepRow += " AND ";
                    keepRow += "(" + quoteIdent(c) + " IS NULL OR " + quoteIdent(c) + " IN ('', '[]', 0, '0'))";
                }
            }
            const int skipped = keepRow.empty() ? 0
                : d.execAndGet("SELECT COUNT(*) FROM bak." + table + " WHERE NOT (" + keepRow + ")").getInt();

            std::string colList;
            std::string selectList;
            for (const auto& c : mainCols) {
                if (bakCols.count(c) == 0) continue;
                if (!colList.empty()) {
                    colList += ",";
                    selectList += ",";
                }
                colList += quoteIdent(c);
                // RoutesRepo numbers each profile's rules 0..n-1; close the gaps the skipped rows leave.
                if (skipped > 0 && c == "rule_order")
                    selectList += "ROW_NUMBER() OVER (PARTITION BY \"route_profile_id\" ORDER BY \"rule_order\") - 1";
                else
                    selectList += quoteIdent(c);
            }
            if (colList.empty()) return 0;

            std::string keepFilter;
            for (qsizetype i = 0; i < keepKeys.size(); ++i) keepFilter += i == 0 ? "?" : ",?";
            if (!keepFilter.empty()) keepFilter = "\"key\" NOT IN (" + keepFilter + ")";
            const auto bindKeepKeys = [&keepKeys](SQLite::Statement& statement) {
                for (qsizetype i = 0; i < keepKeys.size(); ++i) statement.bind(static_cast<int>(i + 1), keepKeys[i].toStdString());
            };
            std::string where = keepRow;
            if (!keepFilter.empty()) where = where.empty() ? keepFilter : "(" + where + ") AND " + keepFilter;

            SQLite::Statement wipe(d, "DELETE FROM main." + table + (keepFilter.empty() ? "" : " WHERE " + keepFilter));
            bindKeepKeys(wipe);
            wipe.exec();
            SQLite::Statement copy(d, "INSERT INTO main." + table + " (" + colList + ") SELECT " + selectList + " FROM bak." +
                                      table + (where.empty() ? "" : " WHERE " + where));
            bindKeepKeys(copy);
            copy.exec();
            return skipped;
        }

        // The id a main.settings row holds, read as SettingsRepo reads it; nullopt when the row is missing.
        std::optional<int> databaseSettingId(SQLite::Database& d, const char* key) {
            SQLite::Statement q(d, "SELECT value FROM main.settings WHERE key = ?");
            q.bind(1, key);
            if (!q.executeStep()) return std::nullopt;
            return QString::fromStdString(q.getColumn(0).getText()).toInt();
        }

        void databaseSetSettingId(SQLite::Database& d, const char* key, int id) {
            SQLite::Statement q(d, "INSERT OR REPLACE INTO main.settings (key, value) VALUES (?, ?)");
            q.bind(1, key);
            q.bind(2, std::to_string(id));
            q.exec();
        }

        bool databaseIdExists(SQLite::Database& d, const std::string& table, int id) {
            SQLite::Statement q(d, "SELECT 1 FROM main." + table + " WHERE id = ?");
            q.bind(1, id);
            return q.executeStep();
        }

        std::optional<int> databaseFirstId(SQLite::Database& d, const std::string& sql) {
            SQLite::Statement q(d, sql);
            if (!q.executeStep() || q.getColumn(0).isNull()) return std::nullopt;
            return q.getColumn(0).getInt();
        }
    }

    void Database::backupSelective(const std::string& destPath, const BackupParts& parts, const QStringList& excludedSettingKeys) {
        // Full snapshot first (WAL-safe, unlike a file copy), then strip the unselected categories.
        {
            SQLite::Database destDb(destPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            SQLite::Backup backup(destDb, db);
            databaseSnapshotStep(backup, writeMutex_);
        }

        SQLite::Database dest(destPath, SQLite::OPEN_READWRITE);
        // Freed pages are zeroed, so a part left out is not shipped in free pages even when the VACUUM below fails.
        dest.exec("PRAGMA secure_delete = ON");
        // A failed wipe throws: the archive must never carry a part that was meant to be left out.
        auto wipe = [&](const std::vector<std::string>& tables) {
            for (const auto& t : tables) {
                if (tableExists(dest, "main", t)) dest.exec("DELETE FROM " + t);
            }
        };
        if (!parts.profiles) wipe(kProfileTables);
        if (!parts.routes) wipe(kRouteTables);
        if (!parts.settings) wipe(kSettingsTables);
        if (!parts.otp) wipe(kOtpTables);
        if (!parts.ipLists) wipe(kIpListTables);
        if (parts.settings && !excludedSettingKeys.isEmpty() && tableExists(dest, "main", "settings")) {
            std::string placeholders;
            for (qsizetype i = 0; i < excludedSettingKeys.size(); ++i) placeholders += i == 0 ? "?" : ",?";
            SQLite::Statement strip(dest, "DELETE FROM settings WHERE \"key\" IN (" + placeholders + ")");
            for (qsizetype i = 0; i < excludedSettingKeys.size(); ++i) strip.bind(static_cast<int>(i + 1), excludedSettingKeys[i].toStdString());
            strip.exec();
        }
        try { dest.exec("VACUUM"); } catch (...) {}
    }

    int Database::restoreSelective(const std::string& srcPath, const BackupParts& parts, const QStringList& preservedSettingKeys) {
        if (!parts.anyDb()) return 0;

        std::unique_lock writeLock(writeMutex_);
        {
            SQLite::Statement attach(db, "ATTACH DATABASE ? AS bak");
            attach.bind(1, srcPath);
            attach.exec();
        }

        int skippedRules = 0;
        bool begun = false;
        try {
            // foreign_keys must be toggled outside a transaction to take effect.
            db.exec("PRAGMA foreign_keys = OFF");
            db.exec("BEGIN IMMEDIATE");
            begun = true;

            // copyTable leaves a table alone when either side lacks it.
            const auto replaces = [&](bool part, const std::string& table) {
                return part && tableExists(db, "main", table) && tableExists(db, "bak", table);
            };
            const bool profilesReplaced = replaces(parts.profiles, "profiles");
            const bool groupsReplaced = replaces(parts.profiles, "groups");
            const bool routesReplaced = replaces(parts.routes, "route_profiles");
            const bool settingsReplaced = replaces(parts.settings, "settings");

            // An id whose table stays this host's keeps this host's value: the backup's would name an unrelated row.
            QStringList settingKeepKeys = preservedSettingKeys;
            const auto keepHostId = [&settingKeepKeys](bool tableReplaced, const QString& key) {
                if (!tableReplaced && !settingKeepKeys.contains(key)) settingKeepKeys << key;
            };
            keepHostId(profilesReplaced, QStringLiteral("remember_id"));
            keepHostId(groupsReplaced, QStringLiteral("current_group"));
            keepHostId(routesReplaced, QStringLiteral("current_route_id"));

            if (parts.profiles) for (const auto& t : kProfileTables) copyTable(db, t);
            if (parts.routes) for (const auto& t : kRouteTables) skippedRules += copyTable(db, t);
            if (parts.settings) {
                for (const auto& t : kSettingsTables) copyTable(db, t, t == "settings" ? settingKeepKeys : QStringList{});
                // Settings saved before the markers table existed have been through no migration yet.
                if (!tableExists(db, "bak", "markers") && tableExists(db, "main", "markers"))
                    db.exec("DELETE FROM main.markers");
            }
            if (parts.otp) for (const auto& t : kOtpTables) copyTable(db, t);
            if (parts.ipLists) for (const auto& t : kIpListTables) copyTable(db, t);

            // A settings id must resolve; one that did not come with its replaced table names an unrelated row.
            if (tableExists(db, "main", "settings")) {
                const auto keptHere = [&](const QString& key) {
                    return !settingsReplaced || settingKeepKeys.contains(key);
                };
                if (const auto rememberId = databaseSettingId(db, "remember_id"); rememberId && *rememberId >= 0) {
                    if ((profilesReplaced && keptHere("remember_id")) || !databaseIdExists(db, "profiles", *rememberId))
                        databaseSetSettingId(db, "remember_id", NoProfileId);
                }
                // Missing rows are written too: SettingsRepo would load hard defaults that need not resolve.
                const auto currentGroup = databaseSettingId(db, "current_group");
                const bool groupStale = groupsReplaced && keptHere("current_group");
                if (!currentGroup || groupStale || !databaseIdExists(db, "groups", *currentGroup)) {
                    auto first = databaseFirstId(db, "SELECT o.group_id FROM main.groups_order o "
                                                     "JOIN main.groups g ON g.id = o.group_id ORDER BY o.display_order LIMIT 1");
                    if (!first) first = databaseFirstId(db, "SELECT MIN(id) FROM main.groups");
                    if (first && first != currentGroup) databaseSetSettingId(db, "current_group", *first);
                }
                const auto currentRoute = databaseSettingId(db, "current_route_id");
                const bool routeStale = routesReplaced && keptHere("current_route_id");
                if (!currentRoute || routeStale || !databaseIdExists(db, "route_profiles", *currentRoute)) {
                    const auto lowest = databaseFirstId(db, "SELECT MIN(id) FROM main.route_profiles");
                    if (lowest && lowest != currentRoute) databaseSetSettingId(db, "current_route_id", *lowest);
                }
            }

            // Keep the ID counters ahead of restored data so newly created IDs never collide.
            if (parts.profiles || parts.routes) {
                const bool bakIds = tableExists(db, "bak", "entity_ids");
                db.exec(
                    "UPDATE entity_ids SET "
                    "profile_last_id = MAX(profile_last_id,"
                    "(SELECT COALESCE(MAX(id),0) FROM profiles)" +
                    std::string(bakIds ? ",(SELECT COALESCE(MAX(profile_last_id),0) FROM bak.entity_ids)" : "") + "),"
                    "group_last_id = MAX(group_last_id,"
                    "(SELECT COALESCE(MAX(id),0) FROM groups)" +
                    std::string(bakIds ? ",(SELECT COALESCE(MAX(group_last_id),0) FROM bak.entity_ids)" : "") + "),"
                    "route_profile_last_id = MAX(route_profile_last_id,"
                    "(SELECT COALESCE(MAX(id),0) FROM route_profiles)" +
                    std::string(bakIds ? ",(SELECT COALESCE(MAX(route_profile_last_id),0) FROM bak.entity_ids)" : "") + ")");
            }

            if (parts.otp) {
                const bool bakOtpIds = columnExists(db, "bak", "entity_ids", "otp_profile_last_id");
                db.exec(
                    "UPDATE entity_ids SET otp_profile_last_id = MAX(otp_profile_last_id,"
                    "(SELECT COALESCE(MAX(id),0) FROM otp_profiles)" +
                    std::string(bakOtpIds ? ",(SELECT COALESCE(MAX(otp_profile_last_id),0) FROM bak.entity_ids)" : "") + ")");
            }

            if (parts.ipLists) {
                const bool bakListIds = columnExists(db, "bak", "entity_ids", "ip_list_last_id");
                db.exec(
                    "UPDATE entity_ids SET ip_list_last_id = MAX(ip_list_last_id,"
                    "(SELECT COALESCE(MAX(id),0) FROM ip_lists)" +
                    std::string(bakListIds ? ",(SELECT COALESCE(MAX(ip_list_last_id),0) FROM bak.entity_ids)" : "") + ")");
                const bool bakScanIds = columnExists(db, "bak", "entity_ids", "ip_scan_last_id");
                db.exec(
                    "UPDATE entity_ids SET ip_scan_last_id = MAX(ip_scan_last_id,"
                    "(SELECT COALESCE(MAX(id),0) FROM ip_scans)" +
                    std::string(bakScanIds ? ",(SELECT COALESCE(MAX(ip_scan_last_id),0) FROM bak.entity_ids)" : "") + ")");
            }

            db.exec("COMMIT");
        } catch (...) {
            if (begun) {
                try { db.exec("ROLLBACK"); } catch (...) {}
            }
            try { db.exec("PRAGMA foreign_keys = ON"); } catch (...) {}
            try { db.exec("DETACH DATABASE bak"); } catch (...) {}
            throw;
        }

        db.exec("PRAGMA foreign_keys = ON");
        db.exec("DETACH DATABASE bak");
        writeLock.unlock();
        checkpointWal();
        return skippedRules;
    }
}
