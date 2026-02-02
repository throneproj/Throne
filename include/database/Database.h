#pragma once

#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include <type_traits>

namespace Configs {
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
    // Max bound parameters per statement (SQLite default SQLITE_MAX_VARIABLE_NUMBER is 999).
    constexpr int BATCH_LIMIT = 1000;

    class Database {
        SQLite::Database db;

        void execDeleteByIdInChunk(const std::string& table, const std::string& idColumn, const std::vector<int>& ids);
        void execBatchSettingsReplaceChunk(const std::vector<std::pair<std::string, std::string>>& keyValues);
        void execBatchInsertIntPairsChunk(const std::string& table, const std::string& colA, const std::string& colB,
                                         const std::vector<int>& pairs);
        void execBatchInsertProfilesChunk(const std::vector<ProfileInsertRow>& rows);
    public:
        Database(const std::string& path)
            : db(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
            // Enable foreign key support
            db.exec("PRAGMA foreign_keys = ON");
        }

        // 1. Bind one argument; explicit overloads avoid ambiguity on Linux (int32_t/int64_t/uint32_t)
        template<typename T>
        std::enable_if_t<std::is_integral_v<std::decay_t<T>>> bindOne(SQLite::Statement& query, int index, T&& value) {
            query.bind(index, static_cast<int64_t>(value));
        }
        template<typename T>
        std::enable_if_t<std::is_floating_point_v<std::decay_t<T>>> bindOne(SQLite::Statement& query, int index, T&& value) {
            query.bind(index, static_cast<double>(value));
        }
        void bindOne(SQLite::Statement& query, int index, const std::string& value) {
            query.bind(index, value);
        }
        void bindOne(SQLite::Statement& query, int index, const char* value) {
            query.bind(index, value);
        }

        template<typename T, typename... Rest>
        void bindArgs(SQLite::Statement& query, int index, T&& first, Rest&&... rest) {
            bindOne(query, index, std::forward<T>(first));
            bindArgs(query, index + 1, std::forward<Rest>(rest)...);
        }

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

        // 4. Execute DELETE FROM table WHERE idColumn IN (ids), chunked by BATCH_LIMIT
        void execDeleteByIdIn(const std::string& table, const std::string& idColumn, const std::vector<int>& ids) {
            for (size_t off = 0; off < ids.size(); off += BATCH_LIMIT) {
                size_t end = std::min(off + BATCH_LIMIT, ids.size());
                std::vector<int> chunk(ids.begin() + static_cast<std::ptrdiff_t>(off), ids.begin() + static_cast<std::ptrdiff_t>(end));
                execDeleteByIdInChunk(table, idColumn, chunk);
            }
        }

        // 5. Execute INSERT OR REPLACE INTO settings (key, value) VALUES ..., chunked (2 params per row -> BATCH_LIMIT/2 rows per chunk)
        void execBatchSettingsReplace(const std::vector<std::pair<std::string, std::string>>& keyValues) {
            const size_t chunkSize = BATCH_LIMIT / 2;
            for (size_t off = 0; off < keyValues.size(); off += chunkSize) {
                size_t end = std::min(off + chunkSize, keyValues.size());
                std::vector<std::pair<std::string, std::string>> chunk(keyValues.begin() + static_cast<std::ptrdiff_t>(off),
                                                                       keyValues.begin() + static_cast<std::ptrdiff_t>(end));
                execBatchSettingsReplaceChunk(chunk);
            }
        }

        // 6. Execute INSERT INTO table (colA, colB) VALUES ..., chunked (2 params per pair -> BATCH_LIMIT/2 pairs per chunk)
        void execBatchInsertIntPairs(const std::string& table, const std::string& colA, const std::string& colB,
                                     const std::vector<int>& pairs) {
            if (pairs.size() < 2 || pairs.size() % 2 != 0) return;
            const size_t chunkPairs = BATCH_LIMIT / 2;
            for (size_t off = 0; off < pairs.size() / 2; off += chunkPairs) {
                size_t pairCount = std::min(chunkPairs, pairs.size() / 2 - off);
                std::vector<int> chunk;
                chunk.reserve(pairCount * 2);
                for (size_t i = 0; i < pairCount; ++i) {
                    size_t idx = (off + i) * 2;
                    chunk.push_back(pairs[idx]);
                    chunk.push_back(pairs[idx + 1]);
                }
                execBatchInsertIntPairsChunk(table, colA, colB, chunk);
            }
        }

        // Chunked (11 params per row -> BATCH_LIMIT/11 rows per chunk)
        void execBatchInsertProfiles(const std::vector<ProfileInsertRow>& rows) {
            const size_t chunkSize = BATCH_LIMIT / 11;
            for (size_t off = 0; off < rows.size(); off += chunkSize) {
                size_t end = std::min(off + chunkSize, rows.size());
                std::vector<ProfileInsertRow> chunk(rows.begin() + static_cast<std::ptrdiff_t>(off),
                                                    rows.begin() + static_cast<std::ptrdiff_t>(end));
                execBatchInsertProfilesChunk(chunk);
            }
        }
    };
}