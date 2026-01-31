#pragma once

#include "Database.h"
#include "include/database/entities/RouteProfile.h"
#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <memory>
#include <mutex>
#include <map>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>

namespace Configs {
    class RoutesRepo {
    private:
        Database& db;
        mutable std::mutex mutex;
        // Identity map: id -> weak_ptr<RouteProfile>
        mutable std::map<int, std::weak_ptr<RouteProfile>> identityMap;

        // Helper to serialize RouteRule to JSON
        QJsonObject routeRuleToJson(const RouteRule* rule) const;
        
        // Helper to deserialize RouteRule from JSON
        std::shared_ptr<RouteRule> routeRuleFromJson(const QJsonObject& json) const;
        
        // Helper to serialize RouteProfile to JSON
        QJsonObject routeProfileToJson(const RouteProfile* routeProfile) const;
        
        // Helper to deserialize RouteProfile from JSON
        std::shared_ptr<RouteProfile> routeProfileFromJson(const QJsonObject& json) const;
        
        // Save route profile to database (internal helper)
        void saveToDatabase(const RouteProfile* routeProfile, int id) const;
        
        // Load route profile from database (including rules)
        std::shared_ptr<RouteProfile> loadFromDatabase(int id) const;
        
        // Build route profile from current row (id, name, default_outbound_id); rules left empty
        std::shared_ptr<RouteProfile> routeProfileFromProfileRow(SQLite::Statement& stmt) const;
        
        // Build rule JSON from current row; rule columns start at baseCol (0 = name at col 0, 1 = name at col 1)
        QJsonObject ruleJsonFromRow(SQLite::Statement& stmt, int baseCol) const;

        // Load rules for given profile ids (one SELECT IN chunk); appends to byId[profileId]->Rules.
        void loadRulesForProfileIdsChunk(const QList<int>& profileIds, std::map<int, std::shared_ptr<RouteProfile>>& byId) const;
        
        // Create tables if they don't exist
        void createTables() const;

        // Get next available route profile ID
        int NewRouteProfileID() const;

    public:
        explicit RoutesRepo(Database& database);
        
        // Create a new route profile (doesn't save to DB yet, id will be -1)
        [[nodiscard]] static std::shared_ptr<RouteProfile> NewRouteProfile();
        
        // Add route profile to database (assigns ID and saves)
        bool AddRouteProfile(std::shared_ptr<RouteProfile>& routeProfile);
        
        // Get route profile by ID (uses identity map)
        std::shared_ptr<RouteProfile> GetRouteProfile(int id) const;
        
        // Delete route profile from database
        void DeleteRouteProfile(int id);
        
        // Update route profiles (replaces all)
        void UpdateRouteProfiles(const QList<std::shared_ptr<RouteProfile>>& routeProfiles);
        
        // Get all route profile IDs in order
        QList<int> GetAllRouteProfileIds() const;

        QList<std::shared_ptr<RouteProfile>> GetAllRouteProfiles() const;
        
        // Save route profile to database (manual save, like old Save() method)
        // Only saves if route profile has a valid ID (id >= 0)
        bool Save(const std::shared_ptr<RouteProfile>& routeProfile);
    };
}
