#include "include/configs/outbounds/tailscale.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // Import only: a route the core would refuse is dropped with a warning; DB loads and clones keep it.
        QStringList tailscaleKeepRoutes(const QStringList& routes)
        {
            QStringList kept;
            for (const auto& raw : routes) {
                const auto route = raw.trimmed();
                if (route.isEmpty()) continue;
                if (!ParseDiag::Active() || AcceptOptional("advertise_routes", Checks::TailscaleRoute(route), route)) kept << route;
            }
            return kept;
        }
    }

    namespace Checks {
        QString TailscaleControlUrl(const QString& text)
        {
            return Validation::Url(text, {"https", "http"});
        }

        // sing-box protocol/tailscale/endpoint.go: an unspecified address with prefix length 0.
        QString TailscaleRoute(const QString& item)
        {
            if (auto message = Validation::Cidr(item); !message.isEmpty()) return message;
            const auto route = item.trimmed();
            const auto slash = route.lastIndexOf('/');
            const QHostAddress address(route.left(slash));
            if (route.mid(slash + 1) == "0" && (address == QHostAddress(QHostAddress::AnyIPv4) || address == QHostAddress(QHostAddress::AnyIPv6))) {
                return QCoreApplication::translate("Validation", "Do not advertise the default route; tick Advertise exit node instead.");
            }
            return {};
        }

        QString TailscaleExitNode(const QString& exitNode, bool advertiseExitNode)
        {
            if (!advertiseExitNode || exitNode.trimmed().isEmpty()) return {};
            return QCoreApplication::translate("Validation", "A node that advertises itself as an exit node cannot use another one; clear the exit node or untick Advertise exit node.");
        }
    }

    bool tailscale::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);

        if (query.hasQueryItem("state_directory")) state_directory = query.queryItemValue("state_directory", QUrl::FullyDecoded);
        if (query.hasQueryItem("auth_key")) auth_key = query.queryItemValue("auth_key", QUrl::FullyDecoded);
        if (query.hasQueryItem("control_url")) control_url = query.queryItemValue("control_url", QUrl::FullyDecoded);
        if (query.hasQueryItem("ephemeral")) ephemeral = query.queryItemValue("ephemeral") == "true";
        if (query.hasQueryItem("hostname")) hostname = query.queryItemValue("hostname", QUrl::FullyDecoded);
        if (query.hasQueryItem("accept_routes")) accept_routes = query.queryItemValue("accept_routes") == "true";
        if (query.hasQueryItem("exit_node")) exit_node = query.queryItemValue("exit_node");
        if (query.hasQueryItem("exit_node_allow_lan_access")) exit_node_allow_lan_access = query.queryItemValue("exit_node_allow_lan_access") == "true";
        if (query.hasQueryItem("advertise_routes")) {
            advertise_routes = tailscaleKeepRoutes(SplitAndTrim(query.queryItemValue("advertise_routes", QUrl::FullyDecoded), ",", false));
        }
        if (query.hasQueryItem("advertise_exit_node")) advertise_exit_node = query.queryItemValue("advertise_exit_node") == "true";
        if (query.hasQueryItem("globalDNS")) globalDNS = query.queryItemValue("globalDNS") == "true";
        if (query.hasQueryItem("global_dns")) globalDNS = query.queryItemValue("global_dns") == "true";

        return true;
    }

    bool tailscale::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "tailscale") return false;
        outbound::ParseFromJson(object);
        if (object.contains("state_directory")) state_directory = object["state_directory"].toString();
        if (object.contains("auth_key")) auth_key = object["auth_key"].toString();
        if (object.contains("control_url")) control_url = object["control_url"].toString();
        if (object.contains("ephemeral")) ephemeral = object["ephemeral"].toBool();
        if (object.contains("hostname")) hostname = object["hostname"].toString();
        if (object.contains("accept_routes")) accept_routes = object["accept_routes"].toBool();
        if (object.contains("exit_node")) exit_node = object["exit_node"].toString();
        if (object.contains("exit_node_allow_lan_access")) exit_node_allow_lan_access = object["exit_node_allow_lan_access"].toBool();
        // Older editor saves stored [""] for an empty field, which the core rejects.
        if (object.contains("advertise_routes")) advertise_routes = tailscaleKeepRoutes(QJsonArray2QListString(object["advertise_routes"].toArray()));
        if (object.contains("advertise_exit_node")) advertise_exit_node = object["advertise_exit_node"].toBool();
        if (object.contains("globalDNS")) globalDNS = object["globalDNS"].toBool();
        return true;
    }

    QString tailscale::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("ts");
        url.setHost("tailscale");
        if (!name.isEmpty()) url.setFragment(name);

        if (!state_directory.isEmpty()) query.addQueryItem("state_directory", QUrl::toPercentEncoding(state_directory));
        if (!auth_key.isEmpty()) query.addQueryItem("auth_key", QUrl::toPercentEncoding(auth_key));
        if (!control_url.isEmpty()) query.addQueryItem("control_url", QUrl::toPercentEncoding(control_url));
        if (ephemeral) query.addQueryItem("ephemeral", "true");
        if (!hostname.isEmpty()) query.addQueryItem("hostname", QUrl::toPercentEncoding(hostname));
        if (accept_routes) query.addQueryItem("accept_routes", "true");
        if (!exit_node.isEmpty()) query.addQueryItem("exit_node", exit_node);
        if (exit_node_allow_lan_access) query.addQueryItem("exit_node_allow_lan_access", "true");
        if (!advertise_routes.isEmpty()) query.addQueryItem("advertise_routes", QUrl::toPercentEncoding(advertise_routes.join(",")));
        if (advertise_exit_node) query.addQueryItem("advertise_exit_node", "true");
        if (globalDNS) query.addQueryItem("global_dns", "true");
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject tailscale::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "tailscale";
        object["tag"] = name;
        if (!state_directory.isEmpty()) object["state_directory"] = state_directory;
        if (!auth_key.isEmpty()) object["auth_key"] = auth_key;
        if (!control_url.isEmpty()) object["control_url"] = control_url;
        if (ephemeral) object["ephemeral"] = ephemeral;
        if (!hostname.isEmpty()) object["hostname"] = hostname;
        if (accept_routes) object["accept_routes"] = accept_routes;
        if (!exit_node.isEmpty()) object["exit_node"] = exit_node;
        if (exit_node_allow_lan_access) object["exit_node_allow_lan_access"] = exit_node_allow_lan_access;
        if (!advertise_routes.isEmpty()) object["advertise_routes"] = QListStr2QJsonArray(advertise_routes);
        if (advertise_exit_node) object["advertise_exit_node"] = advertise_exit_node;
        if (globalDNS) object["globalDNS"] = globalDNS;
        return object;
    }

    BuildResult tailscale::Build()
    {
        QJsonObject object;
        object["type"] = "tailscale";
        if (!state_directory.isEmpty()) object["state_directory"] = state_directory;
        if (!auth_key.isEmpty()) object["auth_key"] = auth_key;
        if (!control_url.isEmpty()) object["control_url"] = control_url;
        if (ephemeral) object["ephemeral"] = ephemeral;
        if (!hostname.isEmpty()) object["hostname"] = hostname;
        if (accept_routes) object["accept_routes"] = accept_routes;
        if (!exit_node.isEmpty()) object["exit_node"] = exit_node;
        if (exit_node_allow_lan_access) object["exit_node_allow_lan_access"] = exit_node_allow_lan_access;
        if (!advertise_routes.isEmpty()) object["advertise_routes"] = QListStr2QJsonArray(advertise_routes);
        if (advertise_exit_node) object["advertise_exit_node"] = advertise_exit_node;
        return {object, ""};
    }

    void tailscale::SetAddress(QString newAddr) {
        control_url = newAddr;
    }

    QString tailscale::GetAddress() {
        return control_url;
    }

    QString tailscale::DisplayAddress()
    {
        return control_url;
    }

    QString tailscale::DisplayType()
    {
        return "Tailscale";
    }

    SecurityInfo tailscale::GetSecurity()
    {
        return {QObject::tr("Encrypted"), "WireGuard", SecurityLevel::Secure};
    }

    bool tailscale::IsEndpoint()
    {
        return true;
    }

    void tailscale::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        const auto report = [&issues](const QString& field, const QString& message, const QString& value = {}) {
            if (!message.isEmpty()) issues << Validation::Issue{Validation::Severity::Error, field, message, value};
        };
        report("control_url", Checks::TailscaleControlUrl(control_url), control_url);
        for (const auto& route : advertise_routes) {
            if (const auto message = Checks::TailscaleRoute(route); !message.isEmpty()) {
                report("advertise_routes", message, route);
                break;
            }
        }
        report("exit_node", Checks::TailscaleExitNode(exit_node, advertise_exit_node), exit_node);
    }
}
