#pragma once

#include <include/configs/proxy/Preset.hpp>

#include "include/global/Configs.hpp"
#include "include/dataStore/RouteEntity.h"
#include <QUrl>

namespace Configs {
    class RouteRule {
    public:
        RouteRule() = default;

        RouteRule(const RouteRule& other);

        QString name = "";
        int type = custom;
        int simpleAction;
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

        [[nodiscard]] QJsonObject get_rule_json(bool forView = false, const QString& outboundTag = "");
        static QStringList get_attributes();
        static inputType get_input_type(const QString& fieldName);
        static QStringList get_values_for_field(const QString& fieldName);
        static std::shared_ptr<RouteRule> get_processPath_direct_rule(QString processPath);
        QStringList get_current_value_string(const QString& fieldName);
        [[nodiscard]] QString get_current_value_bool(const QString& fieldName) const;
        void set_field_value(const QString& fieldName, const QStringList& value);
        [[nodiscard]] bool isEmpty();
    };
} // namespace Configs
