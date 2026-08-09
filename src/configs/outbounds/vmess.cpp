#include "include/configs/outbounds/vmess.h"

#include <QJsonDocument>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool vmess::ParseFromLink(const QString& link)
    {
        // Try V2RayN format first (base64 encoded JSON)
        QString linkN = DecodeB64IfValid(SubStrAfter(link, "vmess://"));
        if (!linkN.isEmpty()) {
            auto objN = QString2QJsonObject(linkN);
            if (!objN.isEmpty()) {
                uuid = objN["id"].toString();
                server = objN["add"].toString();
                server_port = objN["port"].toVariant().toInt();
                name = objN["ps"].toString();
                alter_id = objN["aid"].toVariant().toInt();
                
                QString net = objN["net"].toString();
                if (net == "h2") net = "http";
                if (QString type = objN["type"].toString(); type == "http") net = "http";
                transport->type = net;
                transport->host = objN["host"].toString();
                if (net == "grpc") transport->service_name = objN["path"].toString();
                else transport->path = objN["path"].toString();
                
                QString scy = objN["scy"].toString();
                if (!scy.isEmpty()) security = scy;
                
                QString tlsStr = objN["tls"].toString();
                if (tlsStr == "tls") {
                    tls->enabled = true;
                    tls->server_name = objN["sni"].toString();
                }
                tls->alpn = objN["alpn"].toString().split(',', Qt::SkipEmptyParts);
                const auto insecure = objN["insecure"].toString();
                tls->insecure = insecure == "1" || insecure == "true";
                if (const auto fingerprint = objN["fp"].toString(); !fingerprint.isEmpty()) {
                    tls->utls->enabled = true;
                    tls->utls->fingerPrint = fingerprint;
                }
                
                return !(uuid.isEmpty() || server.isEmpty());
            }
        }
        
        // Standard VMess URL format
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        uuid = url.userName();
        if (server_port == 0) server_port = 443;

        security = GetQueryValue(query, "encryption", "auto");

        transport->ParseFromLink(link);
        
        tls->ParseFromLink(link);
        if (!tls->server_name.isEmpty()) {
            tls->enabled = true;
        }
        
        multiplex->ParseFromLink(link);
        
        if (query.hasQueryItem("alterId")) alter_id = query.queryItemValue("alterId").toInt();
        if (query.hasQueryItem("globalPadding")) global_padding = query.queryItemValue("globalPadding") == "true";
        if (query.hasQueryItem("authenticatedLength")) authenticated_length = query.queryItemValue("authenticatedLength") == "true";
        if (query.hasQueryItem("packetEncoding")) packet_encoding = query.queryItemValue("packetEncoding");
        if (!Configs::vPacketEncoding.contains(packet_encoding)) packet_encoding = "";

        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool vmess::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "vmess") return false;
        outbound::ParseFromJson(object);
        if (object.contains("uuid")) uuid = object["uuid"].toString();
        if (object.contains("security")) security = object["security"].toString();
        if (object.contains("alter_id")) alter_id = object["alter_id"].toInt();
        if (object.contains("alter-id")) alter_id = object["alter-id"].toInt();
        if (object.contains("global_padding")) global_padding = object["global_padding"].toBool();
        if (object.contains("global-padding")) global_padding = object["global-padding"].toBool();
        if (object.contains("authenticated_length")) authenticated_length = object["authenticated_length"].toBool();
        if (object.contains("packet_encoding")) packet_encoding = object["packet_encoding"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        if (object.contains("transport")) transport->ParseFromJson(object["transport"].toObject());
        if (object.contains("multiplex")) multiplex->ParseFromJson(object["multiplex"].toObject());
        return true;
    }

    bool vmess::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "vmess") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        if (!object.cipher.empty()) security = QString::fromStdString(object.cipher);
        alter_id = object.alterId;
        packet_encoding = QString::fromStdString(object.packet_encoding);

        tls->ParseFromClash(object);
        transport->ParseFromClash(object);
        multiplex->ParseFromClash(object);
        return true;
    }

    QString vmess::ExportToLink()
    {
        const auto network = transport->type.isEmpty() || transport->type == "tcp"
                                 ? QStringLiteral("tcp")
                                 : transport->type == "http" ? QStringLiteral("h2") : transport->type;
        const auto path = network == "grpc" ? transport->service_name : transport->path;
        const QJsonObject object{
            {"v", "2"},
            {"ps", name},
            {"add", server},
            {"port", QString::number(server_port)},
            {"id", uuid},
            {"aid", QString::number(alter_id)},
            {"scy", security.isEmpty() ? QStringLiteral("auto") : security},
            {"net", network},
            {"type", "none"},
            {"host", transport->host},
            {"path", path},
            {"tls", tls->enabled ? QStringLiteral("tls") : QString()},
            {"sni", tls->server_name},
            {"alpn", tls->alpn.join(',')},
            {"fp", tls->utls->fingerPrint},
            {"insecure", tls->insecure ? QStringLiteral("1") : QStringLiteral("0")},
        };
        const auto payload = QJsonDocument(object).toJson(QJsonDocument::Compact).toBase64();
        return QStringLiteral("vmess://") + QString::fromLatin1(payload);
    }

    QJsonObject vmess::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "vmess";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (security != "auto") object["security"] = security;
        if (alter_id > 0) object["alter_id"] = alter_id;
        if (global_padding) object["global_padding"] = global_padding;
        if (authenticated_length) object["authenticated_length"] = authenticated_length;
        object["packet_encoding"] = packet_encoding;
        if (auto tlsObj = tls->ExportToJson(); !tlsObj.isEmpty()) object["tls"] = tlsObj;
        if (auto transportObj = transport->ExportToJson(); !transportObj.isEmpty()) object["transport"] = transportObj;
        if (auto muxObj = multiplex->ExportToJson(); !muxObj.isEmpty()) object["multiplex"] = muxObj;
        return object;
    }

    BuildResult vmess::Build()
    {
        QJsonObject object;
        object["type"] = "vmess";
        mergeJsonObjects(object, outbound::Build().object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (security != "auto") object["security"] = security;
        if (alter_id > 0) object["alter_id"] = alter_id;
        if (global_padding) object["global_padding"] = global_padding;
        if (authenticated_length) object["authenticated_length"] = authenticated_length;
        object["packet_encoding"] = packet_encoding;
        if (auto tlsObj = tls->Build().object; !tlsObj.isEmpty()) object["tls"] = tlsObj;
        if (auto transportObj = transport->Build().object; !transportObj.isEmpty()) object["transport"] = transportObj;
        if (auto muxObj = multiplex->Build().object; !muxObj.isEmpty()) object["multiplex"] = muxObj;
        return {object, ""};
    }

    QString vmess::DisplayType()
    {
        return "VMess";
    }

    SecurityInfo vmess::GetSecurity()
    {
        auto info = outbound::GetSecurity();
        // VMess still encrypts its payload without TLS, unless a no-op cipher.
        if (info.level == SecurityLevel::None && security != "none" && security != "zero") {
            info.label = QObject::tr("Encrypted");
            info.level = SecurityLevel::Weak;
        }
        return info;
    }
}
