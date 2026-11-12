#include "include/database/IpListsRepo.h"

#include <QSet>

#include <algorithm>
#include <chrono>
#include <limits>
#include <mutex>
#include <thread>

namespace Configs {
    namespace {
        // Keeps every WriteMutex and file-lock hold far below BUSY_TIMEOUT_MS.
        constexpr int kIpListsRepoChunkRows = 20000;
        constexpr int kIpListsRepoRowsPerInsert = BATCH_LIMIT_WRITE / 9;

        struct IpListsRepoPace {
            std::chrono::steady_clock::time_point lastEnd;
            std::chrono::steady_clock::duration lastHeld{};
        };
        thread_local IpListsRepoPace ipListsRepoPace;

        // Neither WriteMutex nor SQLite's busy polling is fair: back-to-back write chunks first wait half the last hold (1..50 ms).
        class IpListsRepoChunkLock {
        public:
            explicit IpListsRepoChunkLock(Database &db) : lock(db.WriteMutex(), std::defer_lock) {
                const auto gap = std::clamp<std::chrono::steady_clock::duration>(
                    ipListsRepoPace.lastHeld / 2, std::chrono::milliseconds(1), std::chrono::milliseconds(50));
                const auto idle = std::chrono::steady_clock::now() - ipListsRepoPace.lastEnd;
                if (idle < gap) std::this_thread::sleep_for(gap - idle);
                lock.lock();
                start = std::chrono::steady_clock::now();
            }

            ~IpListsRepoChunkLock() {
                lock.unlock();
                ipListsRepoPace.lastEnd = std::chrono::steady_clock::now();
                ipListsRepoPace.lastHeld = ipListsRepoPace.lastEnd - start;
            }

            IpListsRepoChunkLock(const IpListsRepoChunkLock &) = delete;
            IpListsRepoChunkLock &operator=(const IpListsRepoChunkLock &) = delete;

        private:
            std::unique_lock<std::recursive_mutex> lock;
            std::chrono::steady_clock::time_point start;
        };

        const std::string kIpListsRepoHeaderSelect =
            "SELECT id, name, related_test_id, role, source_kind, source, auto_update, update_interval, last_update, "
            "last_error, (SELECT COUNT(*) FROM ip_list_entries WHERE ip_list_id = ip_lists.id "
            "AND generation = ip_lists.entries_generation), entries_generation FROM ip_lists";

        const std::string kIpListsRepoHeaderSelectUncounted =
            "SELECT id, name, related_test_id, role, source_kind, source, auto_update, update_interval, last_update, "
            "last_error, 0, entries_generation FROM ip_lists";

        // Readers only see entries_generation; a bulk write fills another generation and flips to it in one transaction.
        constexpr const char *kIpListsRepoCreateEntries = R"(
            CREATE TABLE IF NOT EXISTS ip_list_entries (
                ip_list_id INTEGER NOT NULL,
                generation INTEGER NOT NULL DEFAULT 0,
                entry_order INTEGER NOT NULL,
                cidr TEXT NOT NULL,
                port INTEGER NOT NULL DEFAULT 0,
                latency_ms INTEGER NOT NULL DEFAULT 0,
                max_qname INTEGER NOT NULL DEFAULT 0,
                upload_mtu INTEGER NOT NULL DEFAULT 0,
                download_mtu INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (ip_list_id, generation, entry_order),
                FOREIGN KEY (ip_list_id) REFERENCES ip_lists(id) ON DELETE CASCADE
            ) WITHOUT ROWID
        )";

        bool ipListsRepoHasColumn(Database &db, const char *table, const char *column) {
            auto pragma = db.query(std::string("PRAGMA table_info(") + table + ")");
            if (!pragma) return false;
            while (pragma->executeStep()) {
                if (pragma->getColumn(1).getText() == std::string(column)) return true;
            }
            return false;
        }

        std::string ipListsRepoInsertSql(int rows) {
            std::string sql = "INSERT OR IGNORE INTO ip_list_entries (ip_list_id, generation, entry_order, cidr, port, "
                              "latency_ms, max_qname, upload_mtu, download_mtu) VALUES ";
            sql.reserve(sql.size() + static_cast<size_t>(rows) * 20);
            for (int i = 0; i < rows; ++i) {
                if (i > 0) sql += ',';
                sql += "(?,?,?,?,?,?,?,?,?)";
            }
            return sql;
        }

