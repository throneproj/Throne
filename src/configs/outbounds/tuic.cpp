#include "include/configs/outbounds/tuic.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool tuic::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        uuid = url.userName();
        password = url.password();
        
        if (query.hasQueryItem("congestion_control")) congestion_control = query.queryItemValue("congestion_control");
        if (query.hasQueryItem("udp_relay_mode")) udp_relay_mode = query.queryItemValue("udp_relay_mode");
        if (query.hasQueryItem("udp_over_stream")) udp_over_stream = query.queryItemValue("udp_over_stream") == "true";
        if (query.hasQueryItem("zero_rtt_handshake")) zero_rtt_handshake = query.queryItemValue("zero_rtt_handshake") == "true";
        if (query.hasQueryItem("heartbeat")) heartbeat = query.queryItemValue("heartbeat");
        
        tls->ParseFromLink(link);
        tls->enabled = true; // TUIC always uses tls
        
        if (server_port == 0) server_port = 443;

        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool tuic::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "tuic") return false;
        outbound::ParseFromJson(object);
        if (object.contains("uuid")) uuid = object["uuid"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("congestion_control")) congestion_control = object["congestion_control"].toString();
        if (object.contains("udp_relay_mode")) udp_relay_mode = object["udp_relay_mode"].toString();
        if (object.contains("udp_over_stream")) udp_over_stream = object["udp_over_stream"].toBool();
        if (object.contains("zero_rtt_handshake")) zero_rtt_handshake = object["zero_rtt_handshake"].toBool();
        if (object.contains("heartbeat")) heartbeat = object["heartbeat"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }

    bool tuic::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "tuic") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        password = QString::fromStdString(object.password);
        if (!object.congestion_controller.empty()) congestion_control = QString::fromStdString(object.congestion_controller);
        if (!object.udp_relay_mode.empty()) udp_relay_mode = QString::fromStdString(object.udp_relay_mode);
        zero_rtt_handshake = object.reduce_rtt;
        if (object.heartbeat_interval > 0) heartbeat = QString::number(object.heartbeat_interval) + "ms";
        if (!object.ip.empty()) server = QString::fromStdString(object.ip);

        tls->ParseFromClash(object);
        tls->enabled = true;
        return true;
    }

    QString tuic::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("tuic");
        url.setUserName(uuid);
        if (!password.isEmpty()) url.setPassword(password);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (!congestion_control.isEmpty()) query.addQueryItem("congestion_control", congestion_control);
        if (!udp_relay_mode.isEmpty() && !udp_over_stream) query.addQueryItem("udp_relay_mode", udp_relay_mode);
        if (udp_over_stream) query.addQueryItem("udp_over_stream", "true");
        if (zero_rtt_handshake) query.addQueryItem("zero_rtt_handshake", "true");
        if (!heartbeat.isEmpty()) query.addQueryItem("heartbeat", heartbeat);
        
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject tuic::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "tuic";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!password.isEmpty()) object["password"] = password;
        if (!congestion_control.isEmpty()) object["congestion_control"] = congestion_control;
        if (!udp_relay_mode.isEmpty() && !udp_over_stream) object["udp_relay_mode"] = udp_relay_mode;
        if (udp_over_stream) object["udp_over_stream"] = udp_over_stream;
        if (zero_rtt_handshake) object["zero_rtt_handshake"] = zero_rtt_handshake;
        if (!heartbeat.isEmpty()) object["heartbeat"] = heartbeat;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        return object;
    }

    BuildResult tuic::Build()
    {
        QJsonObject object;
        object["type"] = "tuic";
        mergeJsonObjects(object, outbound::Build().object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!password.isEmpty()) object["password"] = password;
        if (!congestion_control.isEmpty()) object["congestion_control"] = congestion_control;
        if (!udp_relay_mode.isEmpty() && !udp_over_stream) object["udp_relay_mode"] = udp_relay_mode;
        if (udp_over_stream) object["udp_over_stream"] = udp_over_stream;
        if (zero_rtt_handshake) object["zero_rtt_handshake"] = zero_rtt_handshake;
        if (!heartbeat.isEmpty()) object["heartbeat"] = heartbeat;
        if (tls->enabled) object["tls"] = tls->Build().object;
        return {object, ""};
    }

    QString tuic::DisplayType()
    {
        return "TUIC";
    }
}
