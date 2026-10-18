#include "include/configs/outbounds/vless.h"

#include <QCoreApplication>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // Share links write none for no flow; the core knows only the empty value and matches names exactly.
        QString vlessImportFlow(const QString &raw)
        {
            const QString value = raw.trimmed().toLower();
            return value == "none" ? QString() : value;
        }

        // Throne's own links write none for the empty encoding.
        QString vlessImportPacketEncoding(const QString &raw)
        {
            const QString value = raw.trimmed();
            if (value == "none") return {};
            if (vPacketEncoding.contains(value)) return value;
            ParseDiag::Warn("packet_encoding", QCoreApplication::translate("Validation", "Unknown packet encoding, so none was used. Use packetaddr or xudp."), value);
            return {};
        }
    }

    namespace Checks {
        QString VlessFlow(const QString &flow, bool tlsEnabled, const QString &transportType)
        {
            if (flow.isEmpty()) return {};
            if (flow == "xtls-rprx-vision-udp443")
                return QCoreApplication::translate("Validation", "sing-box does not support xtls-rprx-vision-udp443; use xtls-rprx-vision, or the Xray VLESS type.");
            if (flow != "xtls-rprx-vision") return Validation::OneOf(flow, vlessFlows);
            if (!tlsEnabled || !(transportType.isEmpty() || transportType == "tcp"))
                return QCoreApplication::translate("Validation", "XTLS Vision works only with TLS or REALITY directly over TCP: turn on TLS and set the network to tcp, or clear the flow.");
            return {};
        }
    }

    bool vless::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        uuid = url.userName();
        if (server_port == 0) server_port = 443;

        flow = vlessImportFlow(query.queryItemValue("flow"));

        transport->ParseFromLink(link);

        tls->ParseFromLink(link);

        const auto packetEncoding = query.queryItemValue("packetEncoding");
        packet_encoding = packetEncoding.isEmpty() ? QStringLiteral("xudp") : vlessImportPacketEncoding(packetEncoding);

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
        if (ParseDiag::Active()) {
            flow = vlessImportFlow(flow);
            packet_encoding = vlessImportPacketEncoding(packet_encoding);
        }
        return true;
    }

    bool vless::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "vless") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        if (!object.flow.empty()) flow = vlessImportFlow(QString::fromStdString(object.flow));
        packet_encoding = vlessImportPacketEncoding(QString::fromStdString(object.packet_encoding));

        tls->ParseFromClash(object, object.tls);
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
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!flow.isEmpty()) object["flow"] = flow;
        object["packet_encoding"] = packet_encoding;
        const auto tlsResult = tls->Build();
        if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
        if (!tlsResult.object.isEmpty()) object["tls"] = TlsForTransport(tlsResult.object, transport->type);
        const auto transportResult = transport->Build();
        if (!transportResult.error.isEmpty()) return {{}, transportResult.error};
        if (!transportResult.object.isEmpty()) object["transport"] = transportResult.object;
        const auto muxResult = multiplex->Build();
        if (!muxResult.error.isEmpty()) return {{}, muxResult.error};
        if (!muxResult.object.isEmpty()) object["multiplex"] = muxResult.object;
        return {object, ""};
    }

    QString vless::DisplayType()
    {
        return "VLESS";
    }

    void vless::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        if (auto message = Validation::Required(uuid); !message.isEmpty()) issues.append({Severity::Error, "uuid", message});
        if (auto message = Checks::VlessFlow(flow, tls->enabled, transport->type); !message.isEmpty()) issues.append({Severity::Error, "flow", message, flow});
        if (auto message = Validation::OneOf(packet_encoding, vPacketEncoding); !message.isEmpty())
            issues.append({Severity::Error, "packet_encoding", message, packet_encoding});
    }
}
