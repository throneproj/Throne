#pragma once

#include "Database.h"
#include "include/database/entities/Profile.h"
#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <memory>
#include <mutex>
#include <map>
#include <QString>
#include <QJsonObject>

namespace Configs {
    class ProfilesRepo {
    private:
        Database& db;
        mutable std::mutex mutex;
        // Identity map: id -> weak_ptr<Profile>
        mutable std::map<int, std::weak_ptr<Profile>> identityMap;

        // Helper to serialize Profile to JSON
        QJsonObject profileToJson(const Profile* profile) const;
        
        // Helper to deserialize Profile from JSON
        std::shared_ptr<Profile> profileFromJson(const QJsonObject& json) const;
        
        // Save profile to database (internal helper)
        void saveToDatabase(const Profile* profile, int id) const;

        // Build one row for batch insert (same columns as saveToDatabase)
        ProfileInsertRow profileToInsertRow(const Profile* profile, int id, int gid) const;

        // Load profile from database
        std::shared_ptr<Profile> loadFromDatabase(int id) const;
        
        // Build profile from current row of a SELECT (same columns as loadFromDatabase)
        std::shared_ptr<Profile> profileFromRow(SQLite::Statement& stmt) const;

        // Load profiles for given ids (one SELECT IN chunk). Does not touch identity map.
        std::map<int, std::shared_ptr<Profile>> loadProfilesByIdsChunk(const QList<int>& ids) const;
        
        // Create tables if they don't exist
        void createTables() const;

        // Get next available profile ID (single)
        int NewProfileID() const;
        // Allocate a contiguous block of n IDs; returns first ID (use firstId, firstId+1, ..., firstId+n-1). DB atomic, no lock required.
        int NewProfileIDRange(int n) const;

    public:
        explicit ProfilesRepo(Database& database);
        
        // Create a new profile (doesn't save to DB yet, id will be -1)
        [[nodiscard]] static std::shared_ptr<Profile> NewProfile(const QString &type);
        
        // Add profile to database (assigns ID and saves)
        bool AddProfile(std::shared_ptr<Profile>& profile, int gid = -1);
        
        // Add multiple profiles in batch
        bool AddProfileBatch(QList<std::shared_ptr<Profile>>& profiles, int gid = -1);
        
        // Get profile by ID (uses identity map)
        std::shared_ptr<Profile> GetProfile(int id) const;

        QList<std::shared_ptr<Profile>> GetProfileBatch(QList<int> ids);

        std::shared_ptr<Profile> GetProfileByName(const QString &name);

        QStringList GetAllProfileNames();

        // Delete profile from database
        void DeleteProfile(int id);
        
        // Delete multiple profiles
        void BatchDeleteProfiles(const QList<int>& ids);
        
        // Get all profile IDs in order
        QList<int> GetAllProfileIds() const;
        
        // Save profile to database (manual save, like old Save() method)
        // Only saves if profile has a valid ID (id >= 0)
        bool Save(const std::shared_ptr<Profile>& profile);
    };
}
