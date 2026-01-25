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
            if (ent == nullptr || ent->outbound == nullptr || ent->outbound->invalid) {
                delProfile << id;
                continue;
            }
            profiles[id] = ent;
            if (ent->type == "extracore") extraCorePaths.insert(ent->ExtraCore()->extraCorePath);
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
        QList<int> orphanProfiles;
        for (const auto& [id, proxy] : profiles)
        {
            // corrupted data
            if (!groups.contains(proxy->gid) || (!needToCheckGroups.contains(proxy->gid) && !groups[proxy->gid]->HasProfile(id))) {
                orphanProfiles << id;
                continue;
            }
            if (needToCheckGroups.contains(proxy->gid)) groups[proxy->gid]->AddProfile(id);
        }
        for (int id : orphanProfiles) deleteProfile(id);
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

        // First setup
        if (routes.empty()) {
            auto defaultRoute = RoutingChain::GetDefaultChain();
            profileManager->AddRouteChain(defaultRoute);
        }
    }

    void ProfileManager::SaveManager() {
        JsonStore::Save();
    }

    void migrateBeanToOutbound(const std::shared_ptr<ProxyEntity> &ent)
    {
        if (ent->type == "chain")
        {
            auto bean = dynamic_cast<Configs::ChainBean*>(ent->_bean.get());
            if (bean == nullptr)
            {
                qDebug() << "Broken state in migrate for chain";
                return;
            }
            auto chain = ent->Chain();
            if (chain == nullptr)
            {
                qDebug() << "Invalid state in migrate for chain";
                return;
            }
            chain->name = bean->name;
            chain->list = bean->list;
        } else if (ent->type == "custom")
        {
            auto bean = dynamic_cast<Configs::CustomBean*>(ent->_bean.get());
            if (bean == nullptr)
            {
                qDebug() << "Broken state in migrate for custom";
                return;
            }
            auto custom = ent->Custom();
            if (custom == nullptr)
            {
                qDebug() << "Invalid state in migrate for custom";
                return;
            }
            custom->name = bean->name;
            custom->config = bean->config_simple;
            custom->type = bean->core == "internal" ? "outbound" : "fullconfig";
        } else if (ent->type == "extracore")
        {
            auto bean = dynamic_cast<Configs::ExtraCoreBean*>(ent->_bean.get());
            if (bean == nullptr)
            {
                qDebug() << "Broken state in migrate for extracore";
                return;
            }
            auto extraCore = ent->ExtraCore();
            if (extraCore == nullptr)
            {
                qDebug() << "Invalid state in migrate for extracore";
                return;
            }
            extraCore->name = bean->name;
            extraCore->socksAddress = bean->socksAddress;
            extraCore->socksPort = bean->socksPort;
            extraCore->extraCorePath = bean->extraCorePath;
            extraCore->extraCoreArgs = bean->extraCoreArgs;
            extraCore->extraCoreConf = bean->extraCoreConf;
            extraCore->noLogs = bean->noLogs;
        } else
        {
            auto beanJson = ent->_bean->BuildCoreObjSingBox();
            auto obj = beanJson.outbound;
            if (ent->_bean->mux_state == 0) obj["multiplex"] = QJsonObject{};
            else if (ent->_bean->mux_state == 1) obj["multiplex"] = QJsonObject{{"enabled", true}};
            else if (ent->_bean->mux_state == 2) obj["multiplex"] = QJsonObject{{"enabled", false}};
            ent->outbound->ParseFromJson(obj);
            ent->outbound->name = ent->_bean->name;
            if (ent->type == "hysteria2") {
                ent->type = "hysteria";
                ent->Hysteria()->protocol_version = "2";
            }
        }
    }

    std::shared_ptr<ProxyEntity> ProfileManager::LoadProxyEntity(const QString &jsonPath) {
        // Load type
        auto entFile = QFile(jsonPath);
        bool ok = entFile.open(QIODevice::ReadOnly);
        if (!ok) {
            return nullptr;
        }
        QString fileContent = entFile.readAll();
        entFile.close();
        if (fileContent.isEmpty()) {
            return nullptr;
        }
        ProxyEntity ent0(nullptr, nullptr, nullptr);
        ent0.fn = jsonPath;
        auto validJson = ent0.Load(fileContent);
        auto type = ent0.type;

        // Load content
        std::shared_ptr<ProxyEntity> ent;
        bool validType = validJson;

        if (validType) {
            ent = NewProxyEntity(type);
            validType = !ent->outbound->invalid;
        }

        if (validType) {
            ent->load_control_must = true;
            ent->fn = jsonPath;
            ent->Load(fileContent);
            ent->_remove("bean");
        }

        if (!QString2QJsonObject(QString(fileContent.toStdString().c_str())).contains("outbound"))
        {
            migrateBeanToOutbound(ent);
            ent->Save();
        }
        ent->_bean = nullptr;
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

    std::shared_ptr<ProxyEntity> ProfileManager::NewProxyEntity(const QString &type) {
        Configs::AbstractBean *bean;
        Configs::outbound *outbound;

        if (type == "socks") {
            bean = new Configs::SocksHttpBean(Configs::SocksHttpBean::type_Socks5);
            outbound = new Configs::socks();
        } else if (type == "http") {
            bean = new Configs::SocksHttpBean(Configs::SocksHttpBean::type_HTTP);
            outbound = new Configs::http();
        } else if (type == "shadowsocks") {
            bean = new Configs::ShadowSocksBean();
            outbound = new Configs::shadowsocks();
        } else if (type == "chain") {
            bean = new Configs::ChainBean();
            outbound = new Configs::chain();
        } else if (type == "vmess") {
            bean = new Configs::VMessBean();
            outbound = new Configs::vmess();
        } else if (type == "trojan") {
            bean = new Configs::TrojanVLESSBean(Configs::TrojanVLESSBean::proxy_Trojan);
            outbound = new Configs::Trojan();
        } else if (type == "vless") {
            bean = new Configs::TrojanVLESSBean(Configs::TrojanVLESSBean::proxy_VLESS);
            outbound = new Configs::vless();
        } else if (type == "xrayvless") {
            bean = new Configs::TrojanVLESSBean(Configs::TrojanVLESSBean::proxy_VLESS);
            outbound = new Configs::xrayVless();
        } else if (type == "hysteria" || type == "hysteria2") {
            bean = new Configs::QUICBean(type == "hysteria" ? Configs::QUICBean::proxy_Hysteria : Configs::QUICBean::proxy_Hysteria2);
            outbound = new Configs::hysteria();
        } else if (type == "tuic") {
            bean = new Configs::QUICBean(Configs::QUICBean::proxy_TUIC);
            outbound = new Configs::tuic();
        } else if (type == "anytls") {
            bean = new Configs::AnyTLSBean();
            outbound = new Configs::anyTLS();
        } else if (type == "wireguard") {
            bean = new Configs::WireguardBean(Configs::WireguardBean());
            outbound = new Configs::wireguard();
        } else if (type == "tailscale") {
            bean = new Configs::TailscaleBean(Configs::TailscaleBean());
            outbound = new Configs::tailscale();
        } else if (type == "ssh") {
            bean = new Configs::SSHBean(Configs::SSHBean());
            outbound = new Configs::ssh();
        } else if (type == "custom") {
            bean = new Configs::CustomBean();
            outbound = new Configs::Custom();
        } else if (type == "extracore") {
            bean = new Configs::ExtraCoreBean();
            outbound = new Configs::extracore();
        } else {
            bean = new Configs::AbstractBean(-114514);
            outbound = new Configs::outbound();
            outbound->invalid = true;
        }

        auto ent = std::make_shared<ProxyEntity>(outbound, bean, type);
        return ent;
    }

    std::shared_ptr<GroupDeprecated> ProfileManager::NewGroup() {
        auto ent = std::make_shared<GroupDeprecated>();
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

    bool ProfileManager::AddProfileBatch(const QList<std::shared_ptr<ProxyEntity>>& ents, int gid)
    {
        gid = gid < 0 ? dataStore->current_group : gid;
        auto group = GetGroup(gid);
        if (group == nullptr) return false;
        for (const auto& ent : ents)
        {
            if (ent->id >= 0) continue;
            ent->id = NewProfileID();
            ent->gid = gid;
            group->AddProfile(ent->id);
            profiles[ent->id] = ent;
            profilesIdOrder.push_back(ent->id);
            ent->fn = QString("profiles/%1.json").arg(ent->id);
        }
        group->Save();
        runOnNewThread([=,this]
        {
           for (const auto& ent : ents) ent->Save();
        });
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

    void ProfileManager::BatchDeleteProfiles(const QList<int>& ids)
    {
        QSet<std::shared_ptr<GroupDeprecated>> changed_groups;
        QSet<int> deleted_ids;
        for (auto id : ids)
        {
            if (id < 0) continue;
            if (dataStore->started_id == id) continue;
            auto ent = GetProfile(id);
            if (ent == nullptr) continue;
            if (auto group = GetGroup(ent->gid); group != nullptr)
            {
                group->RemoveProfile(id);
                changed_groups.insert(group);
            }
            profiles.erase(id);
            deleted_ids.insert(id);
        }
        for (const auto& group : changed_groups) group->Save();
        QList<int> newOrder;
        for (auto id : profilesIdOrder)
        {
            if (deleted_ids.contains(id)) continue;
            newOrder.append(id);
        }
        profilesIdOrder = newOrder;

        runOnNewThread([=,this]
        {
           for (int id : deleted_ids) QFile(QString("profiles/%1.json").arg(id)).remove();
        });
    }


    void ProfileManager::deleteProfile(int id)
    {
        profiles.erase(id);
        profilesIdOrder.removeAll(id);
        QFile(QString("profiles/%1.json").arg(id)).remove();
    }


    std::shared_ptr<ProxyEntity> ProfileManager::GetProfile(int id) {
        return profiles.contains(id) ? profiles[id] : nullptr;
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

    std::shared_ptr<GroupDeprecated> ProfileManager::LoadGroup(const QString &jsonPath) {
        auto ent = std::make_shared<GroupDeprecated>();
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

    bool ProfileManager::AddGroup(const std::shared_ptr<GroupDeprecated> &ent) {
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

    std::shared_ptr<GroupDeprecated> ProfileManager::GetGroup(int id) {
        return groups.count(id) ? groups[id] : nullptr;
    }

    std::shared_ptr<GroupDeprecated> ProfileManager::CurrentGroup() {
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