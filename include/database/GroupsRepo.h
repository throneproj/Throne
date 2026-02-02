#pragma once

#include "Database.h"
#include "include/database/entities/Group.h"
#include <memory>
#include <mutex>
#include <map>
#include <QJsonObject>

namespace Configs {
    class GroupsRepo {
    private:
        Database& db;
        mutable std::mutex mutex;
        // Identity map: id -> weak_ptr<Group>
        mutable std::map<int, std::weak_ptr<Group>> identityMap;

        // Helper to serialize Group to JSON
        QJsonObject groupToJson(const Group* group) const;
        
        // Helper to deserialize Group from JSON
        std::shared_ptr<Group> groupFromJson(const QJsonObject& json) const;
        
        // Save group to database (internal helper)
        void saveToDatabase(const Group* group, int id) const;
        
        // Load group from database
        std::shared_ptr<Group> loadFromDatabase(int id) const;
        
        // Create tables if they don't exist
        void createTables() const;

        // Get next available group ID
        int NewGroupID() const;
    public:
        explicit GroupsRepo(Database& database);
        
        // Create a new group (doesn't save to DB yet, id will be -1)
        [[nodiscard]] static std::shared_ptr<Group> NewGroup();
        
        // Add group to database (assigns ID and saves)
        bool AddGroup(std::shared_ptr<Group>& group);
        
        // Get group by ID (uses identity map)
        std::shared_ptr<Group> GetGroup(int id) const;
        
        // Get current group (reads current_group from SettingsRepo and returns the group)
        std::shared_ptr<Group> CurrentGroup() const;
        
        // Delete group from database
        void DeleteGroup(int id);
        
        // Get all group IDs in order
        QList<int> GetAllGroupIds() const;
        
        // Get groups tab order (UI display order)
        QList<int> GetGroupsTabOrder() const;
        
        // Set groups tab order (UI display order)
        void SetGroupsTabOrder(const QList<int>& order);
        
        // Save group to database (manual save, like old Save() method)
        // Only saves if group has a valid ID (id >= 0)
        bool Save(const std::shared_ptr<Group>& group);
    };
}
