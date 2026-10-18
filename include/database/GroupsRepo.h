#pragma once

#include "Database.h"
#include "include/database/entities/Group.h"
#include <memory>
#include <mutex>
#include <map>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>

namespace Configs {
    class GroupsRepo {
    private:
        Database& db;
        mutable std::mutex mutex;
        mutable std::map<int, std::shared_ptr<Group>> memMap;
        // Deleted ids: never reused, so Save refuses them for the rest of the process.
        QSet<int> tombstones;

        QJsonObject groupToJson(const Group* group) const;
        
        std::shared_ptr<Group> groupFromJson(const QJsonObject& json) const;
        
        void saveToDatabase(const Group* group, int id) const;
        
        std::shared_ptr<Group> loadFromDatabase(int id) const;
        
        void createTables() const;

        bool groupsColumnExists(const char* columnName) const;

        int NewGroupID() const;
    public:
        explicit GroupsRepo(Database& database);
        
        // Not saved yet: id stays -1 until AddGroup.
        [[nodiscard]] static std::shared_ptr<Group> NewGroup();
        
        bool AddGroup(std::shared_ptr<Group>& group);
        
        std::shared_ptr<Group> GetGroup(int id) const;
        
        // Resolves SettingsRepo::current_group.
        std::shared_ptr<Group> CurrentGroup() const;
        
        // Deletes the tab-order row, the group and its profiles (listed or not) in one transaction, and forgets them all.
        // False when the deletion failed and nothing changed.
        bool DeleteGroup(int id);
        
        QList<int> GetAllGroupIds() const;
        
        QList<int> GetGroupsTabOrder() const;
        
        // Deleted groups are left out.
        void SetGroupsTabOrder(const QList<int>& order);
        
        // No-op unless id >= 0; false for a deleted id, which is not written back.
        bool Save(const std::shared_ptr<Group>& group);
    };

    class Profile;

    // Keep sorted groups sorted from a background queue, one sort at a time; callable from any thread.
    // The main window gets GroupOrderChanged for each group whose order moved.

    // For changes that can move any row.
    void ResortLater(int gid);

    // For a few changed rows; only groups whose sort reads `input` are queued.
    void RepositionLater(const QHash<int, QList<int>>& changedByGroup, SortInput input);
    void RepositionLater(const QList<std::shared_ptr<Profile>>& profiles, SortInput input);
}
