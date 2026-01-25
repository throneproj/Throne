#pragma once

#include "Database.h"
#include "include/database/entities/Profile.h"
#include "include/global/Configs.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMutex>

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
        
        // Load profile from database
        std::shared_ptr<Profile> loadFromDatabase(int id) const;
        
        // Create tables if they don't exist
        void createTables() const;

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
        
        // Delete profile from database
        void DeleteProfile(int id);
        
        // Delete multiple profiles
        void BatchDeleteProfiles(const QList<int>& ids);
        
        // Get all profile IDs in order
        QList<int> GetAllProfileIds() const;
        
        // Get next available profile ID
        int NewProfileID() const;
        
        // Save profile to database (manual save, like old Save() method)
        // Only saves if profile has a valid ID (id >= 0)
        bool Save(const std::shared_ptr<Profile>& profile);
    };
}
