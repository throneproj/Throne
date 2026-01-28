#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"

#include "include/global/Configs.hpp"

namespace Configs {
    DatabaseManager::DatabaseManager(const std::string& dbPath)
        : db(dbPath) {
        // Create entity IDs table first (before repos are initialized)
        createEntityIdsTable(db);
        
        // Initialize repos after entity_ids table is created
        initializeRepos();
    }

    void DatabaseManager::createEntityIdsTable(Database& db) {
        // Create table to track last used ID for each entity type
        // Single row with separate columns for each entity type
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS entity_ids (
                profile_last_id INTEGER NOT NULL DEFAULT 0,
                group_last_id INTEGER NOT NULL DEFAULT 0,
                route_profile_last_id INTEGER NOT NULL DEFAULT 0
            )
        )");
        
        // Initialize entity IDs if table is empty (insert a single row with all zeros)
        auto checkQuery = db.query("SELECT COUNT(*) FROM entity_ids");
        int count = 0;
        if (checkQuery && checkQuery->executeStep()) {
            count = checkQuery->getColumn(0).getInt();
        }
        
        if (count == 0) {
            db.exec(R"(
                INSERT INTO entity_ids (profile_last_id, group_last_id, route_profile_last_id)
                VALUES (0, 0, 0)
            )");
        }
    }
    
    void DatabaseManager::initializeRepos() {
        profilesRepo = std::make_unique<ProfilesRepo>(db);
        groupsRepo = std::make_unique<GroupsRepo>(db);
        routesRepo = std::make_unique<RoutesRepo>(db);
        settingsRepo = std::make_unique<SettingsRepo>(db);
    }
}
