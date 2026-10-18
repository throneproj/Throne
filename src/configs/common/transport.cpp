#include "include/configs/common/transport.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"
#include "include/configs/ParseDiagnostics.h"

namespace Configs {
    bool Transport::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        const auto linkType = SingBoxTransportType(query.queryItemValue("type"));
        const bool rawHttp = (linkType.isEmpty() || linkType == "tcp" || linkType == "raw")
                             && query.queryItemValue("headerType") == "http";
        if (query.hasQueryItem("type")) type = linkType == "raw" ? QStringLiteral("tcp") : linkType;
        if (rawHttp || type == "h2") {
            type = "http";
            method = "GET";
        }
        if (query.hasQueryItem("host")) host = query.queryItemValue("host", QUrl::FullyDecoded);
        if (query.hasQueryItem("path")) path = query.queryItemValue("path", QUrl::FullyDecoded);
        // the raw header's server accepts any of its comma-listed paths, while sing-box sends exactly one
        if (rawHttp) path = path.section(',', 0, 0, QString::SectionSkipEmpty).trimmed();
        if (query.hasQueryItem("method")) {
            if (const auto value = query.queryItemValue("method").trimmed(); AcceptOptional("method", Validation::HttpToken(value), value)) method = value;
        }
        if (const auto raw = query.queryItemValue("headers", QUrl::FullyDecoded); !raw.isEmpty()) {
            headers = raw.split('|');
            if (headers.size() % 2 != 0) {
                ParseDiag::Warn("headers", QCoreApplication::translate("Validation", "The headers must alternate names and values separated by |, for example User-Agent|curl, so they were ignored."));
                headers.clear();
            }
        }
        for (const auto& [key, target] : {std::pair{QStringLiteral("idle_timeout"), &idle_timeout}, std::pair{QStringLiteral("ping_timeout"), &ping_timeout}}) {
            if (!query.hasQueryItem(key)) continue;
            if (const auto value = query.queryItemValue(key).trimmed(); AcceptOptional(key, Validation::Duration(value), value)) *target = value;
        }
        ReadQueryInt(query, "max_early_data", max_early_data);
        if (query.hasQueryItem("early_data_header_name")) early_data_header_name = query.queryItemValue("early_data_header_name");
        if (query.hasQueryItem("serviceName")) service_name = query.queryItemValue("serviceName", QUrl::FullyDecoded);
        return true;
    }
    bool Transport::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("type")) type = SingBoxTransportType(object["type"].toString());
        if (object.contains("host")) {
            host = object["host"].isArray() ? QJsonArray2QListString(object["host"].toArray()).join(',') : object["host"].toString();
        }
        if (object.contains("path")) path = object["path"].toString();
        // ParseFromJson is also the DB loader: only an import drops values.
        const bool importing = ParseDiag::Active();
        if (object.contains("method")) {
            const auto value = object["method"].toString().trimmed();
            if (!importing || AcceptOptional("transport.method", Validation::HttpToken(value), value)) method = value;
        }
        if (object.contains("headers") && object["headers"].isObject()) {
            auto headerObj = object["headers"].toObject();
            if (type == "ws") {
                if (headerObj.contains("Host")) {
                    if (headerObj["Host"].isString()) {
                        host = headerObj["Host"].toString();
                    } else if (headerObj["Host"].isArray()) {
                        for (const auto& v : headerObj["Host"].toArray()) {
                            if (v.isString()) {
                                host = v.toString();
                                break;
                            }
                        }
                    }
                    headerObj.remove("Host");
                }
            }
            headers = jsonObjectToQStringList(headerObj);
        }
        for (const auto& [key, target] : {std::pair{QStringLiteral("idle_timeout"), &idle_timeout}, std::pair{QStringLiteral("ping_timeout"), &ping_timeout}}) {
            if (!object.contains(key)) continue;
            const auto value = object[key].toString().trimmed();
            if (!importing || AcceptOptional("transport." + key, Validation::Duration(value), value)) *target = value;
        }
        if (object.contains("max_early_data")) max_early_data = object["max_early_data"].toInt();
        if (object.contains("early_data_header_name")) early_data_header_name = object["early_data_header_name"].toString();
        if (object.contains("service_name")) service_name = object["service_name"].toString();
        return true;
    }
    bool Transport::ParseFromClash(const clash::Proxies& object)
    {
        if (!object.ws_opts.path.empty() || object.network == "ws") {
            if (object.ws_opts.v2ray_http_upgrade) {
                type = "httpupgrade";
            } else {
                type = "ws";
                early_data_header_name = QString::fromStdString(object.ws_opts.early_data_header_name);
                max_early_data = object.ws_opts.max_early_data;
            }
            QString headerHost;
            for (const auto& [key, value] : object.ws_opts.headers) {
                const auto name = QString::fromStdString(key);
                // Host belongs in the host field, which ExportToJson writes back as the ws Host header.
                if (name.compare(QLatin1String("host"), Qt::CaseInsensitive) == 0) {
                    if (headerHost.isEmpty()) headerHost = QString::fromStdString(value);
                    continue;
                }
                headers << name << QString::fromStdString(value);
            }
            // mihomo sends the ws-opts Host header as the HTTP Host and uses servername only for the SNI.
            if (!headerHost.isEmpty()) {
                host = headerHost;
            } else if (!object.servername.empty()) {
                host = QString::fromStdString(object.servername);
            }
            path = QString::fromStdString(object.ws_opts.path);
            return true;
        }
        if (!object.grpc_opts.grpc_service_name.empty()) {
            type = "grpc";
            service_name = QString::fromStdString(object.grpc_opts.grpc_service_name);
            return true;
        }
        if (!object.h2_opts.path.empty() || object.network == "h2") {
            type = "http";
            if (!object.h2_opts.host.empty()) {
                host = QString::fromStdString(object.h2_opts.host[0]);
            }
            path = QString::fromStdString(object.h2_opts.path);
            return true;
        }
        if (!object.http_opts.method.empty()) {
            type = "http";
            if (object.http_opts.headers.contains("Host") && !object.http_opts.headers.at("Host").empty()) {
                host = QString::fromStdString(object.http_opts.headers.at("Host")[0]);
            }
            if (!object.http_opts.path.empty()) {
                path = QString::fromStdString(object.http_opts.path[0]);
            }
            method = QString::fromStdString(object.http_opts.method);
            return true;
        }
        return false;
    }
    QString Transport::ExportToLink()
    {
        return ExportToLink(true);
    }
    QString Transport::ExportToLink(bool tlsEnabled)
    {
        QUrlQuery query;
        if (type.isEmpty() || type == "tcp") return "";
        // plaintext, the http transport is the raw HTTP header on the wire; other clients read type=http as h2
        if (type == "http" && !tlsEnabled) {
            query.addQueryItem("type", "tcp");
            query.addQueryItem("headerType", "http");
        } else {
            query.addQueryItem("type", type);
        }
        if (!host.isEmpty()) query.addQueryItem("host", host);
        if (!path.isEmpty()) query.addQueryItem("path", path);
        if (!method.isEmpty()) query.addQueryItem("method", method);
        if (!headers.isEmpty()) query.addQueryItem("headers", headers.join("|"));
        if (!idle_timeout.isEmpty()) query.addQueryItem("idle_timeout", idle_timeout);
        if (!ping_timeout.isEmpty()) query.addQueryItem("ping_timeout", ping_timeout);
        if (max_early_data > 0) query.addQueryItem("max_early_data", QString::number(max_early_data));
        if (!early_data_header_name.isEmpty()) query.addQueryItem("early_data_header_name", early_data_header_name);
        if (!service_name.isEmpty()) query.addQueryItem("serviceName", service_name);
        return query.toString(QUrl::FullyEncoded);
    }
    QJsonObject Transport::ExportToJson()
    {
        QJsonObject object;
        if (type.isEmpty() || type == "tcp") return object;
        if (!type.isEmpty()) object["type"] = type;
        if (path.contains("?ed=")) {
            auto spl = path.split("?ed=");
            object["path"] = spl[0];
            object["max_early_data"] = spl[1].toInt();
            object["early_data_header_name"] = "Sec-WebSocket-Protocol";
        } else {
            if (!path.isEmpty()) object["path"] = path;
            if (max_early_data > 0) object["max_early_data"] = max_early_data;
            if (!early_data_header_name.isEmpty()) object["early_data_header_name"] = early_data_header_name;
        }
        if (!method.isEmpty()) object["method"] = method;
        if (!headers.isEmpty()) {
            object["headers"] = qStringListToJsonObject(headers);
        }
        if (!host.isEmpty()) {
            if (type == "http" || type == "httpupgrade") object["host"] = toAceHost(host);
            if (type == "ws") {
                auto headersObj = object["headers"].isObject() ? object["headers"].toObject() : QJsonObject();
                headersObj["Host"] = toAceHost(host);
                object["headers"] = headersObj;
            }
        }
        if (!idle_timeout.isEmpty()) object["idle_timeout"] = idle_timeout;
        if (!ping_timeout.isEmpty()) object["ping_timeout"] = ping_timeout;
        if (!service_name.isEmpty()) object["service_name"] = service_name;
        return object;
    }
    QJsonObject Transport::ExportIdentity()
    {
        QJsonObject object;
        if (type.isEmpty() || type == "tcp") return object;
        object["type"] = type;
        return object;
    }
    BuildResult Transport::Build()
    {
        auto object = ExportToJson();
        // value(), never operator[]: the mutable operator[] inserts a null entry for a missing key
        if (const auto exported = object.value("host").toString(); type == "http" && exported.contains(',')) {
            auto hosts = exported.split(',', Qt::SkipEmptyParts);
            for (auto& item : hosts) item = item.trimmed();
            object["host"] = QListStr2QJsonArray(hosts);
        }
        // The core decodes each type strictly, so a key kept from another type in the editor would refuse to start.
        static const QHash<QString, QStringList> transportKeys = {
            {"http", {"host", "path", "method", "headers", "idle_timeout", "ping_timeout"}},
            {"ws", {"path", "headers", "max_early_data", "early_data_header_name"}},
            {"quic", {}},
            {"grpc", {"service_name", "idle_timeout", "ping_timeout", "permit_without_stream"}},
            {"httpupgrade", {"host", "path", "headers"}},
        };
        if (const auto keys = transportKeys.constFind(type); keys != transportKeys.constEnd()) {
            for (const auto& key : object.keys()) {
                if (key != "type" && !keys->contains(key)) object.remove(key);
            }
        }
        return {object, ""};
    }

    namespace Checks {
        QString TransportType(const QString& type)
        {
            const auto t = type.trimmed();
            if (t.isEmpty() || singBoxTransports.contains(t)) return {};
            return QCoreApplication::translate("Validation", "sing-box does not support the transport \"%1\". Use ws, httpupgrade, http, grpc or quic.").arg(t);
        }

        QString TransportHost(const QString& host, const QString& type)
        {
            return HostHeader(host, type == "http");
        }

        QString QuicTransport(bool tls, bool /*utls*/, bool reality)
        {
            if (!tls) return QCoreApplication::translate("Validation", "The QUIC transport needs TLS. Set Security to tls.");
            if (reality) return QCoreApplication::translate("Validation", "Reality does not work with the QUIC transport. Clear the Reality key, or choose another transport.");
            return {};
        }

        QString QuicTransportUtls(bool utls)
        {
            if (!utls) return {};
            return QCoreApplication::translate("Validation", "uTLS is not used with the QUIC transport.");
        }
    }

    QJsonObject TlsForTransport(QJsonObject tls, const QString& transportType)
    {
        if (transportType == "quic" && !tls.contains("reality")) tls.remove("utls");
        return tls;
    }

    void Transport::Validate(Validation::Issues& issues, bool tls, bool utls, bool reality)
    {
        using Validation::Severity;
        const auto add = [&issues](Severity severity, const QString& field, const QString& message, const QString& value = {}) {
            if (!message.isEmpty()) issues.append({severity, field, message, value});
        };
        add(Severity::Error, "transport.type", Checks::TransportType(type), type);
        if (type == "http" || type == "ws" || type == "httpupgrade") add(Severity::Warning, "transport.host", Checks::TransportHost(host, type), host);
        if (type == "http") add(Severity::Warning, "transport.method", Validation::HttpToken(method), method);
        if (type == "http" || type == "grpc") {
            add(Severity::Error, "transport.idle_timeout", Validation::Duration(idle_timeout), idle_timeout);
            add(Severity::Error, "transport.ping_timeout", Validation::Duration(ping_timeout), ping_timeout);
        }
        if (type == "quic") {
            add(Severity::Error, "transport.type", Checks::QuicTransport(tls, utls, reality));
            if (tls && !reality) add(Severity::Warning, "tls.utls.fingerprint", Checks::QuicTransportUtls(utls));
        }
    }
}