        QList<IpListEntry> ipListsRepoUnique(const QList<IpListEntry> &entries) {
            QList<IpListEntry> unique;
            unique.reserve(entries.size());
            QSet<QString> seen;
            seen.reserve(entries.size());
            for (const auto &entry : entries) {
                if (entry.cidr.isEmpty()) continue;
                const QString key = entry.cidr + QLatin1Char(' ') + QString::number(entry.port);
                const auto before = seen.size();
                seen.insert(key);
                if (seen.size() != before) unique.append(entry);
            }
            return unique;
        }

        // Throws, so the caller's transaction aborts as a whole.
        int ipListsRepoInsertEntries(Database &db, int listId, qint64 generation, const QList<IpListEntry> &entries,
                                     qsizetype begin, qsizetype end) {
            int written = 0;
            std::unique_ptr<SQLite::Statement> fullChunk;
            for (qsizetype offset = begin; offset < end; offset += kIpListsRepoRowsPerInsert) {
                const int rows = static_cast<int>(std::min<qsizetype>(kIpListsRepoRowsPerInsert, end - offset));
                std::unique_ptr<SQLite::Statement> tailChunk;
                SQLite::Statement *stmt;
                if (rows == kIpListsRepoRowsPerInsert) {
                    if (!fullChunk) fullChunk = db.queryThrow(ipListsRepoInsertSql(rows));
                    stmt = fullChunk.get();
                } else {
                    tailChunk = db.queryThrow(ipListsRepoInsertSql(rows));
                    stmt = tailChunk.get();
                }
                int index = 1;
                for (int i = 0; i < rows; ++i) {
                    const auto &entry = entries[offset + i];
                    stmt->bind(index++, listId);
                    stmt->bind(index++, static_cast<int64_t>(generation));
                    stmt->bind(index++, static_cast<int64_t>(offset + i));
                    stmt->bind(index++, entry.cidr.toStdString());
                    stmt->bind(index++, entry.port);
                    stmt->bind(index++, entry.latencyMs);
                    stmt->bind(index++, entry.maxQName);
                    stmt->bind(index++, entry.uploadMtu);
                    stmt->bind(index++, entry.downloadMtu);
                }
                written += stmt->exec();
                stmt->reset();
            }
            return written;
        }

        qint64 ipListsRepoGeneration(Database &db, int id) {
            const auto query = db.queryThrow("SELECT entries_generation FROM ip_lists WHERE id = ?", id);
            return query->executeStep() ? static_cast<qint64>(query->getColumn(0).getInt64()) : -1;
        }

        qint64 ipListsRepoLockedGeneration(Database &db, int id, const char *op) {
            try {
                std::lock_guard lock(db.WriteMutex());
                return ipListsRepoGeneration(db, id);
            } catch (const std::exception &e) {
                NotifyError(op, e);
                return -1;
            }
        }

        bool ipListsRepoPurgeGeneration(Database &db, int id, qint64 generation) {
            for (bool more = true; more;) {
                const IpListsRepoChunkLock lock(db);
                const bool ok = db.transaction("IpListsRepo::purgeGeneration", [&] {
                    // A restore on the main connection can make this generation the live one again.
                    if (ipListsRepoGeneration(db, id) == generation) return false;
                    qint64 bound = -1;
                    {
                        const auto query = db.queryThrow(
                            "SELECT entry_order FROM ip_list_entries WHERE ip_list_id = ? AND generation = ? "
                            "ORDER BY entry_order LIMIT 1 OFFSET ?",
                            id, generation, kIpListsRepoChunkRows - 1);
                        if (query->executeStep()) bound = query->getColumn(0).getInt64();
                    }
                    more = bound >= 0;
                    if (more) {
                        db.execThrow("DELETE FROM ip_list_entries WHERE ip_list_id = ? AND generation = ? AND entry_order <= ?",
                                     id, generation, bound);
                    } else {
                        db.execThrow("DELETE FROM ip_list_entries WHERE ip_list_id = ? AND generation = ?", id, generation);
                    }
                    return true;
                });
                if (!ok) return false;
            }
            return true;
        }

        bool ipListsRepoPurgeExcept(Database &db, int id, qint64 keep) {
            for (;;) {
                qint64 stale = -1;
                try {
                    std::lock_guard lock(db.WriteMutex());
                    for (const char *sql : {"SELECT generation FROM ip_list_entries WHERE ip_list_id = ? AND generation < ? LIMIT 1",
                                            "SELECT generation FROM ip_list_entries WHERE ip_list_id = ? AND generation > ? LIMIT 1"}) {
                        const auto query = db.queryThrow(sql, id, keep);
                        if (query->executeStep()) {
                            stale = query->getColumn(0).getInt64();
                            break;
                        }
                    }
                } catch (const std::exception &e) {
                    NotifyError("IpListsRepo::purgeGenerations", e);
                    return false;
                }
                if (stale < 0) return true;
                if (!ipListsRepoPurgeGeneration(db, id, stale)) return false;
            }
        }

