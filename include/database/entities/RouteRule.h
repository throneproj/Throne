#pragma once
#include <QUrl>
#include <QJsonObject>
#include <QSet>

namespace Configs {
    enum inputType {trufalse, select, text};

    enum outboundID {proxyID=-1, directID=-2, blockID=-3};
    inline QString outboundIDToString(int id)
    {
        if (id == proxyID) return {"proxy"};
        if (id == directID) return {"direct"};
        if (id == blockID) return {"block"};
        return {"unknown"};
    }
    inline outboundID stringToOutboundID(const QString& out)
    {
        if (out == "proxy") return proxyID;
        if (out == "direct") return directID;
        if (out == "block") return blockID;
        return proxyID;
    }

    enum ruleType {custom, simpleAddressProxy, simpleAddressBypass, simpleAddressBlock, simpleProcessNameProxy, simpleProcessNameBypass, simpleProcessNameBlock, simpleProcessPathProxy, simpleProcessPathBypass, simpleProcessPathBlock};

    inline QString ruleTypeToString(ruleType type)
    {
        if (type == custom) return {"custom"};
        if (type == simpleAddressProxy) return {"Simple Address Proxy"};
        if (type == simpleAddressBypass) return {"Simple Address Bypass"};
        if (type == simpleAddressBlock) return {"Simple Address Block"};
        if (type == simpleProcessNameProxy) return {"Simple Process Name Proxy"};
        if (type == simpleProcessNameBypass) return {"Simple Process Name Bypass"};
        if (type == simpleProcessNameBlock) return {"Simple Process Name Block"};
        if (type == simpleProcessPathProxy) return {"Simple Process Path Proxy"};
        if (type == simpleProcessPathBypass) return {"Simple Process Path Bypass"};
        if (type == simpleProcessPathBlock) return {"Simple Process Path Block"};
        return {"invalid"};
    }

    inline QString get_rule_set_name(QString ruleSet)
    {
        if (auto url = QUrl(ruleSet); url.isValid() && url.fileName().contains(".srs"))
        {
            return url.fileName().replace(".srs", "-srs") + "-" + QString::number(qHash(url.toEncoded()));
        }
        return ruleSet;
    }

    class RouteRule {
    public:
        RouteRule() = default;

        RouteRule(const RouteRule& other);

        QString name = "";
        int type = custom;
        QString ip_version;
        QString network;
        QString protocol;
        QList<QString> inbound;
        QList<QString> domain;
        QList<QString> domain_suffix;
        QList<QString> domain_keyword;
        QList<QString> domain_regex;
        QList<QString> source_ip_cidr;
        bool source_ip_is_private = false;
        QList<QString> ip_cidr;
        bool ip_is_private = false;
        QList<QString> source_port;
        QList<QString> source_port_range;
        QList<QString> port;
        QList<QString> port_range;
        QList<QString> process_name;
        QList<QString> process_path;
        QList<QString> process_path_regex;
        QList<QString> wifi_ssid;
        QList<QString> wifi_bssid;
        QList<QString> rule_set;
        bool invert = false;
        int outboundID = directID; // -1 is proxy -2 is direct -3 is block -4 is dns_out
        // since sing-box 1.11.0
        QString action = "route";

        // reject options
        QString rejectMethod;
        bool no_drop = false;

        // route options
        QString override_address;
        QString override_port;
        // TODO maybe add some of dial fields?

        // sniff options
        QStringList sniffers;
        bool sniffOverrideDest = false;

        // resolve options
        QString strategy;

        QSet<QString> uiVisibleAttributes;
        bool uiAttributeTabsSeeded = false;
        QString uiActiveAttributeTabLabel;

        [[nodiscard]] QJsonObject get_rule_json(bool forView = false, const QString& outboundTag = "");
        static QStringList get_attributes();
        static QStringList tab_attributes();
        [[nodiscard]] static bool is_attribute_at_default(RouteRule& rule, const QString& attr);
        void clear_attribute_value(const QString& attr);
        void ensure_ui_visible_attribute_tabs_seeded();
        static inputType get_input_type(const QString& fieldName);
        static QStringList get_values_for_field(const QString& fieldName);
        static std::shared_ptr<RouteRule> get_processPath_direct_rule(QString processPath);
        QStringList get_current_value_string(const QString& fieldName);
        [[nodiscard]] QString get_current_value_bool(const QString& fieldName) const;
        void set_field_value(const QString& fieldName, const QStringList& value);
        [[nodiscard]] bool isEmpty();
        bool canEditAttr(const QString &attr);
    };
} // namespace Configs
