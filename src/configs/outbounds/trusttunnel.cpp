#include "include/configs/outbounds/trusttunnel.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool trusttunnel::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        username = url.userName();
        password = url.password();
        
        if (query.hasQueryItem("health_check")) health_check = query.queryItemValue("health_check") == "true";
        if (query.hasQueryItem("congestion_control")) {
            quic = true;
            congestion_control = query.queryItemValue("congestion_control");
        }
        
        tls->ParseFromLink(link);
        tls->enabled = true; // TrustTunnel always uses tls
        
        if (server_port == 0) server_port = 443;

        return !(username.isEmpty() || password.isEmpty() || server.isEmpty());
    }

    bool trusttunnel::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "trusttunnel") return false;
        outbound::ParseFromJson(object);
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("health_check")) health_check = object["health_check"].toBool();
        if (object.contains("quic")) quic = object["quic"].toBool();
        if (object.contains("congestion_control")) congestion_control = object["congestion_control"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }

    QString trusttunnel::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("tt");
        url.setUserName(username);
        url.setPassword(password);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (health_check) query.addQueryItem("health_check", "true");
        if (quic && !congestion_control.isEmpty()) query.addQueryItem("congestion_control", congestion_control);
        
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject trusttunnel::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "trusttunnel";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (health_check) object["health_check"] = health_check;
        if (quic) {
            object["quic"] = quic;
            if (!congestion_control.isEmpty()) object["quic_congestion_control"] = congestion_control;
        }
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        return object;
    }

    BuildResult trusttunnel::Build()
    {
        QJsonObject object;
        object["type"] = "trusttunnel";
        mergeJsonObjects(object, outbound::Build().object);
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (health_check) object["health_check"] = health_check;
        if (quic) {
            object["quic"] = quic;
            if (!congestion_control.isEmpty()) object["quic_congestion_control"] = congestion_control;
        }
        if (tls->enabled) object["tls"] = tls->Build().object;
        return {object, ""};
    }

    QString trusttunnel::DisplayType()
    {
        return "TrustTunnel";
    }
}
