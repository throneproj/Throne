#include <QJsonObject>
#include <QJsonArray>
#include "include/database/entities/RouteProfile.h"
#include <iostream>

#include "include/database/ProfilesRepo.h"

#include "include/global/Configs.hpp"

namespace Configs {
    bool isOutboundIDValid(int id) {
        switch (id) {
            case -1:
                return true;
            case -2:
                return true;
            default:
                return Configs::dataManager->profilesRepo->GetProfile(id) != nullptr;
        }
    }

    int getOutboundID(const QString& name) {
        if (name == "proxy") return -1;
        if (name == "direct") return -2;
        if (auto profile = Configs::dataManager->profilesRepo->GetProfileByName(name)) return profile->id;

        return INVALID_ID;
    }

    RouteProfile::RouteProfile(const RouteProfile& other) {
        id = other.id;
        name = QString(other.name);
        for (const auto& item: other.Rules) {
            Rules.push_back(std::make_shared<RouteRule>(*item));
        }
        defaultOutboundID = other.defaultOutboundID;
    }

    QList<std::shared_ptr<RouteRule>> RouteProfile::parseJsonArray(const QJsonArray& arr, QString* parseError) {
        if (arr.empty()) {
            parseError->append("Input is not a valid json array");
            return {};
        }

        auto rules = QList<std::shared_ptr<RouteRule>>();
        auto ruleID = 1;
        for (const auto& item: arr) {
            if (!item.isObject()) {
                parseError->append(QString("expected array of json objects but have member of type '%1'").arg(item.type()));
                return {};
            }

            auto obj = item.toObject();
            auto rule = std::make_shared<RouteRule>();
            for (const auto& key: obj.keys()) {
                auto val = obj.value(key);
                if (key == "outbound") {
                    if (val.isDouble()) {
                        if (!isOutboundIDValid(val.toInt())) {
                            parseError->append(QString("outbound id %1 is not valid").arg(val.toInt()));
                            return {};
                        }
                        rule->outboundID = val.toInt();
                    } else if (val.isString()) {
                        auto id = getOutboundID(val.toString());
                        if (id == INVALID_ID) {
                            parseError->append(QString("outbound with name %1 does not exist").arg(val.toString()));
                            return {};
                        }
                        rule->outboundID = id;
                    }
                } else if (val.isArray()) {
                    rule->set_field_value(key, QJsonArray2QListString(val.toArray()));
                } else if (val.isString()) {
                    rule->set_field_value(key, {val.toString()});
                } else if (val.isBool()) {
                    rule->set_field_value(key, {val.toBool() ? "true":"false"});
                }
            }
            rule->name = "imported rule #" + Int2String(ruleID++);
            rules << rule;
        }

        return rules;
    }

    QJsonArray RouteProfile::get_route_rules(bool forView, std::map<int, QString> outboundMap) {
        QJsonArray res;
        bool added_adblock = false;
        auto createAdblockRule = []() -> QJsonObject {
            QJsonObject obj;
            obj["action"] = "reject";
            QJsonArray jarray;
            jarray.append("throne-adblocksingbox");
            obj["rule_set"] = jarray;
            return obj;
        };
        for (const auto &item: Rules) {
            auto outboundTag = QString();
            if (outboundMap.contains(item->outboundID)) outboundTag = outboundMap[item->outboundID];
            auto rule_json = item->get_rule_json(forView, outboundTag);
            if (rule_json.empty()) {
                MW_show_log("Aborted generating routing section, an error has occurred");
                return {};
            }
            if (!added_adblock && Configs::dataManager->settingsRepo->adblock_enable && rule_json["action"] == "route") {
                res += createAdblockRule();
                added_adblock = true;
            }                
            res += rule_json;
        }
        if (!added_adblock && Configs::dataManager->settingsRepo->adblock_enable)
            res += createAdblockRule();

        return res;
    }

    std::shared_ptr<RouteProfile> RouteProfile::GetDefaultChain() {
        auto defaultChain = std::make_shared<RouteProfile>();
        defaultChain->name = "Default";
        auto defaultRule = std::make_shared<RouteRule>();
        defaultRule->name = "Route DNS";
        defaultRule->action = "hijack-dns";
        defaultRule->protocol = "dns";
        defaultChain->Rules << defaultRule;
        return defaultChain;
    }

    std::shared_ptr<QList<int>> RouteProfile::get_used_outbounds() {
        auto res = std::make_shared<QList<int>>();
        for (const auto& item: Rules) {
            res->push_back(item->outboundID);
        }
        return res;
    }

    std::shared_ptr<QStringList> RouteProfile::get_used_rule_sets() {
        auto res = std::make_shared<QStringList>();
        for (const auto& item: Rules) {
            for (const auto& ruleItem: item->rule_set) {
                res->push_back(ruleItem);
            }
        }
        return res;
    }

    QStringList RouteProfile::get_direct_sites() {
        auto res = QStringList();
        for (const auto& item: Rules) {
            if (item->outboundID == -2) {
                for (const auto& rset: item->rule_set) {
                    if (rset.startsWith("geosite-")) res << QString("ruleset:" + rset);
                }
                for (const auto& domain: item->domain) {
                    res << QString("domain:" + domain);
                }
                for (const auto& suffix: item->domain_suffix) {
                    res << QString("suffix:" + suffix);
                }
                for (const auto& keyword: item->domain_keyword) {
                    res << QString("keyword:" + keyword);
                }
                for (const auto& regex: item->domain_regex) {
                    res << QString("regex:" + regex);
                }
            }
        }
        return res;
    }

