#pragma once

#include "include/global/NekoGui.hpp"
#include "ProxyEntity.hpp"
#include "Group.hpp"
#include "RouteEntity.h"

namespace NekoGui {
    const int INVALID_ID = -99999;

    class ProfileManager : private JsonStore {
    public:
        // JsonStore

        // order -> id
        QList<int> groupsTabOrder;

        // Manager

        std::map<int, std::shared_ptr<ProxyEntity>> profiles;
        std::map<int, std::shared_ptr<Group>> groups;
        std::map<int, std::shared_ptr<RoutingChain>> routes;

        ProfileManager();

        // LoadManager Reset and loads profiles & groups
        void LoadManager();

        void SaveManager();

        [[nodiscard]] static std::shared_ptr<ProxyEntity> NewProxyEntity(const QString &type);

        [[nodiscard]] static std::shared_ptr<Group> NewGroup();

        [[nodiscard]] static std::shared_ptr<RoutingChain> NewRouteChain();

        bool AddProfile(const std::shared_ptr<ProxyEntity> &ent, int gid = -1);

        void DeleteProfile(int id);

        void MoveProfile(const std::shared_ptr<ProxyEntity> &ent, int gid);

        std::shared_ptr<ProxyEntity> GetProfile(int id);

        bool AddGroup(const std::shared_ptr<Group> &ent);

        void DeleteGroup(int gid);

        std::shared_ptr<Group> GetGroup(int id);

        std::shared_ptr<Group> CurrentGroup();

        bool AddRouteChain(const std::shared_ptr<RoutingChain>& chain);

        std::shared_ptr<RoutingChain> GetRouteChain(int id);

        void UpdateRouteChains(const QList<std::shared_ptr<RoutingChain>>& newChain);

        QStringList GetExtraCorePaths() const;

        bool AddExtraCorePath(const QString &path);

    private:
        // sort by id
        QList<int> profilesIdOrder;
        QList<int> groupsIdOrder;
        QList<int> routesIdOrder;
        QSet<QString> extraCorePaths;

        [[nodiscard]] int NewProfileID() const;

        [[nodiscard]] int NewGroupID() const;

        [[nodiscard]] int NewRouteChainID() const;

        static std::shared_ptr<ProxyEntity> LoadProxyEntity(const QString &jsonPath);

        static std::shared_ptr<Group> LoadGroup(const QString &jsonPath);

        static std::shared_ptr<RoutingChain> LoadRouteChain(const QString &jsonPath);
    };

    extern ProfileManager *profileManager;
} // namespace NekoGui
