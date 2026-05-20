#include "include/database/Database.h"
#include <3rdparty/SQLiteCpp/include/Backup.h>
#include <set>

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

    namespace {
        // Tables that make up each logical category. Delete order matters when
        // foreign keys are on; we copy with them off, but keep child-first order
        // for clarity and so re-enabling FK checks afterwards stays consistent.
        const std::vector<std::string> kProfileTables = {"profiles", "groups_order", "groups"};
        const std::vector<std::string> kRouteTables = {"route_rules", "route_profiles"};
        const std::vector<std::string> kSettingsTables = {"settings"};

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

        // Replace every row of main.<table> with the rows from bak.<table>,
        // copying only the columns that exist in both schemas.
        void copyTable(SQLite::Database& d, const std::string& table) {
            if (!tableExists(d, "main", table) || !tableExists(d, "bak", table)) return;

            const auto mainCols = tableColumns(d, "main", table);
            const auto bakColsVec = tableColumns(d, "bak", table);
            const std::set<std::string> bakCols(bakColsVec.begin(), bakColsVec.end());

            std::string colList;
            for (const auto& c : mainCols) {
                if (bakCols.count(c) == 0) continue;
                if (!colList.empty()) colList += ",";
                colList += "\"" + c + "\"";
            }
            if (colList.empty()) return;

            d.exec("DELETE FROM main." + table);
            d.exec("INSERT INTO main." + table + " (" + colList + ") SELECT " + colList + " FROM bak." + table);
        }
    }

    void Database::backupSelective(const std::string& destPath, const BackupParts& parts) {
        // Take a full, WAL-safe snapshot first (same mechanism as backupTo),
        // then strip the categories the user did not select. entity_ids is
        // always kept so profile/route IDs stay consistent on restore.
        {
            SQLite::Database destDb(destPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            SQLite::Backup backup(destDb, db);
            backup.executeStep(-1);
        }

        SQLite::Database dest(destPath, SQLite::OPEN_READWRITE);
        auto wipe = [&](const std::vector<std::string>& tables) {
            for (const auto& t : tables) {
                try {
                    if (tableExists(dest, "main", t)) dest.exec("DELETE FROM " + t);
                } catch (...) {}
            }
        };
        if (!parts.profiles) wipe(kProfileTables);
        if (!parts.routes) wipe(kRouteTables);
        if (!parts.settings) wipe(kSettingsTables);
        try { dest.exec("VACUUM"); } catch (...) {}
    }

    void Database::restoreSelective(const std::string& srcPath, const BackupParts& parts) {
        if (!parts.anyDb()) return;

        {
            SQLite::Statement attach(db, "ATTACH DATABASE ? AS bak");
            attach.bind(1, srcPath);
            attach.exec();
        }

        try {
            // foreign_keys must be toggled outside a transaction to take effect.
            db.exec("PRAGMA foreign_keys = OFF");
            db.exec("BEGIN IMMEDIATE");

            if (parts.profiles) for (const auto& t : kProfileTables) copyTable(db, t);
            if (parts.routes) for (const auto& t : kRouteTables) copyTable(db, t);
            if (parts.settings) for (const auto& t : kSettingsTables) copyTable(db, t);

            // Keep the ID counters ahead of any restored data so freshly created
            // profiles/groups/routes never collide with restored ones.
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

            db.exec("COMMIT");
        } catch (...) {
            try { db.exec("ROLLBACK"); } catch (...) {}
            try { db.exec("PRAGMA foreign_keys = ON"); } catch (...) {}
            try { db.exec("DETACH DATABASE bak"); } catch (...) {}
            throw;
        }

        db.exec("PRAGMA foreign_keys = ON");
        db.exec("DETACH DATABASE bak");
        checkpointWal();
    }
}
