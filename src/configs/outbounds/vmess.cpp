#include "include/configs/outbounds/vmess.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // The core matches security names exactly.
        QString vmessImportSecurity(const QString &raw)
        {
            const QString value = raw.trimmed().toLower();
            if (value.isEmpty()) return QStringLiteral("auto");
            if (vmessSecurity.contains(value)) return value;
            ParseDiag::Warn("security", QCoreApplication::translate("Validation", "Unknown security, so auto was used. Use auto, none, zero, aes-128-gcm, chacha20-poly1305 or aes-128-cfb."), raw.trimmed());
            return QStringLiteral("auto");
        }

        // Throne's own links write none for the empty encoding.
        QString vmessImportPacketEncoding(const QString &raw)
        {
            const QString value = raw.trimmed();
            if (value == "none") return {};
            if (vPacketEncoding.contains(value)) return value;
            ParseDiag::Warn("packet_encoding", QCoreApplication::translate("Validation", "Unknown packet encoding, so none was used. Use packetaddr or xudp."), value);
            return {};
        }
    }

    bool vmess::ParseFromLink(const QString& link)
    {
        // Try V2RayN format first (base64 encoded JSON); some generators append #name or use the URL-safe alphabet.
        const QString payload = SubStrBefore(SubStrAfter(link, "vmess://"), "#");
        QString linkN = DecodeB64IfValid(payload);
        if (linkN.isEmpty()) linkN = DecodeB64IfValid(payload, QByteArray::Base64UrlEncoding);
        if (!linkN.isEmpty()) {
            auto objN = QString2QJsonObject(linkN);
            if (!objN.isEmpty()) {
                uuid = objN["id"].toString();
                server = toAceHost(objN["add"].toString());
                server_port = objN["port"].toVariant().toInt();
                name = objN["ps"].toString();
                alter_id = objN["aid"].toVariant().toInt();
                
                QString net = SingBoxTransportType(objN["net"].toString());
                if (net == "raw") net = "tcp";
                const bool rawHttp = (net.isEmpty() || net == "tcp") && objN["type"].toString() == "http";
                if (net == "h2" || rawHttp) net = "http";
                transport->type = net;
                transport->host = objN["host"].toString();
                if (net == "grpc") transport->service_name = objN["path"].toString();
                else transport->path = objN["path"].toString();
                if (rawHttp) {
                    transport->method = "GET";
                    transport->path = transport->path.section(',', 0, 0, QString::SectionSkipEmpty).trimmed();
                }
                
                QString scy = objN["scy"].toString();
                if (!scy.isEmpty()) security = vmessImportSecurity(scy);

                const QString tlsStr = objN["tls"].toString().trimmed();
                if (!tlsStr.isEmpty() && tlsStr.compare("tls", Qt::CaseInsensitive) != 0 && tlsStr.compare("none", Qt::CaseInsensitive) != 0) {
                    ParseDiag::Error("tls", QCoreApplication::translate("Validation", "Unsupported TLS mode; a VMess link can use only tls or none."), tlsStr);
                }
                if (tlsStr.compare("tls", Qt::CaseInsensitive) == 0) {
                    tls->enabled = true;
                    tls->server_name = objN["sni"].toString();
                    tls->alpn = SplitAndTrim(objN["alpn"].toString(), ",", false);
                    const auto insecure = objN.contains("insecure")
                                              ? objN["insecure"].toVariant().toString()
                                              : objN["allowInsecure"].toVariant().toString();
                    tls->insecure = insecure == "1" || insecure == "true";
                    if (const auto fingerprint = objN["fp"].toString(); !fingerprint.trimmed().isEmpty()) {
                        tls->utls->fingerPrint = ImportTlsFingerprint(fingerprint, "fp");
                        tls->utls->enabled = !tls->utls->fingerPrint.isEmpty();
                    }
                    tls->cipher_suites = ImportTlsCipherSuites(splitCipherSuites(objN["cs"].toString()), "cs");
                }

                // throneExtra holds what the V2RayN schema cannot; the dummy authority only satisfies QUrl.
                if (const auto extra = objN["throneExtra"].toString(); !extra.isEmpty()) {
                    const QString extraLink = "vmess://x@127.0.0.1:1?" + extra;
                    transport->ParseFromLink(extraLink);
                    multiplex->ParseFromLink(extraLink);
                    const auto extraQuery = QUrlQuery(extra);
                    if (extraQuery.hasQueryItem("globalPadding")) global_padding = extraQuery.queryItemValue("globalPadding") == "true";
                    if (extraQuery.hasQueryItem("authenticatedLength")) authenticated_length = extraQuery.queryItemValue("authenticatedLength") == "true";
                    if (extraQuery.hasQueryItem("packetEncoding")) packet_encoding = vmessImportPacketEncoding(extraQuery.queryItemValue("packetEncoding"));
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

        security = vmessImportSecurity(query.queryItemValue("encryption"));

        transport->ParseFromLink(link);
        
        tls->ParseFromLink(link);

        multiplex->ParseFromLink(link);
        
        if (query.hasQueryItem("alterId")) alter_id = query.queryItemValue("alterId").toInt();
        if (query.hasQueryItem("globalPadding")) global_padding = query.queryItemValue("globalPadding") == "true";
        if (query.hasQueryItem("authenticatedLength")) authenticated_length = query.queryItemValue("authenticatedLength") == "true";
        if (query.hasQueryItem("packetEncoding")) packet_encoding = vmessImportPacketEncoding(query.queryItemValue("packetEncoding"));

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
        if (ParseDiag::Active()) {
            security = vmessImportSecurity(security);
            packet_encoding = vmessImportPacketEncoding(packet_encoding);
        }
        return true;
    }

    bool vmess::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "vmess") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        if (!object.cipher.empty()) security = vmessImportSecurity(QString::fromStdString(object.cipher));
        alter_id = object.alterId;
        packet_encoding = vmessImportPacketEncoding(QString::fromStdString(object.packet_encoding));

        tls->ParseFromClash(object, object.tls);
        transport->ParseFromClash(object);
        multiplex->ParseFromClash(object);
        return true;
    }

    QString vmess::ExportToLink()
    {
        const bool rawHttp = transport->type == "http" && !tls->enabled;
        const auto network = transport->type.isEmpty() || transport->type == "tcp" || rawHttp
                                 ? QStringLiteral("tcp")
                                 : transport->type == "http" ? QStringLiteral("h2") : transport->type;
        const auto path = network == "grpc" ? transport->service_name : transport->path;

        // What the V2RayN schema has no room for; other clients ignore the unknown key.
        QUrlQuery extra;
        mergeUrlQuery(extra, transport->ExportToLink(tls->enabled));
        mergeUrlQuery(extra, multiplex->ExportToLink());
        if (global_padding) extra.addQueryItem("globalPadding", "true");
        if (authenticated_length) extra.addQueryItem("authenticatedLength", "true");
        if (packet_encoding != "xudp") {
            extra.addQueryItem("packetEncoding", packet_encoding.isEmpty() ? "none" : packet_encoding);
        }

        QJsonObject object{
            {"v", "2"},
            {"ps", name},
            {"add", server},
            {"port", QString::number(server_port)},
            {"id", uuid},
            {"aid", QString::number(alter_id)},
            {"scy", security.isEmpty() ? QStringLiteral("auto") : security},
            {"net", network},
            {"type", rawHttp ? QStringLiteral("http") : QStringLiteral("none")},
            {"host", transport->host},
            {"path", path},
            {"tls", tls->enabled ? QStringLiteral("tls") : QString()},
            {"sni", tls->server_name},
            {"alpn", tls->alpn.join(',')},
            {"fp", tls->utls->fingerPrint},
        };
        if (!tls->cipher_suites.isEmpty()) object["cs"] = tls->cipher_suites.join(':');
        if (tls->insecure) object["allowInsecure"] = QStringLiteral("1");
        if (!extra.isEmpty()) object["throneExtra"] = extra.toString(QUrl::FullyEncoded);
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
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (security != "auto") object["security"] = security;
        if (alter_id > 0) object["alter_id"] = alter_id;
        if (global_padding) object["global_padding"] = global_padding;
        if (authenticated_length) object["authenticated_length"] = authenticated_length;
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

    QString vmess::DisplayType()
    {
        return "VMess";
    }

    void vmess::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        if (auto message = Validation::Required(uuid); !message.isEmpty()) issues.append({Severity::Error, "uuid", message});
        if (auto message = Validation::OneOf(security, vmessSecurity); !message.isEmpty()) issues.append({Severity::Error, "security", message, security});
        if (auto message = Validation::OneOf(packet_encoding, vPacketEncoding); !message.isEmpty())
            issues.append({Severity::Error, "packet_encoding", message, packet_encoding});
    }

    SecurityInfo vmess::GetSecurity()
    {
        auto info = outbound::GetSecurity();
        // Encrypted, but VMess without TLS still counts as insecure; a no-op cipher stays Raw.
        if (info.level == SecurityLevel::None && security != "none" && security != "zero") {
            info.label = QObject::tr("Insecure");
            info.level = SecurityLevel::Weak;
        }
        return info;
    }
}
