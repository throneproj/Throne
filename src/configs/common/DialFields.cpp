#include "include/configs/common/DialFields.h"

#include <QCoreApplication>
#include <QUrlQuery>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        QString dialFieldsRule(const QString& key, const QString& value)
        {
            if (key.endsWith(QLatin1String("connect_timeout"))) return Validation::Duration(value);
            if (key.endsWith(QLatin1String("inet4_bind_address"))) return Validation::Ip(value, Validation::Family::V4, Validation::Empty::Allowed);
            return Checks::BindAddress6(value);
        }

        // sing-box DialerOptions keys (option/outbound.go) that Throne has no field for.
        void dialFieldsReportUnmodelled(const QJsonObject& object)
        {
            static const QStringList unmodelled = {"detour", "bind_address_no_port", "protect_path", "routing_mark", "netns",
                                                   "disable_tcp_keep_alive", "tcp_keep_alive", "tcp_keep_alive_interval",
                                                   "domain_resolver", "network_strategy", "network_type", "fallback_network_type",
                                                   "fallback_delay", "tls_fragment", "domain_strategy"};
            // The core matches keys case-insensitively.
            QStringList dropped;
            for (const auto& key : object.keys()) {
                if (unmodelled.contains(key, Qt::CaseInsensitive)) dropped << key.toLower();
            }
            if (dropped.isEmpty()) return;
            auto message = QCoreApplication::translate("Validation", "These dial options are not supported and were ignored: %1.").arg(dropped.join(", "));
            if (dropped.contains("detour")) {
                message += ' ' + QCoreApplication::translate("Validation", "To reach this server through another one, build a chain profile instead of a detour.");
            }
            if (dropped.contains("domain_resolver")) {
                message += ' ' + QCoreApplication::translate("Validation", "Throne picks the resolver for server names itself.");
            }
            ParseDiag::Warn(QString(), message);
        }
    }

    namespace Checks {
        QString BindAddress6(const QString& text)
        {
            const auto t = text.trimmed();
            if (const auto zone = t.indexOf('%'); zone > 0 && zone < t.size() - 1 && !t.mid(zone + 1).contains(':')) {
                return Validation::Ip(t.left(zone), Validation::Family::V6);
            }
            return Validation::Ip(t, Validation::Family::V6, Validation::Empty::Allowed);
        }
    }

    bool DialFields::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("reuse_addr")) reuse_addr = query.queryItemValue("reuse_addr") == "true";
        if (query.hasQueryItem("tcp_fast_open")) tcp_fast_open = query.queryItemValue("tcp_fast_open") == "true";
        if (query.hasQueryItem("tcp_multi_path")) tcp_multi_path = query.queryItemValue("tcp_multi_path") == "true";
        if (query.hasQueryItem("udp_fragment")) udp_fragment = query.queryItemValue("udp_fragment") == "true";
        if (query.hasQueryItem("bind_interface")) bind_interface = query.queryItemValue("bind_interface");
        for (const auto& [key, target] : {std::pair{QStringLiteral("connect_timeout"), &connect_timeout},
                                          std::pair{QStringLiteral("inet4_bind_address"), &inet4_bind_address},
                                          std::pair{QStringLiteral("inet6_bind_address"), &inet6_bind_address}}) {
            if (!query.hasQueryItem(key)) continue;
            if (const auto value = query.queryItemValue(key).trimmed(); AcceptOptional(key, dialFieldsRule(key, value), value)) *target = value;
        }
        return true;
    }
    bool DialFields::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;

        // ParseFromJson is also the DB loader: only an import drops values.
        const bool importing = ParseDiag::Active();
        if (object.contains("reuse_addr")) reuse_addr = object["reuse_addr"].toBool();
        if (object.contains("tcp_fast_open")) tcp_fast_open = object["tcp_fast_open"].toBool();
        if (object.contains("tcp_multi_path")) tcp_multi_path = object["tcp_multi_path"].toBool();
        if (object.contains("udp_fragment")) udp_fragment = object["udp_fragment"].toBool();
        if (object.contains("bind_interface")) bind_interface = object["bind_interface"].toString();
        for (const auto& [key, target] : {std::pair{QStringLiteral("connect_timeout"), &connect_timeout},
                                          std::pair{QStringLiteral("inet4_bind_address"), &inet4_bind_address},
                                          std::pair{QStringLiteral("inet6_bind_address"), &inet6_bind_address}}) {
            if (!object.contains(key)) continue;
            const auto value = object[key].toString().trimmed();
            if (!importing || AcceptOptional(key, dialFieldsRule(key, value), value)) *target = value;
        }
        if (importing) dialFieldsReportUnmodelled(object);
        return true;
    }
    QString DialFields::ExportToLink()
    {
        QUrlQuery query;
        if (reuse_addr) query.addQueryItem("reuse_addr", "true");
        if (!connect_timeout.isEmpty()) query.addQueryItem("connect_timeout", connect_timeout);
        if (tcp_fast_open) query.addQueryItem("tcp_fast_open", "true");
        if (tcp_multi_path) query.addQueryItem("tcp_multi_path", "true");
        if (udp_fragment) query.addQueryItem("udp_fragment", "true");
        if (!bind_interface.isEmpty()) query.addQueryItem("bind_interface", bind_interface);
        if (!inet4_bind_address.isEmpty()) query.addQueryItem("inet4_bind_address", inet4_bind_address);
        if (!inet6_bind_address.isEmpty()) query.addQueryItem("inet6_bind_address", inet6_bind_address);
        return query.toString();
    }
    QJsonObject DialFields::ExportToJson()
    {
        QJsonObject object;
        if (reuse_addr) object["reuse_addr"] = reuse_addr;
        if (!connect_timeout.isEmpty()) object["connect_timeout"] = connect_timeout;
        if (tcp_fast_open) object["tcp_fast_open"] = tcp_fast_open;
        if (tcp_multi_path) object["tcp_multi_path"] = tcp_multi_path;
        if (udp_fragment) object["udp_fragment"] = udp_fragment;
        if (!bind_interface.isEmpty()) object["bind_interface"] = bind_interface;
        if (!inet4_bind_address.isEmpty()) object["inet4_bind_address"] = inet4_bind_address;
        if (!inet6_bind_address.isEmpty()) object["inet6_bind_address"] = inet6_bind_address;
        return object;
    }
    BuildResult DialFields::Build()
    {
        return {ExportToJson(), ""};
    }

    void DialFields::Validate(Validation::Issues& issues)
    {
        for (const auto& [key, value] : {std::pair{QStringLiteral("connect_timeout"), connect_timeout},
                                         std::pair{QStringLiteral("inet4_bind_address"), inet4_bind_address},
                                         std::pair{QStringLiteral("inet6_bind_address"), inet6_bind_address}}) {
            if (const auto message = dialFieldsRule(key, value); !message.isEmpty()) issues.append({Validation::Severity::Error, key, message, value});
        }
    }
}


