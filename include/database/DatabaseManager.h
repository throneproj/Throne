#pragma once

#include "Database.h"
#include "include/database/SettingsRepo.h"
#include <string>
#include <memory>

namespace Configs {
    class RoutesRepo;
    class GroupsRepo;
    class ProfilesRepo;
    class OtpProfilesRepo;
    class TrafficStatsRepo;
    class MarkersRepo;

    void initDB(const std::string& dbPath);

    class DatabaseManager {
    private:
        Database db;
        Database statsDb;

        static void createEntityIdsTable(Database& db);
        static bool entityIdsColumnExists(Database& db, const char* columnName);
        static std::string deriveStatsDbPath(const std::string& dbPath);
        // Quarantines a stats file the previous session flagged, or one that would open read-only or as garbage.
        static std::string prepareStatsDb(const std::string& path);
        static QString statsDbUnusableReason(const std::string& path);
        static void quarantineDbFile(const std::string& path);
        void initializeRepos();
        void applyMigrations();
    public:
        std::unique_ptr<ProfilesRepo> profilesRepo;
        std::unique_ptr<GroupsRepo> groupsRepo;
        std::unique_ptr<RoutesRepo> routesRepo;
        std::unique_ptr<OtpProfilesRepo> otpProfilesRepo;
        std::unique_ptr<SettingsRepo> settingsRepo;
        std::unique_ptr<TrafficStatsRepo> trafficStatsRepo;
        std::unique_ptr<MarkersRepo> markersRepo;

        explicit DatabaseManager(const std::string& dbPath);
        ~DatabaseManager() = default;

        // Call once, after the UI is up.
        void RunDeferredMaintenance();
        
        DatabaseManager(const DatabaseManager&) = delete;
        DatabaseManager& operator=(const DatabaseManager&) = delete;
        
        Database& getDatabase() { return db; }
        const Database& getDatabase() const { return db; }
    };

    inline DatabaseManager* dataManager;
}