        qint64 ipListsRepoBumpGeneration(Database &db, int id, bool requireDetached = false) {
            std::lock_guard lock(db.WriteMutex());
            qint64 fresh = -1;
            const bool ok = db.transaction("IpListsRepo::bumpGeneration", [&] {
                if (requireDetached) {
                    const auto query = db.queryThrow("SELECT 1 FROM ip_lists WHERE id = ? AND role = ? AND related_test_id = -1", id,
                                                     static_cast<int>(IpList::Role::ScanSnapshot));
                    if (!query->executeStep()) return false;
                }
                fresh = ipListsRepoGeneration(db, id);
                if (fresh < 0) return false;
                {
                    const auto query = db.queryThrow(
                        "SELECT generation FROM ip_list_entries WHERE ip_list_id = ? ORDER BY generation DESC LIMIT 1", id);
                    if (query->executeStep()) fresh = std::max<qint64>(fresh, query->getColumn(0).getInt64());
                }
                ++fresh;
                db.execThrow("UPDATE ip_lists SET entries_generation = ?, updated_at = strftime('%s', 'now') WHERE id = ?",
                             fresh, id);
                return true;
            });
            return ok ? fresh : -1;
        }

        bool ipListsRepoStage(Database &db, const char *op, int id, qint64 current, qint64 target,
                              const QList<IpListEntry> &entries, int &written) {
            const qsizetype total = entries.size();
            qsizetype begin = 0;
            do {
                const qsizetype end = std::min<qsizetype>(begin + kIpListsRepoChunkRows, total);
                const IpListsRepoChunkLock lock(db);
                const bool ok = db.transaction(op, [&] {
                    if (ipListsRepoGeneration(db, id) != current) return false;
                    written += ipListsRepoInsertEntries(db, id, target, entries, begin, end);
                    if (end == total) {
                        db.execThrow("UPDATE ip_lists SET entries_generation = ?, updated_at = strftime('%s', 'now') WHERE id = ?",
                                     target, id);
                    }
                    return true;
                });
                if (!ok) return false;
                begin = end;
            } while (begin < total);
            return true;
        }

        // expectedGeneration >= 0: the header goes only if no restore replaced the row meanwhile.
        void ipListsRepoDiscard(Database &db, int id, qint64 expectedGeneration = -1) {
            const bool purged = ipListsRepoPurgeExcept(db, id, expectedGeneration);
            std::lock_guard lock(db.WriteMutex());
            if (expectedGeneration < 0) {
                db.exec("DELETE FROM ip_lists WHERE id = ?", id);
            } else if (purged) {
                db.exec("DELETE FROM ip_lists WHERE id = ? AND entries_generation = ?", id, expectedGeneration);
            }
        }

        bool ipListsRepoReadGeneration(Database &db, int id, qint64 generation, int offset, int limit,
                                       QList<IpListEntry> &out) {
            if (limit > 0) out.reserve(std::min(limit, BATCH_LIMIT_READ));
            qint64 remaining = limit < 0 ? std::numeric_limits<qint64>::max() : limit;
            qint64 after = -1;
            while (remaining > 0) {
                std::lock_guard lock(db.WriteMutex());
                if (ipListsRepoGeneration(db, id) != generation) return false;
                const int page = static_cast<int>(std::min<qint64>(remaining, kIpListsRepoChunkRows));
                const auto query =
                    after < 0 ? db.queryThrow("SELECT entry_order, cidr, port, latency_ms, max_qname, upload_mtu, download_mtu "
                                              "FROM ip_list_entries "
                                              "WHERE ip_list_id = ? AND generation = ? ORDER BY entry_order LIMIT ? OFFSET ?",
                                              id, generation, page, std::max(offset, 0))
                              : db.queryThrow("SELECT entry_order, cidr, port, latency_ms, max_qname, upload_mtu, download_mtu "
                                              "FROM ip_list_entries "
                                              "WHERE ip_list_id = ? AND generation = ? AND entry_order > ? "
                                              "ORDER BY entry_order LIMIT ?",
                                              id, generation, after, page);
                int rows = 0;
                while (query->executeStep()) {
                    after = query->getColumn(0).getInt64();
                    const auto cidrColumn = query->getColumn(1);
                    const char *cidr = cidrColumn.getText();
                    const int cidrBytes = cidrColumn.getBytes();
                    out.append({QString::fromUtf8(cidr, cidrBytes), query->getColumn(2).getInt(), query->getColumn(3).getInt(),
                                query->getColumn(4).getInt(), query->getColumn(5).getInt(), query->getColumn(6).getInt()});
                    ++rows;
                }
                if (rows < page) break;
                remaining -= rows;
            }
            return true;
        }

