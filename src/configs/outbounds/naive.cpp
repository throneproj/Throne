#include "include/configs/outbounds/naive.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool naive::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        username = url.userName();
        password = url.password();
        
        if (query.hasQueryItem("uot")) uot = query.queryItemValue("uot") == "true" || query.queryItemValue("uot").toInt() > 0;
        
        if (url.scheme() == "naive+quic") {
            quic = true;
            if (query.hasQueryItem("congestion_control")) congestion_control = query.queryItemValue("congestion_control");
        }
        
        tls->ParseFromLink(link);
        tls->enabled = true; // Naive always uses tls
        
        if (server_port == 0) server_port = 443;

        return !server.isEmpty();
    }

    bool naive::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "naive") return false;
        outbound::ParseFromJson(object);
        if (object.contains("username")) username = object["username"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("udp_over_tcp"))
        {
            if (object["udp_over_tcp"].isBool()) uot = object["udp_over_tcp"].toBool();
            if (object["udp_over_tcp"].isObject()) uot = object["udp_over_tcp"].toObject()["enabled"].toBool();
        }
        if (object.contains("quic")) quic = object["quic"].toBool();
        if (object.contains("congestion_control")) congestion_control = object["congestion_control"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }

    QString naive::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setUserName(username);
        url.setPassword(password);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (uot) query.addQueryItem("uot", "1");
        if (quic) {
            url.setScheme("naive+quic");
            if(!congestion_control.isEmpty()) query.addQueryItem("congestion_control", congestion_control);
        } else {
            url.setScheme("naive+https");
        }
        
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject naive::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "naive";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (uot) object["udp_over_tcp"] = uot;
        if (quic) {
            object["quic"] = quic;
            if (!congestion_control.isEmpty()) object["quic_congestion_control"] = congestion_control;
        }
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        return object;
    }

    BuildResult naive::Build()
    {
        QJsonObject object;
        object["type"] = "naive";
        mergeJsonObjects(object, outbound::Build().object);
        if (!username.isEmpty()) object["username"] = username;
        if (!password.isEmpty()) object["password"] = password;
        if (uot) object["udp_over_tcp"] = uot;
        if (quic) {
            object["quic"] = quic;
            if (!congestion_control.isEmpty()) object["quic_congestion_control"] = congestion_control;
        }
        if (tls->enabled) object["tls"] = tls->Build().object;
        return {object, ""};
    }

    QString naive::DisplayType()
    {
        return "Naive";
    }
}
