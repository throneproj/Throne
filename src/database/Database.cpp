#include "include/database/Database.h"
#include <3rdparty/SQLiteCpp/include/Backup.h>

namespace Configs {
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
            std::cerr << "DB WAL checkpoint error: " << e.what() << std::endl;
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
            std::cerr << "DB Error: " << e.what() << std::endl;
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
            std::cerr << "DB Error: " << e.what() << std::endl;
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
            std::cerr << "DB Error: " << e.what() << std::endl;
        }
    }

    void Database::execBatchInsertProfilesChunk(const std::vector<ProfileInsertRow>& rows) {
        if (rows.empty()) return;
        const size_t n = rows.size();
        std::string sql = "INSERT INTO profiles (id, type, name, gid, latency, dl_speed, ul_speed, test_country, ip_out, outbound_json, traffic_dl, traffic_up) VALUES ";
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) sql += ",";
            sql += "(?,?,?,?,?,?,?,?,?,?,?,?)";
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
                stmt.bind(idx++, r.dl_speed);
                stmt.bind(idx++, r.ul_speed);
                stmt.bind(idx++, r.test_country);
                stmt.bind(idx++, r.ip_out);
                stmt.bind(idx++, r.outbound_json);
                stmt.bind(idx++, static_cast<int64_t>(r.traffic_dl));
                stmt.bind(idx++, static_cast<int64_t>(r.traffic_up));
            }
            stmt.exec();
            maybeCheckpoint(static_cast<int>(rows.size()));
        } catch (std::exception& e) {
            std::cerr << "DB Error: " << e.what() << std::endl;
        }
    }

    void Database::execBatchReplaceProfilesChunk(const std::vector<ProfileInsertRow>& rows) {
        if (rows.empty()) return;
        const size_t n = rows.size();
        std::string sql = "INSERT OR REPLACE INTO profiles (id, type, name, gid, latency, dl_speed, ul_speed, test_country, ip_out, outbound_json, traffic_dl, traffic_up) VALUES ";
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) sql += ",";
            sql += "(?,?,?,?,?,?,?,?,?,?,?,?)";
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
                stmt.bind(idx++, r.dl_speed);
                stmt.bind(idx++, r.ul_speed);
                stmt.bind(idx++, r.test_country);
                stmt.bind(idx++, r.ip_out);
                stmt.bind(idx++, r.outbound_json);
                stmt.bind(idx++, static_cast<int64_t>(r.traffic_dl));
                stmt.bind(idx++, static_cast<int64_t>(r.traffic_up));
            }
            stmt.exec();
            maybeCheckpoint(static_cast<int>(rows.size()));
        } catch (std::exception& e) {
            std::cerr << "DB Error: " << e.what() << std::endl;
        }
    }

    void Database::backupTo(const std::string& destPath) {
        SQLite::Database destDb(destPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        SQLite::Backup backup(destDb, db);
        backup.executeStep(-1);
    }

    void Database::restoreFrom(const std::string& srcPath) {
        SQLite::Database srcDb(srcPath, SQLite::OPEN_READONLY);
        SQLite::Backup restore(db, srcDb);
        restore.executeStep(-1);
    }
}
