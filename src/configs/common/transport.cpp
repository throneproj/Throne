#include "include/configs/common/transport.h"

#include <QJsonArray>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    bool Transport::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("type"))
        {
            type = query.queryItemValue("type");
            if ((type == "tcp" && query.queryItemValue("headerType") == "http") || type == "h2") {
                type = "http";
                method = "GET";
            }
        }
        if (query.hasQueryItem("host")) host = query.queryItemValue("host");
        if (query.hasQueryItem("path")) path = query.queryItemValue("path", QUrl::FullyDecoded);
        if (query.hasQueryItem("method")) method = query.queryItemValue("method");
        if (query.hasQueryItem("headers")) {
            auto raw = query.queryItemValue("headers", QUrl::FullyDecoded);
            headers = raw.split("|");
            if (headers.length()%2 != 0) {
                MW_show_log("Failed to import invalid headers: " + raw);
                headers.clear();
            }
        }
        if (query.hasQueryItem("idle_timeout")) idle_timeout = query.queryItemValue("idle_timeout");
        if (query.hasQueryItem("ping_timeout")) ping_timeout = query.queryItemValue("ping_timeout");
        if (query.hasQueryItem("max_early_data")) max_early_data = query.queryItemValue("max_early_data").toInt();
        if (query.hasQueryItem("early_data_header_name")) early_data_header_name = query.queryItemValue("early_data_header_name");
        if (query.hasQueryItem("serviceName")) service_name = query.queryItemValue("serviceName", QUrl::FullyDecoded);
        return true;
    }
    bool Transport::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("type")) type = object["type"].toString();
        if (object.contains("host")) host = object["host"].toString();
        if (object.contains("path")) path = object["path"].toString();
        if (object.contains("method")) method = object["method"].toString();
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
        if (object.contains("idle_timeout")) idle_timeout = object["idle_timeout"].toString();
        if (object.contains("ping_timeout")) ping_timeout = object["ping_timeout"].toString();
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
            if (!object.servername.empty()) {
                host = QString::fromStdString(object.servername);
            } else if (object.ws_opts.headers.contains("Host")) {
                host = QString::fromStdString(object.ws_opts.headers.at("Host"));
            }
            if (!object.ws_opts.headers.empty()) {
                for (const auto& [key, value] : object.ws_opts.headers) {
                    headers.append(QString::fromStdString(key) + "=" + QString::fromStdString(value));
                }
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
        QUrlQuery query;
        if (type.isEmpty() || type == "tcp") return "";
        if (!type.isEmpty()) query.addQueryItem("type", type);
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
            if (type == "http" || type == "httpupgrade") object["host"] = host;
            if (type == "ws") {
                auto headersObj = object["headers"].isObject() ? object["headers"].toObject() : QJsonObject();
                headersObj["Host"] = host;
                object["headers"] = headersObj;
            }
        }
        if (!idle_timeout.isEmpty()) object["idle_timeout"] = idle_timeout;
        if (!ping_timeout.isEmpty()) object["ping_timeout"] = ping_timeout;
        if (!service_name.isEmpty()) object["service_name"] = service_name;
        return object;
    }
    BuildResult Transport::Build()
    {
        return {ExportToJson(), ""};
    }
}