    QStringList RouteProfile::get_direct_ips()
    {
        auto res = QStringList();
        for (const auto& item: Rules) {
            if (item->outboundID == directID && item->action == "route") {
                for (const auto& rset: item->rule_set) {
                    if (rset.startsWith("geoip-")) res << QString("ruleset:" + rset);
                }
                for (const auto& domain: item->ip_cidr) {
                    res << QString("ip:" + domain);
                }
            }
        }
        return res;
    }

    QString RouteProfile::GetSimpleRules(simpleAction action)
    {
        QString res;
        for (const auto& item: Rules)
        {
            if (item->type != custom && item->simpleAction == action)
            {
                for (const auto& domain : item->domain) res += QString("domain:" + domain + "\n");
                for (const auto& domain_suffix : item->domain_suffix) res += QString("suffix:" + domain_suffix + "\n");
                for (const auto& domain_keyword : item->domain_keyword) res += QString("keyword:" + domain_keyword + "\n");
                for (const auto& domain_regex : item->domain_regex) res += QString("regex:" + domain_regex + "\n");
                for (const auto& rule_set : item->rule_set) res += QString("ruleset:" + rule_set + "\n");
                for (const auto& ip_cidr : item->ip_cidr) res += QString("ip:" + ip_cidr + "\n");
                for (const auto& process_name : item->process_name) res += QString("processName:" + process_name + "\n");
                for (const auto& process_path : item->process_path) res += QString("processPath:" + process_path + "\n");
            }
        }
        return res;
    }


    QString RouteProfile::UpdateSimpleRules(const QString& content, simpleAction action)
    {
        QString res;
        auto items = content.split("\n");

        QList<std::shared_ptr<RouteRule>> toRemove;
        for (int idx=0;idx<Rules.length();idx++)
        {
            auto route_rule = Rules[idx];
            if (route_rule->type != custom && route_rule->simpleAction == action)
            {
                toRemove.append(route_rule);
            }
        }
        for (const auto& rule : toRemove)
        {
            Rules.removeOne(rule);
        }

        QSet<int> added_items;
        for (const auto type : {simpleAddress, simpleProcessName, simpleProcessPath})
        {
            auto rule = std::make_shared<RouteRule>();
            rule->name = QString("Simple ") + ruleTypeToString(type) + " -> " + simpleActionToString(action);
            rule->type = type;
            rule->simpleAction = action;

            if (action == block)
            {
                rule->action = "reject";
            } else if (action == direct)
            {
                rule->action = "route";
                rule->outboundID = -2;
            } else if (action == proxy)
            {
                rule->action = "route";
                rule->outboundID = -1;
            }
            auto ruleEmpty = true;
            for (int i=0;i<items.size();i++)
            {
                auto item = items[i];
                item = item.trimmed();
                if (item.isEmpty()) continue;
                if (add_simple_rule(item, rule, type))
                {
                    added_items.insert(i);
                    ruleEmpty = false;
                }
            }
            if (!ruleEmpty) Rules.append(rule);
        }
        for (int i=0;i<items.size();i++)
        {
            if (items[i].trimmed().isEmpty()) continue;
            if (!added_items.contains(i)) res += "Could not add " + items[i];
        }
        return res;
    }

    bool RouteProfile::add_simple_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type)
    {
        if (type == simpleAddress) return add_simple_address_rule(content, rule);
        else return add_simple_process_rule(content, rule, type);
    }

    bool RouteProfile::add_simple_address_rule(const QString& content, const std::shared_ptr<RouteRule>& rule)
    {
        auto colonIdx = content.indexOf(':');
        if (colonIdx == -1) return false;
        const QString& address = content.mid(colonIdx+1);
        const QString& subType = content.left(colonIdx);
        if (subType == "domain") {
            if (!rule->domain.contains(address)) rule->domain.append(address);
            return true;
        } else if (subType == "suffix") {
            if (!rule->domain_suffix.contains(address)) rule->domain_suffix.append(address);
            return true;
        } else if (subType == "keyword") {
            if (!rule->domain_keyword.contains(address)) rule->domain_keyword.append(address);
            return true;
        } else if (subType == "regex") {
            if (!rule->domain_regex.contains(address)) rule->domain_regex.append(address);
            return true;
        } else if (subType == "ruleset") {
            if (!rule->rule_set.contains(address)) rule->rule_set.append(address);
            return true;
        } else if (subType == "ip") {
            if (!rule->ip_cidr.contains(address)) rule->ip_cidr.append(address);
            return true;
        } else {
            return false;
        }
    }

    bool RouteProfile::add_simple_process_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type)
    {
        if (!content.contains(":")) return false;
        auto prefix = content.first(content.indexOf(':'));
        const QString& address = content.section(':', 1);
        if (prefix == "processPath" && type == simpleProcessPath)
        {
            if (!rule->process_path.contains(address)) rule->process_path.append(address);
            return true;
        } else if (prefix == "processName" && type == simpleProcessName)
        {
            if (!rule->process_name.contains(address)) rule->process_name.append(address);
            return true;
        } else
        {
            return false;
        }
    }
}
