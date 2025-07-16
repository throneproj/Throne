#include "include/dataStore/Database.hpp"

#include "include/configs/proxy/includes.h"

#include <QDir>

namespace Configs {

    ProfileManager *profileManager = new ProfileManager();

    ProfileManager::ProfileManager() : JsonStore("groups/pm.json") {
        _add(new configItem("groups", &groupsTabOrder, itemType::integerList));
    }

    QList<int> filterIntJsonFile(const QString &path) {
        QList<int> result;
        QDir dr(path);
        auto entryList = dr.entryList(QDir::Files);
        for (auto e: entryList) {
            e = e.toLower();
            if (!e.endsWith(".json", Qt::CaseInsensitive)) continue;
            e = e.remove(".json", Qt::CaseInsensitive);
            bool ok;
            auto id = e.toInt(&ok);
            if (ok) {
                result << id;
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    void ProfileManager::LoadManager() {
        JsonStore::Load();
        //
        profiles = {};
        groups = {};
        routes = {};
        profilesIdOrder = filterIntJsonFile("profiles");
        groupsIdOrder = filterIntJsonFile("groups");
        routesIdOrder = filterIntJsonFile("route_profiles");
        // Load Proxys
        QList<int> delProfile;
        for (auto id: profilesIdOrder) {
            auto ent = LoadProxyEntity(QString("profiles/%1.json").arg(id));
            // Corrupted profile?
            if (ent == nullptr || ent->bean == nullptr || ent->bean->version == -114514) {
                delProfile << id;
                continue;
            }
            profiles[id] = ent;
            if (ent->type == "extracore") extraCorePaths.insert(ent->ExtraCoreBean()->extraCorePath);
        }
        // Clear Corrupted profile
        for (auto id: delProfile) {
            deleteProfile(id);
        }
        // Load Groups
        auto loadedOrder = groupsTabOrder;
        groupsTabOrder = {};
        auto needToCheckGroups = QSet<int>();
        for (auto id: groupsIdOrder) {
            auto ent = LoadGroup(QString("groups/%1.json").arg(id));
            // Corrupted group?
            if (ent->id != id) {
                continue;
            }
            // Ensure order contains every group
            if (!loadedOrder.contains(id)) {
                loadedOrder << id;
            }
            groups[id] = ent;
            if (ent->profiles.isEmpty()) needToCheckGroups << id;
        }
        for (const auto& [id, proxy] : profiles)
        {
            // corrupted data
            if (groups.count(proxy->gid) < 1 || !needToCheckGroups.contains(proxy->gid)) continue;
            groups[proxy->gid]->AddProfile(id);
        }
        for (const auto groupID : needToCheckGroups)
        {
            groups[groupID]->Save();
        }
        // Ensure groups contains order
        for (auto id: loadedOrder) {
            if (groups.count(id)) {
                groupsTabOrder << id;
            }
        }
        // Load Routing profiles
        for (auto id : routesIdOrder) {
            auto route = LoadRouteChain(QString("route_profiles/%1.json").arg(id));
            if (route == nullptr) {
                MW_show_log(QString("File route_profiles/%1.json is corrupted, consider manually handling it").arg(id));
                continue;
            }

            routes[id] = route;
        }

        // First setup
        if (groups.empty()) {
            auto defaultGroup = NewGroup();
            defaultGroup->name = QObject::tr("Default");
            profileManager->AddGroup(defaultGroup);
        }

        if (routes.empty()) {
            auto defaultRoute = RoutingChain::GetDefaultChain();
            profileManager->AddRouteChain(defaultRoute);
            routes[IranBypassChainID] = RoutingChain::GetIranDefaultChain();
            routes[ChinaBypassChainID] = RoutingChain::GetChinaDefaultChain();
        }
    }

    void ProfileManager::SaveManager() {
        JsonStore::Save();
    }

    std::shared_ptr<ProxyEntity> ProfileManager::LoadProxyEntity(const QString &jsonPath) {
        // Load type
        ProxyEntity ent0(nullptr, nullptr);
        ent0.fn = jsonPath;
        auto validJson = ent0.Load();
        auto type = ent0.type;

        // Load content
        std::shared_ptr<ProxyEntity> ent;
        bool validType = validJson;

        if (validType) {
            ent = NewProxyEntity(type);
            validType = ent->bean->version != -114514;
        }

        if (validType) {
            ent->load_control_must = true;
            ent->fn = jsonPath;
            ent->Load();
        }
        return ent;
    }

    std::shared_ptr<RoutingChain> ProfileManager::LoadRouteChain(const QString &jsonPath) {
        auto routingChain = std::make_shared<RoutingChain>();
        routingChain->fn = jsonPath;
        if (!routingChain->Load()) {
            return nullptr;
        }

        return routingChain;
    }

    //  新建的不给 fn 和 id

    std::shared_ptr<ProxyEntity> ProfileManager::NewProxyEntity(const QString &type) {
        Configs::AbstractBean *bean;

        if (type == "socks") {
            bean = new Configs::SocksHttpBean(Configs::SocksHttpBean::type_Socks5);
        } else if (type == "http") {
            bean = new Configs::SocksHttpBean(Configs::SocksHttpBean::type_HTTP);
        } else if (type == "shadowsocks") {
            bean = new Configs::ShadowSocksBean();
        } else if (type == "chain") {
            bean = new Configs::ChainBean();
        } else if (type == "vmess") {
            bean = new Configs::VMessBean();
        } else if (type == "trojan") {
            bean = new Configs::TrojanVLESSBean(Configs::TrojanVLESSBean::proxy_Trojan);
        } else if (type == "vless") {
            bean = new Configs::TrojanVLESSBean(Configs::TrojanVLESSBean::proxy_VLESS);
        } else if (type == "hysteria") {
            bean = new Configs::QUICBean(Configs::QUICBean::proxy_Hysteria);
        } else if (type == "hysteria2") {
            bean = new Configs::QUICBean(Configs::QUICBean::proxy_Hysteria2);
        } else if (type == "tuic") {
            bean = new Configs::QUICBean(Configs::QUICBean::proxy_TUIC);
        } else if (type == "wireguard") {
            bean = new Configs::WireguardBean(Configs::WireguardBean());
        } else if (type == "ssh") {
            bean = new Configs::SSHBean(Configs::SSHBean());
        } else if (type == "custom") {
            bean = new Configs::CustomBean();
        } else if (type == "extracore") {
            bean = new Configs::ExtraCoreBean();
        } else {
            bean = new Configs::AbstractBean(-114514);
        }

        auto ent = std::make_shared<ProxyEntity>(bean, type);
        return ent;
    }

    std::shared_ptr<Group> ProfileManager::NewGroup() {
        auto ent = std::make_shared<Group>();
        return ent;
    }

    // Profile

    int ProfileManager::NewProfileID() const {
        if (profiles.empty()) {
            return 0;
        } else {
            return profilesIdOrder.last() + 1;
        }
    }

    bool ProfileManager::AddProfile(const std::shared_ptr<ProxyEntity> &ent, int gid) {
        if (ent->id >= 0) {
            return false;
        }

        ent->id = NewProfileID();
        ent->gid = gid < 0 ? dataStore->current_group : gid;
        if (auto group = GetGroup(ent->gid); group != nullptr)
        {
            group->AddProfile(ent->id);
            group->Save();
        } else
        {
            return false;
        }
        profiles[ent->id] = ent;
        profilesIdOrder.push_back(ent->id);

        ent->fn = QString("profiles/%1.json").arg(ent->id);
        ent->Save();
        return true;
    }

    void ProfileManager::DeleteProfile(int id) {
        if (id < 0) return;
        if (dataStore->started_id == id) return;
        auto ent = GetProfile(id);
        if (ent == nullptr) return;
        if (auto group = GetGroup(ent->gid); group != nullptr)
        {
            group->RemoveProfile(id);
            group->Save();
        }
        deleteProfile(id);
    }

    void ProfileManager::deleteProfile(int id)
    {
        profiles.erase(id);
        profilesIdOrder.removeAll(id);
        QFile(QString("profiles/%1.json").arg(id)).remove();
    }


    std::shared_ptr<ProxyEntity> ProfileManager::GetProfile(int id) {
        return profiles.count(id) ? profiles[id] : nullptr;
    }

    QStringList ProfileManager::GetExtraCorePaths() const {
        return extraCorePaths.values();
    }

    bool ProfileManager::AddExtraCorePath(const QString &path)
    {
        if (extraCorePaths.contains(path))
        {
            return false;
        }
        extraCorePaths.insert(path);
        return true;
    }
    // Group

    std::shared_ptr<Group> ProfileManager::LoadGroup(const QString &jsonPath) {
        auto ent = std::make_shared<Group>();
        ent->fn = jsonPath;
        ent->Load();
        return ent;
    }

    int ProfileManager::NewGroupID() const {
        if (groups.empty()) {
            return 0;
        } else {
            return groupsIdOrder.last() + 1;
        }
    }

    bool ProfileManager::AddGroup(const std::shared_ptr<Group> &ent) {
        if (ent->id >= 0) {
            return false;
        }

        ent->id = NewGroupID();
        groups[ent->id] = ent;
        groupsIdOrder.push_back(ent->id);
        groupsTabOrder.push_back(ent->id);

        ent->fn = QString("groups/%1.json").arg(ent->id);
        ent->Save();
        return true;
    }

    void ProfileManager::DeleteGroup(int gid) {
        if (groups.size() <= 1) return;
        auto group = GetGroup(gid);
        if (group == nullptr) return;
        for (const auto id : group->Profiles()) {
            deleteProfile(id);
        }
        groups.erase(gid);
        groupsIdOrder.removeAll(gid);
        groupsTabOrder.removeAll(gid);
        QFile(QString("groups/%1.json").arg(gid)).remove();
    }

    std::shared_ptr<Group> ProfileManager::GetGroup(int id) {
        return groups.count(id) ? groups[id] : nullptr;
    }

    std::shared_ptr<Group> ProfileManager::CurrentGroup() {
        return GetGroup(dataStore->current_group);
    }

    std::shared_ptr<RoutingChain> ProfileManager::NewRouteChain() {
        auto route = std::make_shared<RoutingChain>();
        return route;
    }

    int ProfileManager::NewRouteChainID() const {
        if (routes.empty()) {
            return 0;
        }
        return routesIdOrder.last() + 1;
    }

    bool ProfileManager::AddRouteChain(const std::shared_ptr<RoutingChain>& chain) {
        if (chain->id >= 0) {
            return false;
        }

        chain->id = NewRouteChainID();
        chain->fn = QString("route_profiles/%1.json").arg(chain->id);
        routes[chain->id] = chain;
        routesIdOrder.push_back(chain->id);
        chain->Save();

        return true;
    }

    std::shared_ptr<RoutingChain> ProfileManager::GetRouteChain(int id) {
        return routes.count(id) > 0 ? routes[id] : nullptr;
    }

    void ProfileManager::UpdateRouteChains(const QList<std::shared_ptr<RoutingChain>>& newChain) {
        routes.clear();
        routesIdOrder.clear();

        for (const auto &item: newChain) {
            if (!AddRouteChain(item)) {
                routes[item->id] = item;
                routesIdOrder << item->id;
                item->Save();
            }
        }
        auto currFiles = filterIntJsonFile("route_profiles");
        for (const auto &item: currFiles) { // clean up removed route profiles
            if (!routes.count(item)) {
                QFile(QString(ROUTES_PREFIX+"%1.json").arg(item)).remove();
            }
        }
    }

} // namespace Configs