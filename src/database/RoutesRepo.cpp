#include "include/database/RoutesRepo.h"
#include "include/database/GroupsRepo.h"
#include "include/global/Configs.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutexLocker>
#include <QSet>

namespace Configs {
    namespace {
        // ruleFromRow reads this order: the hand-written columns, then the attribute table's in table order.
        const std::string& routesRepoRuleColumns() {
            static const std::string columns = [] {
                std::string out = "name, type, outbound_id, action, sniffers_json, disabled";
                for (const auto& attr : RouteRule::attribute_table()) {
                    if (attr.column != nullptr) out += std::string(", ") + attr.column;
                }
                return out;
            }();
            return columns;
        }

        const std::string& routesRepoRuleInsert() {
            static const std::string sql = [] {
                std::string values = "?, ?, ?, ?, ?, ?, ?, ?";
                for (const auto& attr : RouteRule::attribute_table()) {
                    if (attr.column != nullptr) values += ", ?";
                }
                return "INSERT INTO route_rules (route_profile_id, rule_order, " + routesRepoRuleColumns() + ") VALUES (" + values + ")";
            }();
            return sql;
        }

        std::string routesRepoListJson(const QList<QString>& list) {
            return QString::fromUtf8(QJsonDocument(QListStr2QJsonArray(list)).toJson(QJsonDocument::Compact)).toStdString();
        }

        QList<QString> routesRepoParseList(const std::string& s) {
            QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(s).toUtf8());
            return QJsonArray2QListString(doc.isArray() ? doc.array() : QJsonArray());
        }

        // NULL: written before rule origins were kept, or restored from such a backup.
        std::optional<QSet<int>> routesRepoRemoteIndexes(const SQLite::Column& column) {
            if (column.isNull()) return std::nullopt;
            QSet<int> indexes;
            for (const auto& value : QJsonDocument::fromJson(QByteArray(column.getText())).array()) {
                if (value.isDouble()) indexes.insert(value.toInt());
            }
            return indexes;
        }

