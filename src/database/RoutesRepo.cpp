#include "include/database/RoutesRepo.h"
#include "include/global/Configs.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutexLocker>
#include <QSet>

namespace Configs {
    RoutesRepo::RoutesRepo(Database& database) : db(database) {
        createTables();
    }

    void RoutesRepo::createTables() const {
        // Create route_profiles table
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS route_profiles (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL DEFAULT '',
                default_outbound_id INTEGER NOT NULL DEFAULT -1,
                created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now')),
                updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now'))
            )
        )");
        
        // Create route_rules table
        db.exec(R"(
            CREATE TABLE IF NOT EXISTS route_rules (
                id INTEGER PRIMARY KEY,
                route_profile_id INTEGER NOT NULL,
                rule_order INTEGER NOT NULL,
                name TEXT NOT NULL DEFAULT '',
                type INTEGER NOT NULL DEFAULT 0,
                simple_action INTEGER,
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
                FOREIGN KEY(route_profile_id) REFERENCES route_profiles(id) ON DELETE CASCADE
            )
        )");
        
        // Create index for faster lookups
        db.exec("CREATE INDEX IF NOT EXISTS idx_route_rules_profile_id ON route_rules(route_profile_id)");
    }

    QJsonObject RoutesRepo::routeRuleToJson(const RouteRule* rule) const {
        QJsonObject json;
        
        json["name"] = rule->name;
        json["type"] = rule->type;
        json["simpleAction"] = rule->simpleAction;
        json["ip_version"] = rule->ip_version;
        json["network"] = rule->network;
        json["protocol"] = rule->protocol;
        json["inbound"] = QListStr2QJsonArray(rule->inbound);
        json["domain"] = QListStr2QJsonArray(rule->domain);
        json["domain_suffix"] = QListStr2QJsonArray(rule->domain_suffix);
        json["domain_keyword"] = QListStr2QJsonArray(rule->domain_keyword);
        json["domain_regex"] = QListStr2QJsonArray(rule->domain_regex);
        json["source_ip_cidr"] = QListStr2QJsonArray(rule->source_ip_cidr);
        json["source_ip_is_private"] = rule->source_ip_is_private;
        json["ip_cidr"] = QListStr2QJsonArray(rule->ip_cidr);
        json["ip_is_private"] = rule->ip_is_private;
        json["source_port"] = QListStr2QJsonArray(rule->source_port);
        json["source_port_range"] = QListStr2QJsonArray(rule->source_port_range);
        json["port"] = QListStr2QJsonArray(rule->port);
        json["port_range"] = QListStr2QJsonArray(rule->port_range);
        json["process_name"] = QListStr2QJsonArray(rule->process_name);
        json["process_path"] = QListStr2QJsonArray(rule->process_path);
        json["process_path_regex"] = QListStr2QJsonArray(rule->process_path_regex);
        json["rule_set"] = QListStr2QJsonArray(rule->rule_set);
        json["invert"] = rule->invert;
        json["outboundID"] = rule->outboundID;
        json["action"] = rule->action;
        json["rejectMethod"] = rule->rejectMethod;
        json["no_drop"] = rule->no_drop;
        json["override_address"] = rule->override_address;
        json["override_port"] = rule->override_port;
        json["sniffers"] = QListStr2QJsonArray(rule->sniffers);
        json["sniffOverrideDest"] = rule->sniffOverrideDest;
        json["strategy"] = rule->strategy;
        
        return json;
    }

    std::shared_ptr<RouteRule> RoutesRepo::routeRuleFromJson(const QJsonObject& json) const {
        auto rule = std::make_shared<RouteRule>();
        
        rule->name = json["name"].toString();
        rule->type = json["type"].toInt();
        rule->simpleAction = json["simpleAction"].toInt();
        rule->ip_version = json["ip_version"].toString();
        rule->network = json["network"].toString();
        rule->protocol = json["protocol"].toString();
        rule->inbound = QJsonArray2QListString(json["inbound"].toArray());
        rule->domain = QJsonArray2QListString(json["domain"].toArray());
        rule->domain_suffix = QJsonArray2QListString(json["domain_suffix"].toArray());
        rule->domain_keyword = QJsonArray2QListString(json["domain_keyword"].toArray());
        rule->domain_regex = QJsonArray2QListString(json["domain_regex"].toArray());
        rule->source_ip_cidr = QJsonArray2QListString(json["source_ip_cidr"].toArray());
        rule->source_ip_is_private = json["source_ip_is_private"].toBool();
        rule->ip_cidr = QJsonArray2QListString(json["ip_cidr"].toArray());
        rule->ip_is_private = json["ip_is_private"].toBool();
        rule->source_port = QJsonArray2QListString(json["source_port"].toArray());
        rule->source_port_range = QJsonArray2QListString(json["source_port_range"].toArray());
        rule->port = QJsonArray2QListString(json["port"].toArray());
        rule->port_range = QJsonArray2QListString(json["port_range"].toArray());
        rule->process_name = QJsonArray2QListString(json["process_name"].toArray());
        rule->process_path = QJsonArray2QListString(json["process_path"].toArray());
        rule->process_path_regex = QJsonArray2QListString(json["process_path_regex"].toArray());
        rule->rule_set = QJsonArray2QListString(json["rule_set"].toArray());
        rule->invert = json["invert"].toBool();
        rule->outboundID = json["outboundID"].toInt();
        rule->action = json["action"].toString();
        rule->rejectMethod = json["rejectMethod"].toString();
        rule->no_drop = json["no_drop"].toBool();
        rule->override_address = json["override_address"].toString();
        rule->override_port = json["override_port"].toString();
        rule->sniffers = QJsonArray2QListString(json["sniffers"].toArray());
        rule->sniffOverrideDest = json["sniffOverrideDest"].toBool();
        rule->strategy = json["strategy"].toString();
        
        return rule;
    }

    QJsonObject RoutesRepo::routeProfileToJson(const RouteProfile* routeProfile) const {
        QJsonObject json;
        
        json["id"] = routeProfile->id;
        json["name"] = routeProfile->name;
        json["defaultOutboundID"] = routeProfile->defaultOutboundID;
        
        QJsonArray rulesArray;
        for (const auto& rule : routeProfile->Rules) {
            rulesArray.append(routeRuleToJson(rule.get()));
        }
        json["rules"] = rulesArray;
        
        return json;
    }

    std::shared_ptr<RouteProfile> RoutesRepo::routeProfileFromJson(const QJsonObject& json) const {
        auto routeProfile = std::make_shared<RouteProfile>();
        
        routeProfile->id = json["id"].toInt();
        routeProfile->name = json["name"].toString();
        routeProfile->defaultOutboundID = json["defaultOutboundID"].toInt();
        
        // Load rules
        if (json.contains("rules") && json["rules"].isArray()) {
            QJsonArray rulesArray = json["rules"].toArray();
            for (const auto& ruleValue : rulesArray) {
                if (ruleValue.isObject()) {
                    auto rule = routeRuleFromJson(ruleValue.toObject());
                    routeProfile->Rules.append(rule);
                }
            }
        }
        
        return routeProfile;
    }

    void RoutesRepo::saveToDatabase(const RouteProfile* routeProfile, int id) const {
        QMutexLocker locker(&mutex);
        
        // Check if route profile exists
        auto checkQuery = db.query("SELECT id FROM route_profiles WHERE id = ?", id);
        bool exists = checkQuery && checkQuery->executeStep();
        
        if (exists) {
            // Update route profile
            db.exec(R"(
                UPDATE route_profiles 
                SET name = ?, default_outbound_id = ?, updated_at = strftime('%s', 'now')
                WHERE id = ?
            )",
                routeProfile->name.toStdString(),
                routeProfile->defaultOutboundID,
                id
            );
            
            // Delete existing rules
            db.exec("DELETE FROM route_rules WHERE route_profile_id = ?", id);
        } else {
            // Insert route profile
            db.exec(R"(
                INSERT INTO route_profiles (id, name, default_outbound_id)
                VALUES (?, ?, ?)
            )",
                id,
                routeProfile->name.toStdString(),
                routeProfile->defaultOutboundID
            );
        }
        
        // Insert rules
        int ruleOrder = 0;
        for (const auto& rule : routeProfile->Rules) {
            // Serialize QList<QString> fields to JSON
            QJsonArray inboundArray = QListStr2QJsonArray(rule->inbound);
            QJsonArray domainArray = QListStr2QJsonArray(rule->domain);
            QJsonArray domainSuffixArray = QListStr2QJsonArray(rule->domain_suffix);
            QJsonArray domainKeywordArray = QListStr2QJsonArray(rule->domain_keyword);
            QJsonArray domainRegexArray = QListStr2QJsonArray(rule->domain_regex);
            QJsonArray sourceIpCidrArray = QListStr2QJsonArray(rule->source_ip_cidr);
            QJsonArray ipCidrArray = QListStr2QJsonArray(rule->ip_cidr);
            QJsonArray sourcePortArray = QListStr2QJsonArray(rule->source_port);
            QJsonArray sourcePortRangeArray = QListStr2QJsonArray(rule->source_port_range);
            QJsonArray portArray = QListStr2QJsonArray(rule->port);
            QJsonArray portRangeArray = QListStr2QJsonArray(rule->port_range);
            QJsonArray processNameArray = QListStr2QJsonArray(rule->process_name);
            QJsonArray processPathArray = QListStr2QJsonArray(rule->process_path);
            QJsonArray processPathRegexArray = QListStr2QJsonArray(rule->process_path_regex);
            QJsonArray ruleSetArray = QListStr2QJsonArray(rule->rule_set);
            QJsonArray sniffersArray = QListStr2QJsonArray(rule->sniffers);
            
            QString inboundJson = QString::fromUtf8(QJsonDocument(inboundArray).toJson(QJsonDocument::Compact));
            QString domainJson = QString::fromUtf8(QJsonDocument(domainArray).toJson(QJsonDocument::Compact));
            QString domainSuffixJson = QString::fromUtf8(QJsonDocument(domainSuffixArray).toJson(QJsonDocument::Compact));
            QString domainKeywordJson = QString::fromUtf8(QJsonDocument(domainKeywordArray).toJson(QJsonDocument::Compact));
            QString domainRegexJson = QString::fromUtf8(QJsonDocument(domainRegexArray).toJson(QJsonDocument::Compact));
            QString sourceIpCidrJson = QString::fromUtf8(QJsonDocument(sourceIpCidrArray).toJson(QJsonDocument::Compact));
            QString ipCidrJson = QString::fromUtf8(QJsonDocument(ipCidrArray).toJson(QJsonDocument::Compact));
            QString sourcePortJson = QString::fromUtf8(QJsonDocument(sourcePortArray).toJson(QJsonDocument::Compact));
            QString sourcePortRangeJson = QString::fromUtf8(QJsonDocument(sourcePortRangeArray).toJson(QJsonDocument::Compact));
            QString portJson = QString::fromUtf8(QJsonDocument(portArray).toJson(QJsonDocument::Compact));
            QString portRangeJson = QString::fromUtf8(QJsonDocument(portRangeArray).toJson(QJsonDocument::Compact));
            QString processNameJson = QString::fromUtf8(QJsonDocument(processNameArray).toJson(QJsonDocument::Compact));
            QString processPathJson = QString::fromUtf8(QJsonDocument(processPathArray).toJson(QJsonDocument::Compact));
            QString processPathRegexJson = QString::fromUtf8(QJsonDocument(processPathRegexArray).toJson(QJsonDocument::Compact));
            QString ruleSetJson = QString::fromUtf8(QJsonDocument(ruleSetArray).toJson(QJsonDocument::Compact));
            QString sniffersJson = QString::fromUtf8(QJsonDocument(sniffersArray).toJson(QJsonDocument::Compact));
            
            db.exec(R"(
                INSERT INTO route_rules 
                (route_profile_id, rule_order, name, type, simple_action, ip_version, network, protocol,
                 inbound_json, domain_json, domain_suffix_json, domain_keyword_json, domain_regex_json,
                 source_ip_cidr_json, source_ip_is_private, ip_cidr_json, ip_is_private,
                 source_port_json, source_port_range_json, port_json, port_range_json,
                 process_name_json, process_path_json, process_path_regex_json, rule_set_json,
                 invert, outbound_id, action, reject_method, no_drop,
                 override_address, override_port, sniffers_json, sniff_override_dest, strategy)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            )",
                id,
                ruleOrder++,
                rule->name.toStdString(),
                rule->type,
                rule->simpleAction,
                rule->ip_version.toStdString(),
                rule->network.toStdString(),
                rule->protocol.toStdString(),
                inboundJson.toStdString(),
                domainJson.toStdString(),
                domainSuffixJson.toStdString(),
                domainKeywordJson.toStdString(),
                domainRegexJson.toStdString(),
                sourceIpCidrJson.toStdString(),
                rule->source_ip_is_private ? 1 : 0,
                ipCidrJson.toStdString(),
                rule->ip_is_private ? 1 : 0,
                sourcePortJson.toStdString(),
                sourcePortRangeJson.toStdString(),
                portJson.toStdString(),
                portRangeJson.toStdString(),
                processNameJson.toStdString(),
                processPathJson.toStdString(),
                processPathRegexJson.toStdString(),
                ruleSetJson.toStdString(),
                rule->invert ? 1 : 0,
                rule->outboundID,
                rule->action.toStdString(),
                rule->rejectMethod.toStdString(),
                rule->no_drop ? 1 : 0,
                rule->override_address.toStdString(),
                rule->override_port.toStdString(),
                sniffersJson.toStdString(),
                rule->sniffOverrideDest ? 1 : 0,
                rule->strategy.toStdString()
            );
        }
    }

    std::shared_ptr<RouteProfile> RoutesRepo::loadFromDatabase(int id) const {
        // Load route profile
        auto profileQuery = db.query(R"(
            SELECT id, name, default_outbound_id
            FROM route_profiles WHERE id = ?
        )", id);
        if (!profileQuery || !profileQuery->executeStep()) {
            return nullptr;
        }
        
        QJsonObject json;
        json["id"] = profileQuery->getColumn(0).getInt();
        json["name"] = QString::fromStdString(profileQuery->getColumn(1).getText());
        json["defaultOutboundID"] = profileQuery->getColumn(2).getInt();
        
        // Load rules
        QJsonArray rulesArray;
        auto rulesQuery = db.query(R"(
            SELECT name, type, simple_action, ip_version, network, protocol,
                   inbound_json, domain_json, domain_suffix_json, domain_keyword_json, domain_regex_json,
                   source_ip_cidr_json, source_ip_is_private, ip_cidr_json, ip_is_private,
                   source_port_json, source_port_range_json, port_json, port_range_json,
                   process_name_json, process_path_json, process_path_regex_json, rule_set_json,
                   invert, outbound_id, action, reject_method, no_drop,
                   override_address, override_port, sniffers_json, sniff_override_dest, strategy
            FROM route_rules WHERE route_profile_id = ? ORDER BY rule_order
        )", id);
        if (rulesQuery) {
            while (rulesQuery->executeStep()) {
                QJsonObject ruleJson;
                ruleJson["name"] = QString::fromStdString(rulesQuery->getColumn(0).getText());
                ruleJson["type"] = rulesQuery->getColumn(1).getInt();
                ruleJson["simpleAction"] = rulesQuery->getColumn(2).getInt();
                ruleJson["ip_version"] = QString::fromStdString(rulesQuery->getColumn(3).getText());
                ruleJson["network"] = QString::fromStdString(rulesQuery->getColumn(4).getText());
                ruleJson["protocol"] = QString::fromStdString(rulesQuery->getColumn(5).getText());
                
                // Parse JSON arrays
                QString inboundJsonStr = QString::fromStdString(rulesQuery->getColumn(6).getText());
                QJsonDocument inboundDoc = QJsonDocument::fromJson(inboundJsonStr.toUtf8());
                ruleJson["inbound"] = inboundDoc.isArray() ? inboundDoc.array() : QJsonArray();
                
                QString domainJsonStr = QString::fromStdString(rulesQuery->getColumn(7).getText());
                QJsonDocument domainDoc = QJsonDocument::fromJson(domainJsonStr.toUtf8());
                ruleJson["domain"] = domainDoc.isArray() ? domainDoc.array() : QJsonArray();
                
                QString domainSuffixJsonStr = QString::fromStdString(rulesQuery->getColumn(8).getText());
                QJsonDocument domainSuffixDoc = QJsonDocument::fromJson(domainSuffixJsonStr.toUtf8());
                ruleJson["domain_suffix"] = domainSuffixDoc.isArray() ? domainSuffixDoc.array() : QJsonArray();
                
                QString domainKeywordJsonStr = QString::fromStdString(rulesQuery->getColumn(9).getText());
                QJsonDocument domainKeywordDoc = QJsonDocument::fromJson(domainKeywordJsonStr.toUtf8());
                ruleJson["domain_keyword"] = domainKeywordDoc.isArray() ? domainKeywordDoc.array() : QJsonArray();
                
                QString domainRegexJsonStr = QString::fromStdString(rulesQuery->getColumn(10).getText());
                QJsonDocument domainRegexDoc = QJsonDocument::fromJson(domainRegexJsonStr.toUtf8());
                ruleJson["domain_regex"] = domainRegexDoc.isArray() ? domainRegexDoc.array() : QJsonArray();
                
                QString sourceIpCidrJsonStr = QString::fromStdString(rulesQuery->getColumn(11).getText());
                QJsonDocument sourceIpCidrDoc = QJsonDocument::fromJson(sourceIpCidrJsonStr.toUtf8());
                ruleJson["source_ip_cidr"] = sourceIpCidrDoc.isArray() ? sourceIpCidrDoc.array() : QJsonArray();
                
                ruleJson["source_ip_is_private"] = rulesQuery->getColumn(12).getInt() != 0;
                
                QString ipCidrJsonStr = QString::fromStdString(rulesQuery->getColumn(13).getText());
                QJsonDocument ipCidrDoc = QJsonDocument::fromJson(ipCidrJsonStr.toUtf8());
                ruleJson["ip_cidr"] = ipCidrDoc.isArray() ? ipCidrDoc.array() : QJsonArray();
                
                ruleJson["ip_is_private"] = rulesQuery->getColumn(14).getInt() != 0;
                
                QString sourcePortJsonStr = QString::fromStdString(rulesQuery->getColumn(15).getText());
                QJsonDocument sourcePortDoc = QJsonDocument::fromJson(sourcePortJsonStr.toUtf8());
                ruleJson["source_port"] = sourcePortDoc.isArray() ? sourcePortDoc.array() : QJsonArray();
                
                QString sourcePortRangeJsonStr = QString::fromStdString(rulesQuery->getColumn(16).getText());
                QJsonDocument sourcePortRangeDoc = QJsonDocument::fromJson(sourcePortRangeJsonStr.toUtf8());
                ruleJson["source_port_range"] = sourcePortRangeDoc.isArray() ? sourcePortRangeDoc.array() : QJsonArray();
                
                QString portJsonStr = QString::fromStdString(rulesQuery->getColumn(17).getText());
                QJsonDocument portDoc = QJsonDocument::fromJson(portJsonStr.toUtf8());
                ruleJson["port"] = portDoc.isArray() ? portDoc.array() : QJsonArray();
                
                QString portRangeJsonStr = QString::fromStdString(rulesQuery->getColumn(18).getText());
                QJsonDocument portRangeDoc = QJsonDocument::fromJson(portRangeJsonStr.toUtf8());
                ruleJson["port_range"] = portRangeDoc.isArray() ? portRangeDoc.array() : QJsonArray();
                
                QString processNameJsonStr = QString::fromStdString(rulesQuery->getColumn(19).getText());
                QJsonDocument processNameDoc = QJsonDocument::fromJson(processNameJsonStr.toUtf8());
                ruleJson["process_name"] = processNameDoc.isArray() ? processNameDoc.array() : QJsonArray();
                
                QString processPathJsonStr = QString::fromStdString(rulesQuery->getColumn(20).getText());
                QJsonDocument processPathDoc = QJsonDocument::fromJson(processPathJsonStr.toUtf8());
                ruleJson["process_path"] = processPathDoc.isArray() ? processPathDoc.array() : QJsonArray();
                
                QString processPathRegexJsonStr = QString::fromStdString(rulesQuery->getColumn(21).getText());
                QJsonDocument processPathRegexDoc = QJsonDocument::fromJson(processPathRegexJsonStr.toUtf8());
                ruleJson["process_path_regex"] = processPathRegexDoc.isArray() ? processPathRegexDoc.array() : QJsonArray();
                
                QString ruleSetJsonStr = QString::fromStdString(rulesQuery->getColumn(22).getText());
                QJsonDocument ruleSetDoc = QJsonDocument::fromJson(ruleSetJsonStr.toUtf8());
                ruleJson["rule_set"] = ruleSetDoc.isArray() ? ruleSetDoc.array() : QJsonArray();
                
                ruleJson["invert"] = rulesQuery->getColumn(23).getInt() != 0;
                ruleJson["outboundID"] = rulesQuery->getColumn(24).getInt();
                ruleJson["action"] = QString::fromStdString(rulesQuery->getColumn(25).getText());
                ruleJson["rejectMethod"] = QString::fromStdString(rulesQuery->getColumn(26).getText());
                ruleJson["no_drop"] = rulesQuery->getColumn(27).getInt() != 0;
                ruleJson["override_address"] = QString::fromStdString(rulesQuery->getColumn(28).getText());
                ruleJson["override_port"] = QString::fromStdString(rulesQuery->getColumn(29).getText());
                
                QString sniffersJsonStr = QString::fromStdString(rulesQuery->getColumn(30).getText());
                QJsonDocument sniffersDoc = QJsonDocument::fromJson(sniffersJsonStr.toUtf8());
                ruleJson["sniffers"] = sniffersDoc.isArray() ? sniffersDoc.array() : QJsonArray();
                
                ruleJson["sniffOverrideDest"] = rulesQuery->getColumn(31).getInt() != 0;
                ruleJson["strategy"] = QString::fromStdString(rulesQuery->getColumn(32).getText());
                
                rulesArray.append(ruleJson);
            }
        }
        
        json["rules"] = rulesArray;
        
        return routeProfileFromJson(json);
    }

    std::shared_ptr<RouteProfile> RoutesRepo::NewRouteProfile() {
        return std::make_shared<RouteProfile>();
    }

    bool RoutesRepo::AddRouteProfile(std::shared_ptr<RouteProfile>& routeProfile) {
        QMutexLocker locker(&mutex);
        
        if (routeProfile->id >= 0) {
            return false; // Already has an ID
        }
        
        int newId = NewRouteProfileID();
        routeProfile->id = newId;
        
        // Save to database first
        saveToDatabase(routeProfile.get(), newId);
        
        // Add to identity map
        identityMap[newId] = std::weak_ptr<RouteProfile>(routeProfile);
        
        return true;
    }

    std::shared_ptr<RouteProfile> RoutesRepo::GetRouteProfile(int id) const {
        QMutexLocker locker(&mutex);
        
        // Check identity map first
        auto it = identityMap.find(id);
        if (it != identityMap.end()) {
            auto shared = it->second.lock();
            if (shared) {
                return shared; // Return existing instance
            } else {
                // Weak pointer expired, remove from map
                identityMap.erase(it);
            }
        }
        
        // Load from database
        auto routeProfile = loadFromDatabase(id);
        if (!routeProfile) {
            return nullptr;
        }
        
        // Add to identity map
        identityMap[id] = std::weak_ptr<RouteProfile>(routeProfile);
        
        return routeProfile;
    }

    void RoutesRepo::DeleteRouteProfile(int id) {
        QMutexLocker locker(&mutex);
        
        // Remove from identity map
        identityMap.erase(id);
        
        // Delete from database (CASCADE will delete rules)
        db.exec("DELETE FROM route_profiles WHERE id = ?", id);
    }

    void RoutesRepo::UpdateRouteProfiles(const QList<std::shared_ptr<RouteProfile>>& routeProfiles) {
        QMutexLocker locker(&mutex);
        
        // Get all existing IDs
        QSet<int> existingIds;
        auto query = db.query("SELECT id FROM route_profiles");
        if (query) {
            while (query->executeStep()) {
                existingIds.insert(query->getColumn(0).getInt());
            }
        }
        
        QSet<int> newIds;
        // Add or update route profiles
        for (const auto& routeProfile : routeProfiles) {
            newIds.insert(routeProfile->id);
            if (routeProfile->id >= 0) {
                // Update existing
                saveToDatabase(routeProfile.get(), routeProfile->id);
                // Update identity map if exists
                auto it = identityMap.find(routeProfile->id);
                if (it != identityMap.end()) {
                    auto shared = it->second.lock();
                    if (shared) {
                        // Update the existing object
                        *shared = *routeProfile;
                    }
                }
            } else {
                // Add new
                auto mutableRouteProfile = const_cast<std::shared_ptr<RouteProfile>&>(routeProfile);
                AddRouteProfile(mutableRouteProfile);
            }
        }
        
        // Delete route profiles that are no longer in the list
        for (int id : existingIds) {
            if (!newIds.contains(id)) {
                identityMap.erase(id);
                db.exec("DELETE FROM route_profiles WHERE id = ?", id);
            }
        }
    }

    QList<int> RoutesRepo::GetAllRouteProfileIds() const {
        QMutexLocker locker(&mutex);
        
        QList<int> ids;
        auto query = db.query("SELECT id FROM route_profiles ORDER BY id");
        if (query) {
            while (query->executeStep()) {
                ids.append(query->getColumn(0).getInt());
            }
        }
        return ids;
    }

    QList<std::shared_ptr<RouteProfile>> RoutesRepo::GetAllRouteProfiles() const {
        QMutexLocker locker(&mutex);
        QList<std::shared_ptr<RouteProfile>> routeProfiles;
        
        // Get all route profile IDs
        auto query = db.query("SELECT id FROM route_profiles ORDER BY id");
        if (query) {
            while (query->executeStep()) {
                int id = query->getColumn(0).getInt();
                
                // Check identity map first (we already have the lock)
                auto it = identityMap.find(id);
                if (it != identityMap.end()) {
                    auto shared = it->second.lock();
                    if (shared) {
                        routeProfiles.append(shared);
                        continue; // Use existing instance from identity map
                    } else {
                        // Weak pointer expired, remove from map
                        identityMap.erase(it);
                    }
                }
                
                // Load from database (loadFromDatabase doesn't lock mutex)
                auto routeProfile = loadFromDatabase(id);
                if (routeProfile) {
                    // Add to identity map
                    identityMap[id] = std::weak_ptr<RouteProfile>(routeProfile);
                    routeProfiles.append(routeProfile);
                }
            }
        }
        
        return routeProfiles;
    }

    int RoutesRepo::NewRouteProfileID() const {
        // Atomically increment and get the new ID using RETURNING clause
        // Note: This method is called from within methods that already hold the mutex lock
        auto query = db.query("UPDATE entity_ids SET route_profile_last_id = route_profile_last_id + 1 RETURNING route_profile_last_id");
        if (query && query->executeStep()) {
            return query->getColumn(0).getInt();
        }
        
        // Fallback if RETURNING is not supported (shouldn't happen with modern SQLite)
        return 0;
    }

    bool RoutesRepo::Save(const std::shared_ptr<RouteProfile>& routeProfile) {
        if (!routeProfile) {
            return false;
        }
        
        if (routeProfile->id < 0) {
            return false; // Route profile doesn't have an ID, use AddRouteProfile instead
        }
        
        QMutexLocker locker(&mutex);
        
        // Save to database
        saveToDatabase(routeProfile.get(), routeProfile->id);
        
        // Update identity map
        identityMap[routeProfile->id] = std::weak_ptr<RouteProfile>(routeProfile);
        
        return true;
    }
}
