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

    QList<std::shared_ptr<RouteRule>> RouteProfile::get_simple_rules() {
        QList<std::shared_ptr<RouteRule>> rules;

        auto rule = RouteRule();
        rule.type = simpleAddressProxy;
        rule.action = "route";
        rule.outboundID = getOutboundID("proxy");
        rule.name = ruleTypeToString(static_cast<ruleType>(rule.type));
        rules << std::make_shared<RouteRule>(rule);

        rule = RouteRule();
        rule.type = simpleAddressBypass;
        rule.action = "route";
        rule.outboundID = getOutboundID("direct");
        rule.name = ruleTypeToString(static_cast<ruleType>(rule.type));
        rules << std::make_shared<RouteRule>(rule);

        rule = RouteRule();
        rule.type = simpleAddressBlock;
        rule.action = "reject";
        rule.name = ruleTypeToString(static_cast<ruleType>(rule.type));
        rules << std::make_shared<RouteRule>(rule);

        rule = RouteRule();
        rule.type = simpleProcessNameProxy;
        rule.action = "route";
        rule.outboundID = getOutboundID("proxy");
        rule.name = ruleTypeToString(static_cast<ruleType>(rule.type));
        rules << std::make_shared<RouteRule>(rule);

        rule = RouteRule();
        rule.type = simpleProcessNameBypass;
        rule.action = "route";
        rule.outboundID = getOutboundID("direct");
        rule.name = ruleTypeToString(static_cast<ruleType>(rule.type));
        rules << std::make_shared<RouteRule>(rule);

        rule = RouteRule();
        rule.type = simpleProcessNameBlock;
        rule.action = "reject";
        rule.name = ruleTypeToString(static_cast<ruleType>(rule.type));
        rules << std::make_shared<RouteRule>(rule);

        rule = RouteRule();
        rule.type = simpleProcessPathProxy;
        rule.action = "route";
        rule.outboundID = getOutboundID("proxy");
        rule.name = ruleTypeToString(static_cast<ruleType>(rule.type));
        rules << std::make_shared<RouteRule>(rule);

        rule = RouteRule();
        rule.type = simpleProcessPathBypass;
        rule.action = "route";
        rule.outboundID = getOutboundID("direct");
        rule.name = ruleTypeToString(static_cast<ruleType>(rule.type));
        rules << std::make_shared<RouteRule>(rule);

        rule = RouteRule();
        rule.type = simpleProcessPathBlock;
        rule.action = "reject";
        rule.name = ruleTypeToString(static_cast<ruleType>(rule.type));
        rules << std::make_shared<RouteRule>(rule);

        return rules;
    }

    void RouteProfile::reset_simple_rule(std::shared_ptr<RouteRule>& rule) {
        auto cleanRules = get_simple_rules();
        for (auto r : cleanRules) {
            if (r->type == rule->type) {
                rule = std::move(r);
                return;
            }
        }
    }

    RouteProfile::RouteProfile(const RouteProfile& other) {
        id = other.id;
        name = QString(other.name);
        for (const auto& item: other.Rules) {
            Rules.push_back(std::make_shared<RouteRule>(*item));
        }
        defaultOutboundID = other.defaultOutboundID;
        rawSimpleRulesText = other.rawSimpleRulesText;
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
            if (item->type != custom && item->isEmpty()) continue;
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
            if (item->outboundID == directID && item->action == "route") {
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

    bool RouteProfile::IsEmpty() {
        for (const auto& item: Rules) {
            if (!item->isEmpty()) return false;
        }
        return true;
    }

    void RouteProfile::ResetRules() {
        Rules.clear();
    }

    void RouteProfile::ResetSimpleRule(ruleType type) {
        for (std::shared_ptr<RouteRule> &item: Rules) {
            if (item->type == type) {
                reset_simple_rule(item);
                return;
            }
        }
        auto cleanRules = get_simple_rules();
        for (std::shared_ptr<RouteRule> &item: cleanRules) {
            if (item->type == type) {
                Rules << item;
                return;
            }
        }
    }

    /// Возвращает указатель на поле хранения сырого текста для заданного действия
    static QString* rawTextForAction(simpleAction action, RouteProfile::RawSimpleRules& storage) {
        if (action == direct) return &storage.direct;
        if (action == block) return &storage.block;
        return &storage.proxy;
    }

    QString RouteProfile::GetSimpleRules(simpleAction action)
    {
        // Если есть кеш сырого текста из UpdateSimpleRules, вернуть его для сохранения комментариев
        const QString* cached = rawTextForAction(action, const_cast<RouteProfile::RawSimpleRules&>(rawSimpleRulesText));
        if (!cached->isEmpty()) return *cached;

        // Фоллбек: перегенерация из структурированных правил (первое открытие до редактирования в Basic)
        QList<int> types;
        if (action == proxy) {
            types << simpleAddressProxy;
            types << simpleProcessNameProxy;
            types << simpleProcessPathProxy;
        } else if (action == direct) {
            types << simpleAddressBypass;
            types << simpleProcessNameBypass;
            types << simpleProcessPathBypass;
        } else {
            types << simpleAddressBlock;
            types << simpleProcessNameBlock;
            types << simpleProcessPathBlock;
        }
        QString res;
        for (const auto& item: Rules)
        {
            if (types.contains(item->type))
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
        // Сохраняем исходный текст (с комментариями), чтобы GetSimpleRules мог восстановить его при переключении вкладки
        *rawTextForAction(action, rawSimpleRulesText) = content;

        QString res;
        auto items = content.split("\n");
        QList<ruleType> types;
        if (action == proxy) {
            types << simpleAddressProxy;
            types << simpleProcessNameProxy;
            types << simpleProcessPathProxy;
        } else if (action == direct) {
            types << simpleAddressBypass;
            types << simpleProcessNameBypass;
            types << simpleProcessPathBypass;
        } else {
            types << simpleAddressBlock;
            types << simpleProcessNameBlock;
            types << simpleProcessPathBlock;
        }
        for (auto t : types) {
            ResetSimpleRule(t);
        }
        for (const auto& raw : items) {
            if (raw.trimmed().isEmpty()) continue;
            // Пропускаем строки-комментарии (начинающиеся с #)
            if (raw.trimmed().startsWith("#")) continue;
            auto type = get_rule_type(raw, action);
            if (type == custom) {
                res += "invalid rule:" + raw + "\n";
                continue;
            }
            auto rule = get_simple_rule_by_type(type);
            if (!rule) {
                res += "internal error, failed to get rule for: " + ruleTypeToString(type) + "\n";
                continue;
            }
            if (!add_simple_rule(raw, rule, type)) {
                res += "invalid rule:" + raw + "\n";
            }
        }
        FilterEmptyRules();
        return res;
    }

    void RouteProfile::FilterEmptyRules() {
        QList<std::shared_ptr<RouteRule>> newRules;
        for (auto rule : Rules) {
            if (!rule->isEmpty()) newRules.append(rule);
        }
        Rules = newRules;
    }

    bool RouteProfile::add_simple_rule(const QString& content, const std::shared_ptr<RouteRule>& rule, ruleType type)
    {
        if (type == simpleAddressProxy || type == simpleAddressBypass || type == simpleAddressBlock) return add_simple_address_rule(content, rule);
        else return add_simple_process_rule(content, rule);
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

    bool RouteProfile::add_simple_process_rule(const QString& content, const std::shared_ptr<RouteRule>& rule)
    {
        if (!content.contains(":")) return false;
        auto prefix = content.first(content.indexOf(':'));
        const QString& address = content.section(':', 1);
        if (prefix == "processPath")
        {
            if (!rule->process_path.contains(address)) rule->process_path.append(address);
            return true;
        } else if (prefix == "processName")
        {
            if (!rule->process_name.contains(address)) rule->process_name.append(address);
            return true;
        } else
        {
            return false;
        }
    }

    std::shared_ptr<RouteRule> RouteProfile::get_simple_rule_by_type(ruleType type) {
        for (auto r : Rules) {
            if (r->type == type) return r;
        }
        return nullptr;
    }

    ruleType RouteProfile::get_rule_type(const QString& content, simpleAction action) {
        if (content.startsWith("domain") ||
            content.startsWith("suffix") ||
            content.startsWith("keyword") ||
            content.startsWith("regex") ||
            content.startsWith("ruleset") ||
            content.startsWith("ip")) {
            if (action == proxy) return simpleAddressProxy;
            if (action == direct) return simpleAddressBypass;
            return simpleAddressBlock;
        }
        if (content.startsWith("processName")) {
            if (action == proxy) return simpleProcessNameProxy;
            if (action == direct) return simpleProcessNameBypass;
            return simpleProcessNameBlock;
        }
        if (content.startsWith("processPath")) {
            if (action == proxy) return simpleProcessPathProxy;
            if (action == direct) return simpleProcessPathBypass;
            return simpleProcessPathBlock;
        }
        return custom;
    }
}