        // Leftovers of bulk writes cut short by an exit; must run before any writer starts.
        void ipListsRepoPurgeLeftovers(Database &db) {
            std::lock_guard lock(db.WriteMutex());
            try {
                std::vector<std::pair<int, qint64>> stale;
                {
                    const auto query = db.queryThrow(
                        "SELECT id, entries_generation FROM ip_lists AS l WHERE "
                        "EXISTS (SELECT 1 FROM ip_list_entries WHERE ip_list_id = l.id AND generation < l.entries_generation) OR "
                        "EXISTS (SELECT 1 FROM ip_list_entries WHERE ip_list_id = l.id AND generation > l.entries_generation)");
                    while (query->executeStep()) stale.emplace_back(query->getColumn(0).getInt(), query->getColumn(1).getInt64());
                }
                for (const auto &[id, generation] : stale) {
                    db.execThrow("DELETE FROM ip_list_entries WHERE ip_list_id = ? AND generation < ?", id, generation);
                    db.execThrow("DELETE FROM ip_list_entries WHERE ip_list_id = ? AND generation > ?", id, generation);
                }
            } catch (const std::exception &e) {
                NotifyError("IpListsRepo::purgeLeftovers", e);
            }
        }

        void ipListsRepoInsertHeader(Database &db, int id, const IpList &list) {
            db.execThrow(
                "INSERT INTO ip_lists (id, name, related_test_id, role, source_kind, source, auto_update, update_interval, "
                "last_update, last_error, sort_order) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                "(SELECT COALESCE(MAX(sort_order), 0) + 1 FROM ip_lists))",
                id,
                list.name.toStdString(),
                list.related_test_id,
                static_cast<int>(list.role),
                static_cast<int>(list.source_kind),
                list.source.toStdString(),
                list.auto_update ? 1 : 0,
                list.update_interval,
                list.last_update,
                list.last_error.toStdString());
        }
    } // namespace

    class IpListsRepo::ListClaim {
    public:
        ListClaim(IpListsRepo &owner, int listId) : repo(owner), id(listId) {
            std::unique_lock lock(repo.claimMutex);
            repo.claimReleased.wait(lock, [this] { return !repo.claimedLists.contains(id); });
            repo.claimedLists.insert(id);
        }

        ~ListClaim() {
            {
                std::lock_guard lock(repo.claimMutex);
                repo.claimedLists.erase(id);
            }
            repo.claimReleased.notify_all();
        }

        ListClaim(const ListClaim &) = delete;
        ListClaim &operator=(const ListClaim &) = delete;

    private:
        IpListsRepo &repo;
        int id;
    };

    IpListsRepo::IpListsRepo(Database &database) : db(database) {
        createTables();
    }

    void IpListsRepo::createTables() const {
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS ip_lists (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL DEFAULT '',
                related_test_id INTEGER NOT NULL DEFAULT -1,
                role INTEGER NOT NULL DEFAULT 0,
                source_kind INTEGER NOT NULL DEFAULT 0,
                source TEXT NOT NULL DEFAULT '',
                auto_update INTEGER NOT NULL DEFAULT 0,
                update_interval INTEGER NOT NULL DEFAULT 1440,
                last_update INTEGER NOT NULL DEFAULT 0,
                last_error TEXT NOT NULL DEFAULT '',
                sort_order INTEGER NOT NULL DEFAULT 0,
                entries_generation INTEGER NOT NULL DEFAULT 0,
                created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
                updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
            )
        )");
        if (!ipListsColumnExists("entries_generation"))
            db.exec("ALTER TABLE ip_lists ADD COLUMN entries_generation INTEGER NOT NULL DEFAULT 0");
        db.exec(kIpListsRepoCreateEntries);
        if (!ipListsRepoHasColumn(db, "ip_list_entries", "generation")) {
            db.transaction("IpListsRepo::createTables", [&] {
                db.execThrow("ALTER TABLE ip_list_entries RENAME TO ip_list_entries_legacy");
                db.execThrow(kIpListsRepoCreateEntries);
                db.execThrow("INSERT INTO ip_list_entries (ip_list_id, generation, entry_order, cidr, port, latency_ms) "
                             "SELECT ip_list_id, 0, entry_order, cidr, port, latency_ms FROM ip_list_entries_legacy");
                db.execThrow("DROP TABLE ip_list_entries_legacy");
                return true;
            });
        }
        for (const char *column : {"max_qname", "upload_mtu", "download_mtu"}) {
            if (!ipListsRepoHasColumn(db, "ip_list_entries", column))
                db.exec(std::string("ALTER TABLE ip_list_entries ADD COLUMN ") + column + " INTEGER NOT NULL DEFAULT 0");
        }
        db.exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_ip_list_entries_target ON ip_list_entries(ip_list_id, generation, cidr, port)");
        ipListsRepoPurgeLeftovers(db);
    }

    bool IpListsRepo::ipListsColumnExists(const char *columnName) const {
        return ipListsRepoHasColumn(db, "ip_lists", columnName);
    }

    std::shared_ptr<IpList> IpListsRepo::ipListFromRow(SQLite::Statement &stmt) const {
        auto list = std::make_shared<IpList>();
        list->id = stmt.getColumn(0).getInt();
        list->name = QString::fromStdString(stmt.getColumn(1).getString());
        list->related_test_id = stmt.getColumn(2).getInt();
        const int role = stmt.getColumn(3).getInt();
        list->role = role >= 0 && role <= 2 ? static_cast<IpList::Role>(role) : IpList::Role::User;
        const int sourceKind = stmt.getColumn(4).getInt();
        list->source_kind = sourceKind >= 0 && sourceKind <= 2 ? static_cast<IpList::SourceKind>(sourceKind)
                                                               : IpList::SourceKind::Manual;
        list->source = QString::fromStdString(stmt.getColumn(5).getString());
        list->auto_update = stmt.getColumn(6).getInt() != 0;
        list->update_interval = stmt.getColumn(7).getInt();
        list->last_update = stmt.getColumn(8).getInt64();
        list->last_error = QString::fromStdString(stmt.getColumn(9).getString());
        list->entryCount = stmt.getColumn(10).getInt();
        return list;
    }

    int IpListsRepo::NewIpListID() const {
        std::lock_guard lock(db.WriteMutex());
        try {
            const auto query = db.queryThrow("UPDATE entity_ids SET ip_list_last_id = ip_list_last_id + 1 RETURNING ip_list_last_id");
            if (query->executeStep()) return query->getColumn(0).getInt();
        } catch (const std::exception &e) {
            NotifyError("IpListsRepo::NewIpListID", e);
        }
        return 0;
    }

    std::shared_ptr<IpList> IpListsRepo::NewIpList() {
        return std::make_shared<IpList>();
    }

    bool IpListsRepo::AddIpList(std::shared_ptr<IpList> &list) {
        if (!list || list->id >= 0) return false;
        const QList<IpListEntry> entries = ipListsRepoUnique(list->entries);
        const bool staged = entries.size() > kIpListsRepoChunkRows;
        int id = 0;
        int written = 0;
        {
            std::lock_guard lock(db.WriteMutex());
            const bool ok = db.transaction("IpListsRepo::AddIpList", [&] {
                id = NewIpListID();
                if (id <= 0) return false;
                ipListsRepoInsertHeader(db, id, *list);
                if (!staged) written = ipListsRepoInsertEntries(db, id, 0, entries, 0, entries.size());
                return true;
            });
            if (!ok) return false;
        }
        if (staged) {
            const ListClaim claim(*this, id);
            if (!ipListsRepoStage(db, "IpListsRepo::AddIpList", id, 0, 1, entries, written)) {
                ipListsRepoDiscard(db, id);
                return false;
            }
        }
        list->id = id;
        if (entries.size() != list->entries.size()) list->entries = entries;
        list->entryCount = written;
        list->entriesLoaded = true;
        return true;
    }

    std::shared_ptr<IpList> IpListsRepo::GetIpList(int id) const {
        try {
            // A replace publishing between two pages restarts the read once, then under one lock hold.
            for (int attempt = 0; attempt < 2; ++attempt) {
                std::unique_lock hold(db.WriteMutex(), std::defer_lock);
                if (attempt > 0) hold.lock();
                std::shared_ptr<IpList> list;
                qint64 generation = -1;
                {
                    std::lock_guard lock(db.WriteMutex());
                    const auto query = db.queryThrow(kIpListsRepoHeaderSelectUncounted + " WHERE id = ?", id);
                    if (!query->executeStep()) return nullptr;
                    list = ipListFromRow(*query);
                    generation = query->getColumn(11).getInt64();
                }
                if (!ipListsRepoReadGeneration(db, id, generation, 0, -1, list->entries)) continue;
                list->entryCount = static_cast<int>(list->entries.size());
                list->entriesLoaded = true;
                return list;
            }
        } catch (const std::exception &e) {
            NotifyError("IpListsRepo::GetIpList", e);
        }
        return nullptr;
    }

    std::shared_ptr<IpList> IpListsRepo::GetIpListHeader(int id) const {
        try {
            std::lock_guard lock(db.WriteMutex());
            const auto query = db.queryThrow(kIpListsRepoHeaderSelectUncounted + " WHERE id = ?", id);
            if (query->executeStep()) return ipListFromRow(*query);
        } catch (const std::exception &e) {
            NotifyError("IpListsRepo::GetIpListHeader", e);
        }
        return nullptr;
    }

    QList<std::shared_ptr<IpList>> IpListsRepo::GetAllIpLists(bool includeHidden) const {
        QList<std::shared_ptr<IpList>> lists;
        const std::string filter =
            includeHidden ? "" : " WHERE role != " + std::to_string(static_cast<int>(IpList::Role::ScanSnapshot));
        try {
            std::lock_guard lock(db.WriteMutex());
            const auto query = db.queryThrow(kIpListsRepoHeaderSelect + filter + " ORDER BY sort_order, id");
            while (query->executeStep()) lists.append(ipListFromRow(*query));
        } catch (const std::exception &e) {
            NotifyError("IpListsRepo::GetAllIpLists", e);
            lists.clear();
        }
        return lists;
    }

    QList<IpListEntry> IpListsRepo::GetEntries(int id, int offset, int limit) const {
        try {
            for (int attempt = 0; attempt < 2; ++attempt) {
                std::unique_lock hold(db.WriteMutex(), std::defer_lock);
                if (attempt > 0) hold.lock();
                const qint64 generation = ipListsRepoLockedGeneration(db, id, "IpListsRepo::GetEntries");
                if (generation < 0) return {};
                QList<IpListEntry> entries;
                if (ipListsRepoReadGeneration(db, id, generation, offset, limit, entries)) return entries;
            }
        } catch (const std::exception &e) {
            NotifyError("IpListsRepo::GetEntries", e);
        }
        return {};
    }

    int IpListsRepo::EntryCount(int id) const {
        try {
            std::lock_guard lock(db.WriteMutex());
            const auto query = db.queryThrow(
                "SELECT COUNT(*) FROM ip_list_entries WHERE ip_list_id = ? "
                "AND generation = (SELECT entries_generation FROM ip_lists WHERE id = ?)",
                id, id);
            if (query->executeStep()) return query->getColumn(0).getInt();
        } catch (const std::exception &e) {
            NotifyError("IpListsRepo::EntryCount", e);
        }
        return 0;
    }

    bool IpListsRepo::SaveHeader(const std::shared_ptr<IpList> &list) {
        if (!list || list->id < 0) return false;
        std::lock_guard lock(db.WriteMutex());
        // An UPDATE, never an upsert: a refresh finishing after the list was deleted must not bring it back.
        return db.execChanges(
                   "UPDATE ip_lists SET name = ?, related_test_id = ?, role = ?, source_kind = ?, source = ?, auto_update = ?, "
                   "update_interval = ?, last_update = ?, last_error = ?, updated_at = strftime('%s', 'now') WHERE id = ?",
                   list->name.toStdString(),
                   list->related_test_id,
                   static_cast<int>(list->role),
                   static_cast<int>(list->source_kind),
                   list->source.toStdString(),
                   list->auto_update ? 1 : 0,
                   list->update_interval,
                   list->last_update,
                   list->last_error.toStdString(),
                   list->id) > 0;
    }

    bool IpListsRepo::ReplaceEntries(int id, const QList<IpListEntry> &entries) {
        if (id < 0) return false;
        const QList<IpListEntry> unique = ipListsRepoUnique(entries);
        const ListClaim claim(*this, id);
        const qint64 current = ipListsRepoLockedGeneration(db, id, "IpListsRepo::ReplaceEntries");
        if (current < 0) return false;
        // Under the claim, any other generation is a leftover of an interrupted write.
        if (!ipListsRepoPurgeExcept(db, id, current)) return false;
        int written = 0;
        if (!ipListsRepoStage(db, "IpListsRepo::ReplaceEntries", id, current, current + 1, unique, written)) {
            ipListsRepoPurgeGeneration(db, id, current + 1);
            return false;
        }
        ipListsRepoPurgeGeneration(db, id, current);
        return true;
    }

    int IpListsRepo::UpsertEntries(int id, const QList<IpListEntry> &entries) {
        if (id < 0 || entries.isEmpty()) return 0;
        int appended = 0;
        qint64 pinned = -1;
        for (qsizetype begin = 0; begin < entries.size(); begin += kIpListsRepoChunkRows) {
            const qsizetype end = std::min<qsizetype>(begin + kIpListsRepoChunkRows, entries.size());
            int chunkAppended = 0;
            const IpListsRepoChunkLock lock(db);
            const bool ok = db.transaction("IpListsRepo::UpsertEntries", [&] {
                const qint64 generation = ipListsRepoGeneration(db, id);
                // Replaced between two chunks: the rest would land in a list this call never saw.
                if (generation < 0 || (pinned >= 0 && generation != pinned)) return false;
                pinned = generation;
                qint64 order = 0;
                {
                    const auto query = db.queryThrow(
                        "SELECT entry_order FROM ip_list_entries WHERE ip_list_id = ? AND generation = ? "
                        "ORDER BY entry_order DESC LIMIT 1",
                        id, generation);
                    if (query->executeStep()) order = query->getColumn(0).getInt64() + 1;
                }
                const auto insert = db.queryThrow(
                    "INSERT OR IGNORE INTO ip_list_entries (ip_list_id, generation, entry_order, cidr, port, latency_ms, "
                    "max_qname, upload_mtu, download_mtu) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
                const auto update = db.queryThrow(
                    "UPDATE ip_list_entries SET latency_ms = ?, max_qname = ?, upload_mtu = ?, download_mtu = ? "
                    "WHERE ip_list_id = ? AND generation = ? AND cidr = ? AND port = ?");
                for (qsizetype i = begin; i < end; ++i) {
                    const auto &entry = entries[i];
                    if (entry.cidr.isEmpty()) continue;
                    const std::string cidr = entry.cidr.toStdString();
                    insert->bind(1, id);
                    insert->bind(2, static_cast<int64_t>(generation));
                    insert->bind(3, static_cast<int64_t>(order));
                    insert->bind(4, cidr);
                    insert->bind(5, entry.port);
                    insert->bind(6, entry.latencyMs);
                    insert->bind(7, entry.maxQName);
                    insert->bind(8, entry.uploadMtu);
                    insert->bind(9, entry.downloadMtu);
                    const int inserted = insert->exec();
                    insert->reset();
                    if (inserted > 0) {
                        ++order;
                        ++chunkAppended;
                        continue;
                    }
                    update->bind(1, entry.latencyMs);
                    update->bind(2, entry.maxQName);
                    update->bind(3, entry.uploadMtu);
                    update->bind(4, entry.downloadMtu);
                    update->bind(5, id);
                    update->bind(6, static_cast<int64_t>(generation));
                    update->bind(7, cidr);
                    update->bind(8, entry.port);
                    update->exec();
                    update->reset();
                }
                return true;
            });
            if (!ok) break;
            appended += chunkAppended;
        }
        return appended;
    }

    int IpListsRepo::RemoveEntries(int id, const QList<IpListEntry> &entries) {
        if (id < 0 || entries.isEmpty()) return 0;
        int removed = 0;
        qint64 pinned = -1;
        for (qsizetype begin = 0; begin < entries.size(); begin += kIpListsRepoChunkRows) {
            const qsizetype end = std::min<qsizetype>(begin + kIpListsRepoChunkRows, entries.size());
            int chunkRemoved = 0;
            const IpListsRepoChunkLock lock(db);
            const bool ok = db.transaction("IpListsRepo::RemoveEntries", [&] {
                const qint64 generation = ipListsRepoGeneration(db, id);
                if (generation < 0 || (pinned >= 0 && generation != pinned)) return false;
                pinned = generation;
                const auto remove = db.queryThrow(
                    "DELETE FROM ip_list_entries WHERE ip_list_id = ? AND generation = ? AND cidr = ? AND port = ?");
                for (qsizetype i = begin; i < end; ++i) {
                    const auto &entry = entries[i];
                    remove->bind(1, id);
                    remove->bind(2, static_cast<int64_t>(generation));
                    remove->bind(3, entry.cidr.toStdString());
                    remove->bind(4, entry.port);
                    chunkRemoved += remove->exec();
                    remove->reset();
                }
                return true;
            });
            if (!ok) break;
            removed += chunkRemoved;
        }
        return removed;
    }

    void IpListsRepo::ClearEntries(int id) {
        if (id < 0) return;
        const ListClaim claim(*this, id);
        const qint64 fresh = ipListsRepoBumpGeneration(db, id);
        if (fresh >= 0) ipListsRepoPurgeExcept(db, id, fresh);
    }

    void IpListsRepo::DetachIpList(int id) {
        if (id < 0) return;
        std::lock_guard lock(db.WriteMutex());
        db.exec("UPDATE ip_lists SET role = ?, related_test_id = -1, updated_at = strftime('%s', 'now') WHERE id = ?",
                static_cast<int>(IpList::Role::ScanSnapshot), id);
    }

    void IpListsRepo::DeleteIpList(int id, bool onlyIfDetached) {
        if (id < 0) return;
        const ListClaim claim(*this, id);
        // Readers see an empty list while the rows go chunk by chunk; the header goes last.
        const qint64 fresh = ipListsRepoBumpGeneration(db, id, onlyIfDetached);
        if (fresh < 0) return;
        ipListsRepoDiscard(db, id, fresh);
    }

    QList<int> IpListsRepo::OrphanedIpLists() const {
        QList<int> ids;
        std::lock_guard lock(db.WriteMutex());
        try {
            const auto query = db.queryThrow(
                "SELECT id FROM ip_lists AS l WHERE role = ? AND NOT EXISTS (SELECT 1 FROM ip_scans WHERE id = l.related_test_id)",
                static_cast<int>(IpList::Role::ScanSnapshot));
            while (query->executeStep()) ids.append(query->getColumn(0).getInt());
        } catch (const std::exception &e) {
            NotifyError("IpListsRepo::OrphanedIpLists", e);
            ids.clear();
        }
        return ids;
    }

    void IpListsRepo::UpdateIpListsOrder(const QList<int> &idsInOrder) {
        if (idsInOrder.isEmpty()) return;
        std::lock_guard lock(db.WriteMutex());
        db.transaction("IpListsRepo::UpdateIpListsOrder", [&] {
            const auto update = db.queryThrow("UPDATE ip_lists SET sort_order = ? WHERE id = ?");
            for (int i = 0; i < idsInOrder.size(); ++i) {
                update->bind(1, i + 1);
                update->bind(2, idsInOrder[i]);
                update->exec();
                update->reset();
            }
            return true;
        });
    }

    std::shared_ptr<IpList> IpListsRepo::CopyAsUserList(int sourceId, const QString &name) {
        if (sourceId < 0) return nullptr;
        int id = 0;
        {
            // Keeps the source on one generation for the whole copy.
            const ListClaim sourceClaim(*this, sourceId);
            qint64 sourceGeneration = -1;
            {
                std::lock_guard lock(db.WriteMutex());
                const bool ok = db.transaction("IpListsRepo::CopyAsUserList", [&] {
                    IpList copy;
                    copy.name = name;
                    {
                        const auto source =
                            db.queryThrow("SELECT entries_generation FROM ip_lists WHERE id = ?", sourceId);
                        if (!source->executeStep()) return false;
                        sourceGeneration = source->getColumn(0).getInt64();
                    }
                    id = NewIpListID();
                    if (id <= 0) return false;
                    ipListsRepoInsertHeader(db, id, copy);
                    return true;
                });
                if (!ok) return nullptr;
            }
            const ListClaim targetClaim(*this, id);
            qint64 after = -1;
            for (bool last = false; !last;) {
                const IpListsRepoChunkLock lock(db);
                const bool ok = db.transaction("IpListsRepo::CopyAsUserList", [&] {
                    if (ipListsRepoGeneration(db, id) != 0) return false;
                    qint64 bound = -1;
                    {
                        const auto query = db.queryThrow(
                            "SELECT entry_order FROM ip_list_entries WHERE ip_list_id = ? AND generation = ? AND entry_order > ? "
                            "ORDER BY entry_order LIMIT 1 OFFSET ?",
                            sourceId, sourceGeneration, after, kIpListsRepoChunkRows - 1);
                        if (query->executeStep()) bound = query->getColumn(0).getInt64();
                    }
                    last = bound < 0;
                    db.execThrow(
                        "INSERT INTO ip_list_entries (ip_list_id, generation, entry_order, cidr, port, latency_ms, max_qname, "
                        "upload_mtu, download_mtu) "
                        "SELECT ?, 1, entry_order, cidr, port, latency_ms, max_qname, upload_mtu, download_mtu "
                        "FROM ip_list_entries "
                        "WHERE ip_list_id = ? AND generation = ? AND entry_order > ? AND entry_order <= ?",
                        id, sourceId, sourceGeneration, after, last ? std::numeric_limits<qint64>::max() : bound);
                    if (last) db.execThrow("UPDATE ip_lists SET entries_generation = 1 WHERE id = ?", id);
                    after = bound;
                    return true;
                });
                if (!ok) {
                    ipListsRepoDiscard(db, id);
                    return nullptr;
                }
            }
        }
        return GetIpList(id);
    }
} // namespace Configs