        // Untracked, a remote-sourced profile's rules all came from its source, as updates used to replace them all.
        void routesRepoMarkRemote(RouteProfile& profile, const std::optional<QSet<int>>& indexes) {
            const bool sourced = profile.isRemote || profile.source_gid >= 0;
            for (int i = 0; i < profile.Rules.size(); ++i) {
                auto& rule = *profile.Rules[i];
                rule.remote = indexes ? indexes->contains(i) : sourced && rule.type != endpointPreferredBy;
            }
        }
    }

    RoutesRepo::RoutesRepo(Database& database) : db(database) {
        createTables();
    }

    void RoutesRepo::createTables() const {
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS route_profiles (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL DEFAULT '',
                default_outbound_id INTEGER NOT NULL DEFAULT -1,
                is_raw INTEGER NOT NULL DEFAULT 0,
                raw_route TEXT NOT NULL DEFAULT '',
                prevent_modifications INTEGER NOT NULL DEFAULT 0,
                is_remote INTEGER NOT NULL DEFAULT 0,
                remote_url TEXT NOT NULL DEFAULT '',
                auto_update INTEGER NOT NULL DEFAULT 0,
                remote_last_update INTEGER NOT NULL DEFAULT 0,
                endpoint_profile_ids TEXT NOT NULL DEFAULT '[]',
                inner_hop_endpoint_ids TEXT NOT NULL DEFAULT '[]',
                dns_json TEXT NOT NULL DEFAULT '',
                custom_dns INTEGER NOT NULL DEFAULT 0,
                source_gid INTEGER NOT NULL DEFAULT -1,
                remote_rules TEXT,
                display_order INTEGER NOT NULL DEFAULT 0,
                created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
                updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
            )
        )");
        if (!routeProfilesColumnExists("is_raw"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN is_raw INTEGER NOT NULL DEFAULT 0");
        if (!routeProfilesColumnExists("raw_route"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN raw_route TEXT NOT NULL DEFAULT ''");
        if (!routeProfilesColumnExists("prevent_modifications"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN prevent_modifications INTEGER NOT NULL DEFAULT 0");
        if (!routeProfilesColumnExists("is_remote"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN is_remote INTEGER NOT NULL DEFAULT 0");
        if (!routeProfilesColumnExists("remote_url"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN remote_url TEXT NOT NULL DEFAULT ''");
        if (!routeProfilesColumnExists("auto_update"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN auto_update INTEGER NOT NULL DEFAULT 0");
        if (!routeProfilesColumnExists("remote_last_update"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN remote_last_update INTEGER NOT NULL DEFAULT 0");
        if (!routeProfilesColumnExists("endpoint_profile_ids"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN endpoint_profile_ids TEXT NOT NULL DEFAULT '[]'");
        if (!routeProfilesColumnExists("inner_hop_endpoint_ids"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN inner_hop_endpoint_ids TEXT NOT NULL DEFAULT '[]'");
        if (!routeProfilesColumnExists("dns_json"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN dns_json TEXT NOT NULL DEFAULT ''");
        if (!routeProfilesColumnExists("custom_dns"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN custom_dns INTEGER NOT NULL DEFAULT 0");
        if (!routeProfilesColumnExists("source_gid"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN source_gid INTEGER NOT NULL DEFAULT -1");
        if (!routeProfilesColumnExists("remote_rules"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN remote_rules TEXT");
        // Rows from before it all hold 0, so they keep their id order.
        if (!routeProfilesColumnExists("display_order"))
            db.exec("ALTER TABLE route_profiles ADD COLUMN display_order INTEGER NOT NULL DEFAULT 0");

        db.exec(R"(
            CREATE TABLE IF NOT EXISTS route_rules (
                route_profile_id INTEGER NOT NULL,
                rule_order INTEGER NOT NULL,
                name TEXT NOT NULL DEFAULT '',
                type INTEGER NOT NULL DEFAULT 0,
                ip_version TEXT,
                network TEXT,
                protocol TEXT,
                inbound_json TEXT,
                domain_json TEXT,
                domain_suffix_json TEXT,
                domain_keyword_json TEXT,
                domain_regex_json TEXT,
                source_ip_cidr_json TEXT,
                source_ip_is_private INTEGER NOT NULL DEFAULT 0,
                ip_cidr_json TEXT,
                ip_is_private INTEGER NOT NULL DEFAULT 0,
                source_port_json TEXT,
                source_port_range_json TEXT,
                port_json TEXT,
                port_range_json TEXT,
                process_name_json TEXT,
                process_path_json TEXT,
                process_path_regex_json TEXT,
                rule_set_json TEXT,
                invert INTEGER NOT NULL DEFAULT 0,
                outbound_id INTEGER NOT NULL DEFAULT -2,
                action TEXT NOT NULL DEFAULT 'route',
                reject_method TEXT,
                no_drop INTEGER NOT NULL DEFAULT 0,
                override_address TEXT,
                override_port TEXT,
                sniffers_json TEXT,
                sniff_override_dest INTEGER NOT NULL DEFAULT 0,
                strategy TEXT,
                wifi_ssid_json TEXT,
                wifi_bssid_json TEXT,
                tls_spoof TEXT,
                tls_spoof_method TEXT,
                package_name_json TEXT,
                tls_fragment INTEGER NOT NULL DEFAULT 0,
                tls_fragment_fallback_delay TEXT,
                tls_record_fragment INTEGER NOT NULL DEFAULT 0,
                disabled INTEGER NOT NULL DEFAULT 0,
                PRIMARY KEY (route_profile_id, rule_order),
                FOREIGN KEY(route_profile_id) REFERENCES route_profiles(id) ON DELETE CASCADE
            )
        )");
        if (!routeRulesColumnExists("wifi_ssid_json"))
            db.exec("ALTER TABLE route_rules ADD COLUMN wifi_ssid_json TEXT");
        if (!routeRulesColumnExists("wifi_bssid_json"))
            db.exec("ALTER TABLE route_rules ADD COLUMN wifi_bssid_json TEXT");
        if (!routeRulesColumnExists("tls_spoof"))
            db.exec("ALTER TABLE route_rules ADD COLUMN tls_spoof TEXT");
        if (!routeRulesColumnExists("tls_spoof_method"))
            db.exec("ALTER TABLE route_rules ADD COLUMN tls_spoof_method TEXT");
        if (!routeRulesColumnExists("package_name_json"))
            db.exec("ALTER TABLE route_rules ADD COLUMN package_name_json TEXT");
        if (!routeRulesColumnExists("tls_fragment"))
            db.exec("ALTER TABLE route_rules ADD COLUMN tls_fragment INTEGER NOT NULL DEFAULT 0");
        if (!routeRulesColumnExists("tls_fragment_fallback_delay"))
            db.exec("ALTER TABLE route_rules ADD COLUMN tls_fragment_fallback_delay TEXT");
        if (!routeRulesColumnExists("tls_record_fragment"))
            db.exec("ALTER TABLE route_rules ADD COLUMN tls_record_fragment INTEGER NOT NULL DEFAULT 0");
        if (!routeRulesColumnExists("disabled"))
            db.exec("ALTER TABLE route_rules ADD COLUMN disabled INTEGER NOT NULL DEFAULT 0");
    }

    bool RoutesRepo::routeRulesColumnExists(const char* columnName) const {
        auto pragma = db.query("PRAGMA table_info(route_rules)");
        if (!pragma) return false;
        while (pragma->executeStep()) {
            if (pragma->getColumn(1).getText() == std::string(columnName)) return true;
        }
        return false;
    }

    bool RoutesRepo::routeProfilesColumnExists(const char* columnName) const {
        auto pragma = db.query("PRAGMA table_info(route_profiles)");
        if (!pragma) return false;
        while (pragma->executeStep()) {
            if (pragma->getColumn(1).getText() == std::string(columnName)) return true;
        }
        return false;
    }

    std::shared_ptr<RouteProfile> RoutesRepo::routeProfileFromJson(const QJsonObject& json) const {
        auto routeProfile = std::make_shared<RouteProfile>();
        
        routeProfile->id = json["id"].toInt();
        routeProfile->name = json["name"].toString();
        routeProfile->defaultOutboundID = json["defaultOutboundID"].toInt();
        routeProfile->isRaw = json["isRaw"].toBool();
        routeProfile->rawRoute = json["rawRoute"].toString();
        routeProfile->preventModifications = json["preventModifications"].toBool();
        routeProfile->isRemote = json["isRemote"].toBool();
        routeProfile->remoteURL = json["remoteURL"].toString();
        routeProfile->autoUpdate = json["autoUpdate"].toBool();
        routeProfile->remoteLastUpdate = static_cast<qint64>(json["remoteLastUpdate"].toDouble());
        routeProfile->source_gid = json["sourceGid"].toInt(-1);
        for (const auto& endpointValue : json["endpointProfileIDs"].toArray()) {
            if (endpointValue.isDouble()) routeProfile->endpointProfileIDs.append(endpointValue.toInt());
        }
        for (const auto& endpointValue : json["innerHopEndpointIDs"].toArray()) {
            if (endpointValue.isDouble()) routeProfile->innerHopEndpointIDs.append(endpointValue.toInt());
        }
        if (json["dns"].isObject()) routeProfile->dns = RouteDns::FromJson(json["dns"].toObject());
        routeProfile->customDns = json["customDns"].toBool() && routeProfile->dns.has_value();
        return routeProfile;
    }

    void RoutesRepo::saveToDatabase(const RouteProfile* routeProfile, int id) const {
        db.transaction("RoutesRepo::saveToDatabase", [&] {
            saveToDatabaseInTx(routeProfile, id);
            return true;
        });
    }

    void RoutesRepo::saveToDatabaseInTx(const RouteProfile* routeProfile, int id) const {
        QJsonArray endpointsArray;
        for (const int endpointID : routeProfile->endpointProfileIDs) endpointsArray.append(endpointID);
        const QString endpointsJson = QString::fromUtf8(QJsonDocument(endpointsArray).toJson(QJsonDocument::Compact));

        QJsonArray innerHopsArray;
        for (const int endpointID : routeProfile->innerHopEndpointIDs) innerHopsArray.append(endpointID);
        const QString innerHopsJson = QString::fromUtf8(QJsonDocument(innerHopsArray).toJson(QJsonDocument::Compact));

        const QString dnsJson = routeProfile->dns
            ? QString::fromUtf8(QJsonDocument(routeProfile->dns->ToJson()).toJson(QJsonDocument::Compact)) : QString();

        QJsonArray remoteArray;
        for (int i = 0; i < routeProfile->Rules.size(); ++i) {
            if (routeProfile->Rules[i]->remote) remoteArray.append(i);
        }
        const QString remoteJson = QString::fromUtf8(QJsonDocument(remoteArray).toJson(QJsonDocument::Compact));

        db.execThrow(R"(
            INSERT INTO route_profiles (id, name, default_outbound_id, is_raw, raw_route, prevent_modifications,
                is_remote, remote_url, auto_update, remote_last_update, endpoint_profile_ids,
                inner_hop_endpoint_ids, dns_json, custom_dns, source_gid, remote_rules, display_order)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
                (SELECT COALESCE(MAX(display_order), -1) + 1 FROM route_profiles))
            ON CONFLICT(id) DO UPDATE SET
                name = excluded.name, default_outbound_id = excluded.default_outbound_id,
                is_raw = excluded.is_raw, raw_route = excluded.raw_route,
                prevent_modifications = excluded.prevent_modifications,
                is_remote = excluded.is_remote, remote_url = excluded.remote_url,
                auto_update = excluded.auto_update, remote_last_update = excluded.remote_last_update,
                endpoint_profile_ids = excluded.endpoint_profile_ids,
                inner_hop_endpoint_ids = excluded.inner_hop_endpoint_ids,
                dns_json = excluded.dns_json, custom_dns = excluded.custom_dns,
                source_gid = excluded.source_gid, remote_rules = excluded.remote_rules,
                updated_at = strftime('%s', 'now')
        )",
            id,
            routeProfile->name.toStdString(),
            routeProfile->defaultOutboundID,
            routeProfile->isRaw ? 1 : 0,
            routeProfile->rawRoute.toStdString(),
            routeProfile->preventModifications ? 1 : 0,
            routeProfile->isRemote ? 1 : 0,
            routeProfile->remoteURL.toStdString(),
            routeProfile->autoUpdate ? 1 : 0,
            static_cast<long long>(routeProfile->remoteLastUpdate),
            endpointsJson.toStdString(),
            innerHopsJson.toStdString(),
            dnsJson.toStdString(),
            routeProfile->customDns ? 1 : 0,
            routeProfile->source_gid,
            remoteJson.toStdString()
        );

        db.execThrow("DELETE FROM route_rules WHERE route_profile_id = ?", id);

        int ruleOrder = 0;
        for (const auto& rule : routeProfile->Rules) {
            const RouteRule& r = *rule;
            const auto insert = db.queryThrow(routesRepoRuleInsert(), id, ruleOrder++, r.name.toStdString(), r.type, r.outboundID,
                                              r.action.toStdString(), routesRepoListJson(r.sniffers), r.disabled ? 1 : 0);
            // Bound here, as execThrow takes a fixed argument list.
            int index = 8;
            for (const auto& attr : RouteRule::attribute_table()) {
                if (attr.column == nullptr) continue;
                ++index;
                if (const bool* flag = attr.field<bool>(r)) insert->bind(index, static_cast<int64_t>(*flag ? 1 : 0));
                else if (const QString* text = attr.field<QString>(r)) insert->bind(index, text->toStdString());
                else if (const QList<QString>* list = attr.field<QList<QString>>(r)) insert->bind(index, routesRepoListJson(*list));
            }
            insert->exec();
            db.countWrites(1);
        }
    }

    std::shared_ptr<RouteRule> RoutesRepo::ruleFromRow(SQLite::Statement& stmt, int baseCol) const {
        auto rule = std::make_shared<RouteRule>();
        int col = baseCol;
        rule->name = QString::fromStdString(stmt.getColumn(col++).getText());
        rule->type = stmt.getColumn(col++).getInt();
        rule->outboundID = stmt.getColumn(col++).getInt();
        rule->action = QString::fromStdString(stmt.getColumn(col++).getText());
        rule->sniffers = routesRepoParseList(stmt.getColumn(col++).getText());
        // The editor used to switch endpoint rules off by accident; they cannot be disabled.
        rule->disabled = stmt.getColumn(col++).getInt() != 0 && rule->type != endpointPreferredBy;
        for (const auto& attr : RouteRule::attribute_table()) {
            if (attr.column == nullptr) continue;
            const SQLite::Column column = stmt.getColumn(col++);
            if (bool* flag = attr.field<bool>(*rule)) *flag = column.getInt() != 0;
            else if (QString* text = attr.field<QString>(*rule)) *text = QString::fromStdString(column.getText());
            else if (QList<QString>* list = attr.field<QList<QString>>(*rule)) *list = routesRepoParseList(column.getText());
        }
        return rule;
    }

    std::shared_ptr<RouteProfile> RoutesRepo::routeProfileFromProfileRow(SQLite::Statement& stmt) const {
        QJsonObject json;
        json["id"] = stmt.getColumn(0).getInt();
        json["name"] = QString::fromStdString(stmt.getColumn(1).getText());
        json["defaultOutboundID"] = stmt.getColumn(2).getInt();
        json["isRaw"] = stmt.getColumn(3).getInt() != 0;
        json["rawRoute"] = QString::fromStdString(stmt.getColumn(4).getText());
        json["preventModifications"] = stmt.getColumn(5).getInt() != 0;
        json["isRemote"] = stmt.getColumn(6).getInt() != 0;
        json["remoteURL"] = QString::fromStdString(stmt.getColumn(7).getText());
        json["autoUpdate"] = stmt.getColumn(8).getInt() != 0;
        json["remoteLastUpdate"] = static_cast<double>(stmt.getColumn(9).getInt64());
        const auto endpointsDoc = QJsonDocument::fromJson(QString::fromStdString(stmt.getColumn(10).getText()).toUtf8());
        json["endpointProfileIDs"] = endpointsDoc.isArray() ? endpointsDoc.array() : QJsonArray();
        const auto innerHopsDoc = QJsonDocument::fromJson(QString::fromStdString(stmt.getColumn(11).getText()).toUtf8());
        json["innerHopEndpointIDs"] = innerHopsDoc.isArray() ? innerHopsDoc.array() : QJsonArray();
        if (const auto dnsDoc = QJsonDocument::fromJson(QString::fromStdString(stmt.getColumn(12).getText()).toUtf8()); dnsDoc.isObject())
            json["dns"] = dnsDoc.object();
        json["customDns"] = stmt.getColumn(13).getInt() != 0;
        json["sourceGid"] = stmt.getColumn(14).getInt();
        return routeProfileFromJson(json);
    }

    void RoutesRepo::loadRulesForProfileIdsChunk(const QList<int>& profileIds, std::map<int, std::shared_ptr<RouteProfile>>& byId) const {
        if (profileIds.isEmpty()) return;
        QString idList;
        for (int i = 0; i < profileIds.size(); ++i) {
            if (i > 0) idList += ",";
            idList += QString::number(profileIds[i]);
        }
        std::string sql = "SELECT route_profile_id, " + routesRepoRuleColumns() + " FROM route_rules WHERE route_profile_id IN (" +
                          idList.toStdString() + ") ORDER BY route_profile_id, rule_order";
        auto rulesQuery = db.query(sql);
        if (!rulesQuery) return;
        while (rulesQuery->executeStep()) {
            int profileId = rulesQuery->getColumn(0).getInt();
            auto it = byId.find(profileId);
            if (it != byId.end()) {
                it->second->Rules.append(ruleFromRow(*rulesQuery, 1));
            }
        }
    }

    std::shared_ptr<RouteProfile> RoutesRepo::loadFromDatabase(int id) const {
        auto profileQuery = db.query(R"(
            SELECT id, name, default_outbound_id, is_raw, raw_route, prevent_modifications,
                   is_remote, remote_url, auto_update, remote_last_update, endpoint_profile_ids,
                   inner_hop_endpoint_ids, dns_json, custom_dns, source_gid, remote_rules
            FROM route_profiles WHERE id = ?
        )", id);
        if (!profileQuery || !profileQuery->executeStep()) {
            return nullptr;
        }

        auto routeProfile = routeProfileFromProfileRow(*profileQuery);
        const auto remoteIndexes = routesRepoRemoteIndexes(profileQuery->getColumn(15));

        auto rulesQuery = db.query("SELECT " + routesRepoRuleColumns() + " FROM route_rules WHERE route_profile_id = ? ORDER BY rule_order", id);
        if (rulesQuery) {
            while (rulesQuery->executeStep()) {
                routeProfile->Rules.append(ruleFromRow(*rulesQuery, 0));
            }
        }
        routesRepoMarkRemote(*routeProfile, remoteIndexes);

        return routeProfile;
    }

    std::shared_ptr<RouteProfile> RoutesRepo::NewRouteProfile() {
        return std::make_shared<RouteProfile>();
    }

    bool RoutesRepo::AddRouteProfile(std::shared_ptr<RouteProfile>& routeProfile) {
        if (routeProfile->id >= 0) return false;
        int newId = NewRouteProfileID();
        routeProfile->id = newId;
        QMutexLocker locker(&mutex);
        identityMap[newId] = std::weak_ptr<RouteProfile>(routeProfile);
        saveToDatabase(routeProfile.get(), routeProfile->id);
        ++revision;
        return true;
    }

    std::shared_ptr<RouteProfile> RoutesRepo::GetRouteProfile(int id) const {
        QMutexLocker locker(&mutex);
        if (auto it = identityMap.find(id); it != identityMap.end()) {
            if (auto shared = it->second.lock()) return shared;
            identityMap.erase(it);
        }
        auto routeProfile = loadFromDatabase(id);
        if (!routeProfile) return nullptr;
        identityMap[id] = std::weak_ptr<RouteProfile>(routeProfile);
        return routeProfile;
    }

    std::optional<RouteDns> RoutesRepo::GetCustomDns(int id) const {
        QMutexLocker locker(&mutex);
        if (auto it = identityMap.find(id); it != identityMap.end()) {
            if (auto shared = it->second.lock()) return shared->ActiveDns();
        }
        auto query = db.query("SELECT custom_dns, dns_json FROM route_profiles WHERE id = ?", id);
        if (!query || !query->executeStep() || query->getColumn(0).getInt() == 0) return std::nullopt;
        const auto doc = QJsonDocument::fromJson(QString::fromStdString(query->getColumn(1).getText()).toUtf8());
        if (!doc.isObject()) return std::nullopt;
        return RouteDns::FromJson(doc.object());
    }

    std::optional<QString> RoutesRepo::GetRouteProfileName(int id) const {
        QMutexLocker locker(&mutex);
        if (auto it = identityMap.find(id); it != identityMap.end()) {
            if (auto shared = it->second.lock()) return shared->name;
        }
        auto query = db.query("SELECT name FROM route_profiles WHERE id = ?", id);
        if (!query || !query->executeStep()) return std::nullopt;
        return QString::fromStdString(query->getColumn(0).getText());
    }

    bool RoutesRepo::HasRouteProfile(int id) const {
        if (id < 0) return false;
        QMutexLocker locker(&mutex);
        if (auto it = identityMap.find(id); it != identityMap.end() && !it->second.expired()) return true;
        auto query = db.query("SELECT 1 FROM route_profiles WHERE id = ?", id);
        return query && query->executeStep();
    }

    QList<int> RoutesRepo::OwnedBy(int gid) const {
        QList<int> ids;
        if (gid < 0) return ids;
        auto query = db.query("SELECT id FROM route_profiles WHERE source_gid = ? ORDER BY id", gid);
        if (query) {
            while (query->executeStep()) ids.append(query->getColumn(0).getInt());
        }
        return ids;
    }

    void RoutesRepo::DeleteRouteProfile(int id) {
        DeleteRouteProfiles({id});
    }

    void RoutesRepo::DeleteRouteProfiles(const QList<int>& ids) {
        if (ids.isEmpty()) return;
        std::vector<int> doomed(ids.cbegin(), ids.cend());
        QSet<int> owners;
        {
            QMutexLocker locker(&mutex);
            QString idList;
            for (const int id : ids) idList += (idList.isEmpty() ? "" : ",") + QString::number(id);
            if (auto query = db.query("SELECT source_gid FROM route_profiles WHERE id IN (" + idList.toStdString() + ")")) {
                while (query->executeStep()) {
                    if (const int gid = query->getColumn(0).getInt(); gid >= 0) owners.insert(gid);
                }
            }
            for (const int id : ids) identityMap.erase(id);
            db.execDeleteByIdIn("route_profiles", "id", doomed);
            ++revision;
        }
        // Outside the lock: the groups repo takes its own.
        forgetDeleted(QSet<int>(ids.cbegin(), ids.cend()), owners);
    }

    void RoutesRepo::forgetDeleted(const QSet<int>& deleted, const QSet<int>& owners) {
        auto& groupsRepo = dataManager->groupsRepo;
        for (const int gid : groupsRepo->GetAllGroupIds()) {
            const auto group = groupsRepo->GetGroup(gid);
            if (group == nullptr) continue;
            bool changed = false;
            if (deleted.contains(group->route_id)) {
                group->route_id = -1;
                changed = true;
                MW_show_log(QStringLiteral("[Groups] %1: routing profile reset to Default (its profile was deleted)").arg(group->name));
            }
            if (owners.contains(gid) && group->import_sub_route) {
                group->import_sub_route = false;
                changed = true;
                MW_show_log(QStringLiteral("[Groups] %1: stopped importing the subscription's routing profile (it was deleted)").arg(group->name));
            }
            if (changed) groupsRepo->Save(group);
        }

        auto& settings = dataManager->settingsRepo;
        if (!deleted.contains(settings->current_route_id)) return;
        const auto remaining = GetAllRouteProfileIds();
        if (!remaining.isEmpty()) {
            settings->current_route_id = remaining.first();
        } else {
            auto fallback = RouteProfile::GetDefaultChain();
            AddRouteProfile(fallback);
            settings->current_route_id = fallback->id;
        }
        settings->Save();
    }

    void RoutesRepo::UpdateRouteProfiles(const QList<std::shared_ptr<RouteProfile>>& routeProfiles, const QList<int>& removedIds) {
        QSet<int> kept;
        {
            QMutexLocker locker(&mutex);
            for (const auto& routeProfile : routeProfiles) {
                if (routeProfile->id < 0) {
                    routeProfile->id = NewRouteProfileID();
                }
                kept.insert(routeProfile->id);
                saveToDatabase(routeProfile.get(), routeProfile->id);
                identityMap[routeProfile->id] = std::weak_ptr<RouteProfile>(routeProfile);
            }
            ++revision;
        }
        QList<int> doomed;
        for (const int id : removedIds) {
            if (id >= 0 && !kept.contains(id) && !doomed.contains(id)) doomed << id;
        }
        DeleteRouteProfiles(doomed);
    }

    QList<int> RoutesRepo::GetAllRouteProfileIds() const {
        QList<int> ids;
        auto query = db.query("SELECT id FROM route_profiles ORDER BY display_order, id");
        if (query) {
            while (query->executeStep()) {
                ids.append(query->getColumn(0).getInt());
            }
        }
        return ids;
    }

    bool RoutesRepo::SetRouteProfilesOrder(const QList<int>& ids) {
        QMutexLocker locker(&mutex);
        const QList<int> current = GetAllRouteProfileIds();
        QList<int> order;
        for (const int id : ids) {
            if (current.contains(id) && !order.contains(id)) order << id;
        }
        for (const int id : current) {
            if (!order.contains(id)) order << id;
        }
        if (order == current) return false;
        const bool saved = db.transaction("RoutesRepo::SetRouteProfilesOrder", [&] {
            for (int position = 0; position < order.size(); ++position) {
                db.execThrow("UPDATE route_profiles SET display_order = ? WHERE id = ?", position, order[position]);
            }
            return true;
        });
        if (saved) ++revision;
        return saved;
    }

    QList<std::shared_ptr<RouteProfile>> RoutesRepo::GetAllRouteProfiles() const {
        QList<std::shared_ptr<RouteProfile>> routeProfiles;
        std::map<int, std::shared_ptr<RouteProfile>> byId;
        QList<int> idsInOrder;
        QSet<int> cachedProfiles;
        std::map<int, std::optional<QSet<int>>> remoteIndexes;

        auto profileQuery = db.query("SELECT id, name, default_outbound_id, is_raw, raw_route, prevent_modifications, is_remote, remote_url, auto_update, remote_last_update, endpoint_profile_ids, inner_hop_endpoint_ids, dns_json, custom_dns, source_gid, remote_rules FROM route_profiles ORDER BY display_order, id");
        if (!profileQuery) return routeProfiles;

        QMutexLocker locker(&mutex);
        while (profileQuery->executeStep()) {
            int id = profileQuery->getColumn(0).getInt();
            std::shared_ptr<RouteProfile> profile;
            auto it = identityMap.find(id);
            if (it != identityMap.end()) {
                if (auto shared = it->second.lock()) {
                    byId[id] = shared;
                    idsInOrder.append(id);
                    cachedProfiles.insert(id);
                    continue;
                }
                identityMap.erase(it);
            }
            profile = routeProfileFromProfileRow(*profileQuery);
            remoteIndexes[id] = routesRepoRemoteIndexes(profileQuery->getColumn(15));
            byId[id] = profile;
            idsInOrder.append(id);
            identityMap[id] = std::weak_ptr<RouteProfile>(profile);
        }

        if (byId.empty()) return routeProfiles;

        for (int off = 0; off < idsInOrder.size(); off += Configs::BATCH_LIMIT_READ) {
            int end = std::min(off + Configs::BATCH_LIMIT_READ, static_cast<int>(idsInOrder.size()));
            QList<int> chunk;
            for (int i = off; i < end; ++i) {
                if (!cachedProfiles.contains(idsInOrder[i])) chunk.append(idsInOrder[i]);
            }
            if (chunk.isEmpty()) continue;
            loadRulesForProfileIdsChunk(chunk, byId);
        }
        for (const auto& [id, indexes] : remoteIndexes) routesRepoMarkRemote(*byId[id], indexes);

        for (int id : idsInOrder) {
            routeProfiles.append(byId[id]);
        }
        return routeProfiles;
    }

    int RoutesRepo::NewRouteProfileID() const {
        auto query = db.query("UPDATE entity_ids SET route_profile_last_id = route_profile_last_id + 1 RETURNING route_profile_last_id");
        if (query && query->executeStep()) {
            return query->getColumn(0).getInt();
        }
        
        return 0;
    }

    bool RoutesRepo::Save(const std::shared_ptr<RouteProfile>& routeProfile) {
        if (!routeProfile) {
            return false;
        }
        
        if (routeProfile->id < 0) {
            return false;
        }
        
        QMutexLocker locker(&mutex);
        saveToDatabase(routeProfile.get(), routeProfile->id);
        identityMap[routeProfile->id] = std::weak_ptr<RouteProfile>(routeProfile);
        ++revision;
        
        return true;
    }
}
