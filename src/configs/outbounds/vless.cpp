#include "include/configs/outbounds/vless.h"

#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool vless::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        uuid = url.userName();
        if (server_port == 0) server_port = 443;

        flow = GetQueryValue(query, "flow", "");

        transport->ParseFromLink(link);
        
        tls->ParseFromLink(link);
        if (!tls->server_name.isEmpty()) {
            tls->enabled = true;
        }
        
        packet_encoding = GetQueryValue(query, "packetEncoding", "xudp");
        if (!Configs::vPacketEncoding.contains(packet_encoding)) packet_encoding = "";

        multiplex->ParseFromLink(link);

        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool vless::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "vless") return false;
        outbound::ParseFromJson(object);
        if (object.contains("uuid")) uuid = object["uuid"].toString();
        if (object.contains("flow")) flow = object["flow"].toString();
        if (object.contains("packet_encoding")) packet_encoding = object["packet_encoding"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        if (object.contains("transport")) transport->ParseFromJson(object["transport"].toObject());
        if (object.contains("multiplex")) multiplex->ParseFromJson(object["multiplex"].toObject());
        return true;
    }

    bool vless::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "vless") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        if (!object.flow.empty()) flow = QString::fromStdString(object.flow);
        packet_encoding = QString::fromStdString(object.packet_encoding);

        tls->ParseFromClash(object);
        transport->ParseFromClash(object);
        multiplex->ParseFromClash(object);
        return true;
    }

    QString vless::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("vless");
        url.setUserName(uuid);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        query.addQueryItem("encryption", "none");
        if (!flow.isEmpty()) query.addQueryItem("flow", flow);

        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, transport->ExportToLink(tls->enabled));
        mergeUrlQuery(query, multiplex->ExportToLink());
        
        query.addQueryItem("packetEncoding", packet_encoding.isEmpty() ? "none" : packet_encoding);
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject vless::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "vless";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!flow.isEmpty()) object["flow"] = flow;
        object["packet_encoding"] = packet_encoding;
        if (auto tlsObj = tls->ExportToJson(); !tlsObj.isEmpty()) object["tls"] = tlsObj;
        if (auto transportObj = transport->ExportToJson(); !transportObj.isEmpty()) object["transport"] = transportObj;
        if (auto muxObj = multiplex->ExportToJson(); !muxObj.isEmpty()) object["multiplex"] = muxObj;
        return object;
    }

    BuildResult vless::Build()
    {
        QJsonObject object;
        object["type"] = "vless";
        mergeJsonObjects(object, outbound::Build().object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!flow.isEmpty()) object["flow"] = flow;
        object["packet_encoding"] = packet_encoding;
        if (auto tlsObj = tls->Build().object; !tlsObj.isEmpty()) object["tls"] = tlsObj;
        if (auto transportObj = transport->Build().object; !transportObj.isEmpty()) object["transport"] = transportObj;
        if (auto muxObj = multiplex->Build().object; !muxObj.isEmpty()) object["multiplex"] = muxObj;
        return {object, ""};
    }

    QString vless::DisplayType()
    {
        return "VLESS";
    }
}
