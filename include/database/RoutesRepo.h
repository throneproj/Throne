#pragma once

#include "Database.h"
#include "include/database/entities/RouteProfile.h"
#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <map>
#include <optional>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>
#include <QSet>

namespace Configs {
    class RoutesRepo {
    private:
        Database& db;
        mutable std::mutex mutex;
        mutable std::map<int, std::weak_ptr<RouteProfile>> identityMap;
        std::atomic<quint64> revision{0};

        QJsonObject routeRuleToJson(const RouteRule* rule) const;
        
        std::shared_ptr<RouteRule> routeRuleFromJson(const QJsonObject& json) const;
        
        QJsonObject routeProfileToJson(const RouteProfile* routeProfile) const;
        
        std::shared_ptr<RouteProfile> routeProfileFromJson(const QJsonObject& json) const;
        
        void saveToDatabase(const RouteProfile* routeProfile, int id) const;

        // Throws, so the caller's transaction aborts.
        void saveToDatabaseInTx(const RouteProfile* routeProfile, int id) const;
        
        std::shared_ptr<RouteProfile> loadFromDatabase(int id) const;
        
        // Rules are left empty.
        std::shared_ptr<RouteProfile> routeProfileFromProfileRow(SQLite::Statement& stmt) const;
        
        // baseCol is the column index the rule name sits at.
        QJsonObject ruleJsonFromRow(SQLite::Statement& stmt, int baseCol) const;

        // Appends to byId[profileId]->Rules rather than replacing.
        void loadRulesForProfileIdsChunk(const QList<int>& profileIds, std::map<int, std::shared_ptr<RouteProfile>>& byId) const;
        
        void createTables() const;

        [[nodiscard]] bool routeRulesColumnExists(const char* columnName) const;

        [[nodiscard]] bool routeProfilesColumnExists(const char* columnName) const;

        int NewRouteProfileID() const;

        // Points every group override at the global profile again, stops a deleted profile's subscription from
        // importing, and moves current_route_id off a deleted id.
        void forgetDeleted(const QSet<int>& deleted, const QSet<int>& owners);

    public:
        explicit RoutesRepo(Database& database);
        
        // Not saved yet: id stays -1 until AddRouteProfile.
        [[nodiscard]] static std::shared_ptr<RouteProfile> NewRouteProfile();
        
        bool AddRouteProfile(std::shared_ptr<RouteProfile>& routeProfile);
        
        std::shared_ptr<RouteProfile> GetRouteProfile(int id) const;

        // Without loading the rules; nullopt when the profile follows the global DNS settings or does not exist.
        std::optional<RouteDns> GetCustomDns(int id) const;

        // Without loading it.
        bool HasRouteProfile(int id) const;

        // The routing profiles group gid's subscription imports into (source_gid), lowest id first.
        QList<int> OwnedBy(int gid) const;

        // Every delete resets what pointed at the profile; see forgetDeleted.
        void DeleteRouteProfile(int id);

        void DeleteRouteProfiles(const QList<int>& ids);

        // Saves the list and deletes removedIds; a profile created elsewhere meanwhile is left alone.
        void UpdateRouteProfiles(const QList<std::shared_ptr<RouteProfile>>& routeProfiles, const QList<int>& removedIds);

        QList<int> GetAllRouteProfileIds() const;

        QList<std::shared_ptr<RouteProfile>> GetAllRouteProfiles() const;
        
        // No-op unless id >= 0.
        bool Save(const std::shared_ptr<RouteProfile>& routeProfile);

        // Any thread; moves with every write.
        quint64 Revision() const { return revision.load(); }
    };
}
