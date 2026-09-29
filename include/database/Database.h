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
        long long latency_at = 0;
        std::string dl_speed;
        std::string ul_speed;
        std::string test_country;
        std::string ip_out;
        std::string outbound_json;
        long long traffic_dl = 0;
        long long traffic_up = 0;
    };
    // icons is the icons/ folder, handled by the UI layer rather than the database.
    struct BackupParts {
        bool profiles = false;
        bool routes = false;
        bool settings = false;
        bool otp = false;
        bool icons = false;

        [[nodiscard]] bool anyDb() const { return profiles || routes || settings || otp; }
        [[nodiscard]] bool any() const { return anyDb() || icons; }
    };

    // Max bound parameters per statement (SQLite default SQLITE_MAX_VARIABLE_NUMBER is 999).
    constexpr int BATCH_LIMIT_WRITE = 1500;
    constexpr int BATCH_LIMIT_READ = 4096;
    constexpr int WAL_CHECKPOINT_AFTER_WRITES = 10000;

    // SQLiteCpp defaults this to 0, so transient contention fails instantly as an exception.
    constexpr int BUSY_TIMEOUT_MS = 5000;

    // Both thresholds must be met; SQLite never returns freed pages to the OS on its own.
    constexpr long long VACUUM_MIN_FREE_BYTES = 4LL * 1024 * 1024;
    constexpr double VACUUM_MIN_FREE_RATIO = 0.50;

    constexpr int INCREMENTAL_VACUUM_PAGES = 1024;
    constexpr unsigned long MAINTENANCE_DELAY_MS = 30000;

    struct DbError {
        std::string what;
        int code = -1; // primary SQLite result code, -1 when the exception is not SQLite's
        int extended = -1;
    };

    DbError DescribeDbError(const std::exception& e);

    // Read-only, corrupt or not a database: the file itself is unusable and retrying cannot help.
    bool IsFatalDbError(const DbError& err);

    // Logs (rate-limited per query) and shows one passive notice per query per window; never blocks the caller.
    void NotifyError(const std::string& query, const DbError& err);

    inline void NotifyError(const std::string& query, const std::exception& e) {
        NotifyError(query, DescribeDbError(e));
    }

    // Sibling marker asking the next start to quarantine and recreate the file.
    std::string DbRebuildMarkerPath(const std::string& dbPath);

    class Database {
        SQLite::Database db;
        std::string path_;
        std::atomic<int> writeCount{0};
        void maybeCheckpoint(int count);
        void maybeVacuum();

        void execDeleteByIdInChunk(const std::string& table, const std::string& idColumn, const std::vector<int>& ids);
        void execBatchSettingsReplaceChunk(const std::vector<std::pair<std::string, std::string>>& keyValues);
        void execBatchInsertIntPairsChunk(const std::string& table, const std::string& colA, const std::string& colB,
                                         const std::vector<int>& pairs);
        void execBatchInsertProfilesChunk(const std::vector<ProfileInsertRow>& rows);
        void execBatchReplaceProfilesChunk(const std::vector<ProfileInsertRow>& rows);
    public:
        explicit Database(const std::string& path, bool incrementalVacuum = false)
            : db(path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE, BUSY_TIMEOUT_MS), path_(path) {
            // Must precede journal_mode: WAL writes the header, after which auto_vacuum no longer takes.
            if (incrementalVacuum) db.exec("PRAGMA auto_vacuum = INCREMENTAL");
            db.exec("PRAGMA foreign_keys = ON");
            db.exec("PRAGMA journal_mode = WAL");
            db.exec("PRAGMA synchronous = NORMAL");
            db.exec("PRAGMA mmap_size = 67108864"); // 64MB
        }

        // Not safe from the constructor: a VACUUM there stalls startup.
        void RunMaintenance();

        [[nodiscard]] const std::string& Path() const { return path_; }

    private:

        // Explicit overloads avoid ambiguity on Linux (int32_t/int64_t/uint32_t).
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
        }

        template<typename... Args>
        void exec0(const std::string& sql, Args&&... args) {
            SQLite::Statement query(db, sql);
            bindArgs(query, 1, std::forward<Args>(args)...);
            query.exec();
            maybeCheckpoint(1);
        }

        // Safe to call from any thread.
        void checkpointWal();

        template<typename... Args>
        std::unique_ptr<SQLite::Statement> query0(const std::string& sql, Args&&... args) {
            auto query = std::make_unique<SQLite::Statement>(db, sql);
            bindArgs(*query, 1, std::forward<Args>(args)...);
            return query;
        }

        void execDeleteByIdIn0(const std::string& table, const std::string& idColumn, const std::vector<int>& ids) {
            for (size_t off = 0; off < ids.size(); off += BATCH_LIMIT_WRITE) {
                size_t end = std::min(off + BATCH_LIMIT_WRITE, ids.size());
                std::vector<int> chunk(ids.begin() + static_cast<std::ptrdiff_t>(off), ids.begin() + static_cast<std::ptrdiff_t>(end));
                execDeleteByIdInChunk(table, idColumn, chunk);
            }
        }

        void execBatchSettingsReplace0(const std::vector<std::pair<std::string, std::string>>& keyValues) {
            const size_t chunkSize = BATCH_LIMIT_WRITE / 2;
            for (size_t off = 0; off < keyValues.size(); off += chunkSize) {
                size_t end = std::min(off + chunkSize, keyValues.size());
                std::vector<std::pair<std::string, std::string>> chunk(keyValues.begin() + static_cast<std::ptrdiff_t>(off),
                                                                       keyValues.begin() + static_cast<std::ptrdiff_t>(end));
                execBatchSettingsReplaceChunk(chunk);
            }
        }

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

        // 13 bind params per row.
        void execBatchInsertProfiles0(const std::vector<ProfileInsertRow>& rows) {
            const size_t chunkSize = BATCH_LIMIT_WRITE / 13;
            for (size_t off = 0; off < rows.size(); off += chunkSize) {
                size_t end = std::min(off + chunkSize, rows.size());
                std::vector<ProfileInsertRow> chunk(rows.begin() + static_cast<std::ptrdiff_t>(off),
                                                    rows.begin() + static_cast<std::ptrdiff_t>(end));
                execBatchInsertProfilesChunk(chunk);
            }
        }

        void execBatchReplaceProfiles0(const std::vector<ProfileInsertRow>& rows) {
            const size_t chunkSize = BATCH_LIMIT_WRITE / 13;
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

        // Throws instead of swallowing, so a caller's explicit transaction aborts as one unit.
        template<typename... Args>
        void execThrow(const std::string& sql, Args&&... args) {
            exec0(sql, std::forward<Args>(args)...);
        }

        template<typename... Args>
        std::unique_ptr<SQLite::Statement> queryThrow(const std::string& sql, Args&&... args) {
            return query0(sql, std::forward<Args>(args)...);
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

        // Throws std::exception on failure. Caller must delete destPath if it already exists.
        void backupTo(const std::string& destPath);
        // Throws std::exception on failure.
        void restoreFrom(const std::string& srcPath);

        // entity_ids is always retained so restored IDs stay consistent; caller must delete destPath first. Throws on failure.
        void backupSelective(const std::string& destPath, const BackupParts& parts);

        // Only columns present in both schemas are copied, so cross-version backups still restore; a route rule with data
        // in a column this schema lacks is skipped instead. Returns the number of skipped route rules. Throws on failure.
        int restoreSelective(const std::string& srcPath, const BackupParts& parts);
    };
}
