#pragma once

#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <atomic>
#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include <type_traits>

#include "include/global/Utils.hpp"

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
        std::string ip_out;
        std::string outbound_json;
        long long traffic_dl = 0;
        long long traffic_up = 0;
    };
    // Max bound parameters per statement (SQLite default SQLITE_MAX_VARIABLE_NUMBER is 999).
    constexpr int BATCH_LIMIT_WRITE = 1500;
    constexpr int BATCH_LIMIT_READ = 4096;
    // Run WAL checkpoint after this many write operations (exec or batch chunk).
    constexpr int WAL_CHECKPOINT_AFTER_WRITES = 10000;

    inline void NotifyError(const std::string& query, std::exception& e) {
        runOnUiThread([=] {
            std::string shortQ;
            if (query.length() > 200) shortQ = query.substr(0, 200);
            else shortQ = query;
            MessageBoxWarning("DB error occurred", "Failed for " + QString::fromStdString(shortQ) + " with exception: " + e.what() + "\n your database may be corrupted");
        });
    }

    class Database {
        SQLite::Database db;
        std::atomic<int> writeCount{0};
        void maybeCheckpoint(int count);

        void execDeleteByIdInChunk(const std::string& table, const std::string& idColumn, const std::vector<int>& ids);
        void execBatchSettingsReplaceChunk(const std::vector<std::pair<std::string, std::string>>& keyValues);
        void execBatchInsertIntPairsChunk(const std::string& table, const std::string& colA, const std::string& colB,
                                         const std::vector<int>& pairs);
        void execBatchInsertProfilesChunk(const std::vector<ProfileInsertRow>& rows);
        void execBatchReplaceProfilesChunk(const std::vector<ProfileInsertRow>& rows);
    public:
        Database(const std::string& path)
            : db(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE) {
            db.exec("PRAGMA foreign_keys = ON");
            db.exec("PRAGMA journal_mode = WAL");
            db.exec("PRAGMA synchronous = NORMAL");
            db.exec("PRAGMA mmap_size = 67108864"); // 64MB
            checkpointWal();
        }

    private:

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
        void exec0(const std::string& sql, Args&&... args) {
            SQLite::Statement query(db, sql);
            bindArgs(query, 1, std::forward<Args>(args)...);
            query.exec();
            maybeCheckpoint(1);
        }

        // Run WAL checkpoint manually (e.g. from a periodic timer). Safe to call from any thread.
        void checkpointWal();

        // 3. Helper for fetching a single row
        // Returns a Statement you can extract data from
        template<typename... Args>
        std::unique_ptr<SQLite::Statement> query0(const std::string& sql, Args&&... args) {
            auto query = std::make_unique<SQLite::Statement>(db, sql);
            bindArgs(*query, 1, std::forward<Args>(args)...);
            return query;
        }

        // 4. Execute DELETE FROM table WHERE idColumn IN (ids), chunked by BATCH_LIMIT
        void execDeleteByIdIn0(const std::string& table, const std::string& idColumn, const std::vector<int>& ids) {
            for (size_t off = 0; off < ids.size(); off += BATCH_LIMIT_WRITE) {
                size_t end = std::min(off + BATCH_LIMIT_WRITE, ids.size());
                std::vector<int> chunk(ids.begin() + static_cast<std::ptrdiff_t>(off), ids.begin() + static_cast<std::ptrdiff_t>(end));
                execDeleteByIdInChunk(table, idColumn, chunk);
            }
        }

        // 5. Execute INSERT OR REPLACE INTO settings (key, value) VALUES ..., chunked (2 params per row -> BATCH_LIMIT/2 rows per chunk)
        void execBatchSettingsReplace0(const std::vector<std::pair<std::string, std::string>>& keyValues) {
            const size_t chunkSize = BATCH_LIMIT_WRITE / 2;
            for (size_t off = 0; off < keyValues.size(); off += chunkSize) {
                size_t end = std::min(off + chunkSize, keyValues.size());
                std::vector<std::pair<std::string, std::string>> chunk(keyValues.begin() + static_cast<std::ptrdiff_t>(off),
                                                                       keyValues.begin() + static_cast<std::ptrdiff_t>(end));
                execBatchSettingsReplaceChunk(chunk);
            }
        }

        // 6. Execute INSERT INTO table (colA, colB) VALUES ..., chunked (2 params per pair -> BATCH_LIMIT/2 pairs per chunk)
        void execBatchInsertIntPairs0(const std::string& table, const std::string& colA, const std::string& colB,
                                     const std::vector<int>& pairs) {
            if (pairs.size() < 2 || pairs.size() % 2 != 0) return;
            const size_t chunkPairs = BATCH_LIMIT_WRITE / 2;
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

        // Chunked (12 params per row -> BATCH_LIMIT/12 rows per chunk)
        void execBatchInsertProfiles0(const std::vector<ProfileInsertRow>& rows) {
            const size_t chunkSize = BATCH_LIMIT_WRITE / 12;
            for (size_t off = 0; off < rows.size(); off += chunkSize) {
                size_t end = std::min(off + chunkSize, rows.size());
                std::vector<ProfileInsertRow> chunk(rows.begin() + static_cast<std::ptrdiff_t>(off),
                                                    rows.begin() + static_cast<std::ptrdiff_t>(end));
                execBatchInsertProfilesChunk(chunk);
            }
        }

        // Same chunking as execBatchInsertProfiles; INSERT OR REPLACE for batch save/update
        void execBatchReplaceProfiles0(const std::vector<ProfileInsertRow>& rows) {
            const size_t chunkSize = BATCH_LIMIT_WRITE / 12;
            for (size_t off = 0; off < rows.size(); off += chunkSize) {
                size_t end = std::min(off + chunkSize, rows.size());
                std::vector<ProfileInsertRow> chunk(rows.begin() + static_cast<std::ptrdiff_t>(off),
                                                    rows.begin() + static_cast<std::ptrdiff_t>(end));
                execBatchReplaceProfilesChunk(chunk);
            }
        }

    public:
        template<typename... Args>
        void exec(const std::string& sql, Args&&... args) {
            try {
                exec0(sql, std::forward<Args>(args)...);
            } catch (std::exception& e) {
                NotifyError(sql, e);
            }
        }

        template<typename... Args>
        std::unique_ptr<SQLite::Statement> query(const std::string& sql, Args&&... args) {
            try {
                auto query = query0(sql, std::forward<Args>(args)...);
                return query;
            } catch (std::exception& e) {
                NotifyError(sql, e);
                return nullptr;
            }
        }

        void execDeleteByIdIn(const std::string& table, const std::string& idColumn, const std::vector<int>& ids) {
            try {
                execDeleteByIdIn0(table, idColumn, ids);
            } catch (std::exception& e) {
                NotifyError("execDeleteByIdIn for " + table, e);
            }
        }

        void execBatchSettingsReplace(const std::vector<std::pair<std::string, std::string>>& keyValues) {
            try {
                execBatchSettingsReplace0(keyValues);
            } catch (std::exception& e) {
                NotifyError("execBatchSettingsReplace", e);
            }
        }

        void execBatchInsertIntPairs(const std::string& table, const std::string& colA, const std::string& colB,
                                     const std::vector<int>& pairs) {
            try {
                execBatchInsertIntPairs0(table, colA, colB, pairs);
            } catch (std::exception& e) {
                NotifyError("execBatchInsertIntPairs for " + table, e);
            }
        }

        void execBatchInsertProfiles(const std::vector<ProfileInsertRow>& rows) {
            try {
                execBatchInsertProfiles0(rows);
            } catch (std::exception& e) {
                NotifyError("execBatchInsertProfiles", e);
            }
        }

        void execBatchReplaceProfiles(const std::vector<ProfileInsertRow>& rows) {
            try {
                execBatchReplaceProfiles0(rows);
            } catch (std::exception& e) {
                NotifyError("execBatchReplaceProfiles", e);
            }
        }
    };
}
