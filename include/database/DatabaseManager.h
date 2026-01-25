#pragma once

#include "Database.h"
#include <string>
#include <memory>

// Forward declarations
namespace Configs {
    class ProfilesRepo;
    class GroupsRepo;
    class RoutesRepo;
}

namespace Configs {
    class DatabaseManager {
    private:
        Database db;
        
        static void createEntityIdsTable(Database& db);
        void initializeRepos();
    public:
        std::unique_ptr<ProfilesRepo> profilesRepo;
        std::unique_ptr<GroupsRepo> groupsRepo;
        std::unique_ptr<RoutesRepo> routesRepo;

        explicit DatabaseManager(const std::string& dbPath);
        ~DatabaseManager() = default;
        
        // Non-copyable
        DatabaseManager(const DatabaseManager&) = delete;
        DatabaseManager& operator=(const DatabaseManager&) = delete;
        
        // Get the underlying Database reference (for repos to access entity_ids table)
        Database& getDatabase() { return db; }
        const Database& getDatabase() const { return db; }
    };
}
