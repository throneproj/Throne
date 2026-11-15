#pragma once

#include <QList>
#include <QMutex>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <set>

#include "Database.h"
#include "include/database/entities/IpList.h"

namespace Configs {
    // Getters return fresh objects; safe from any thread.
    class IpListsRepo {
    private:
        class ListClaim;

        // Writes hold db.WriteMutex(), shared with IpScansRepo on the same connection.
        Database &db;

        // Lists with a generation-changing write in flight; such writes run one at a time per list.
        std::mutex claimMutex;
        std::condition_variable claimReleased;
        std::set<int> claimedLists;

        void createTables() const;

        [[nodiscard]] bool ipListsColumnExists(const char *columnName) const;

        [[nodiscard]] std::shared_ptr<IpList> ipListFromRow(SQLite::Statement &stmt) const;

        [[nodiscard]] int NewIpListID() const;

    public:
        explicit IpListsRepo(Database &database);

        [[nodiscard]] static std::shared_ptr<IpList> NewIpList();

        bool AddIpList(std::shared_ptr<IpList> &list);

        [[nodiscard]] std::shared_ptr<IpList> GetIpList(int id) const;

        // No entries and no count (entryCount stays 0).
        [[nodiscard]] std::shared_ptr<IpList> GetIpListHeader(int id) const;

        // Headers only; snapshot lists only with includeHidden.
        [[nodiscard]] QList<std::shared_ptr<IpList>> GetAllIpLists(bool includeHidden = false) const;

        [[nodiscard]] QList<IpListEntry> GetEntries(int id, int offset = 0, int limit = -1) const;

        // Lowest positive latency; with none measured, the first entry in list order.
        [[nodiscard]] std::optional<IpListEntry> GetBestEntry(int id) const;

        [[nodiscard]] int EntryCount(int id) const;

        bool SaveHeader(const std::shared_ptr<IpList> &list);

        // Atomic; the first of duplicate (cidr, port) pairs wins.
        bool ReplaceEntries(int id, const QList<IpListEntry> &entries);

        // Existing (cidr, port) pairs take the new latency; returns how many were appended.
        int UpsertEntries(int id, const QList<IpListEntry> &entries);

        int RemoveEntries(int id, const QList<IpListEntry> &entries);

        void ClearEntries(int id);

        // Hide and unlink in one write, so nothing adopts the list during a slow delete.
        void DetachIpList(int id);

        // onlyIfDetached: skip a list that is no longer detached (a backup restore brought it back).
        void DeleteIpList(int id, bool onlyIfDetached = false);

        [[nodiscard]] QList<int> OrphanedIpLists() const;

        void UpdateIpListsOrder(const QList<int> &idsInOrder);

        std::shared_ptr<IpList> CopyAsUserList(int sourceId, const QString &name);
    };
} // namespace Configs
