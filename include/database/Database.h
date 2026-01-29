#pragma once

#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <string>
#include <iostream>
#include <vector>
#include <utility>

namespace Configs {
    class Database {
        SQLite::Database db;

    public:
        Database(const std::string& path)
            : db(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
            // Enable foreign key support
            db.exec("PRAGMA foreign_keys = ON");
        }

        // 1. Recursive template to bind arguments one by one
        template<typename T, typename... Rest>
        void bindArgs(SQLite::Statement& query, int index, T&& first, Rest&&... rest) {
            query.bind(index, std::forward<T>(first));
            bindArgs(query, index + 1, std::forward<Rest>(rest)...);
        }

        // Base case for recursion
        void bindArgs(SQLite::Statement& query, int index) {
            // No more args to bind
        }

        // 2. The "PGX Style" Exec (No return value, e.g., UPDATE/INSERT)
        template<typename... Args>
        void exec(const std::string& sql, Args&&... args) {
            try {
                SQLite::Statement query(db, sql);
                bindArgs(query, 1, std::forward<Args>(args)...);
                query.exec();
            } catch (std::exception& e) {
                std::cerr << "DB Error: " << e.what() << std::endl;
            }
        }

        // 3. Helper for fetching a single row
        // Returns a Statement you can extract data from
        template<typename... Args>
        std::unique_ptr<SQLite::Statement> query(const std::string& sql, Args&&... args) {
            auto query = std::make_unique<SQLite::Statement>(db, sql);
            bindArgs(*query, 1, std::forward<Args>(args)...);
            return query;
        }

        // 4. Execute DELETE FROM table WHERE idColumn IN (ids) - single statement, ids as literals (safe for ints)
        void execDeleteByIdIn(const std::string& table, const std::string& idColumn, const std::vector<int>& ids) {
            if (ids.empty()) return;
            std::string sql = "DELETE FROM " + table + " WHERE " + idColumn + " IN (";
            for (size_t i = 0; i < ids.size(); ++i) {
                if (i > 0) sql += ",";
                sql += std::to_string(ids[i]);
            }
            sql += ")";
            try {
                db.exec(sql);
            } catch (std::exception& e) {
                std::cerr << "DB Error: " << e.what() << std::endl;
            }
        }

        // 5. Execute INSERT OR REPLACE INTO settings (key, value) VALUES (?,?), (?,?), ... - single statement
        void execBatchSettingsReplace(const std::vector<std::pair<std::string, std::string>>& keyValues) {
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
            } catch (std::exception& e) {
                std::cerr << "DB Error: " << e.what() << std::endl;
            }
        }

        // 6. Execute INSERT INTO table (colA, colB) VALUES (?,?), (?,?), ... - single statement, bind from flat int vector
        void execBatchInsertIntPairs(const std::string& table, const std::string& colA, const std::string& colB,
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
            } catch (std::exception& e) {
                std::cerr << "DB Error: " << e.what() << std::endl;
            }
        }

        // 7. One row for batch insert into profiles table (id, type, name, gid, latency, dl_speed, ul_speed, test_country, full_test_report, outbound_json, traffic_json)
        struct ProfileInsertRow {
            int id;
            std::string type;
            std::string name;
            int gid;
            int latency;
            std::string dl_speed;
            std::string ul_speed;
            std::string test_country;
            std::string full_test_report;
            std::string outbound_json;
            std::string traffic_json;
        };

        void execBatchInsertProfiles(const std::vector<ProfileInsertRow>& rows) {
            if (rows.empty()) return;
            const size_t n = rows.size();
            const int cols = 11;
            std::string sql = "INSERT INTO profiles (id, type, name, gid, latency, dl_speed, ul_speed, test_country, full_test_report, outbound_json, traffic_json) VALUES ";
            for (size_t i = 0; i < n; ++i) {
                if (i > 0) sql += ",";
                sql += "(?,?,?,?,?,?,?,?,?,?,?)";
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
                    stmt.bind(idx++, r.full_test_report);
                    stmt.bind(idx++, r.outbound_json);
                    stmt.bind(idx++, r.traffic_json);
                }
                stmt.exec();
            } catch (std::exception& e) {
                std::cerr << "DB Error: " << e.what() << std::endl;
            }
        }
    };
}