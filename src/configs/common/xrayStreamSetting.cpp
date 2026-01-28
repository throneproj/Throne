#include "include/configs/common/xrayStreamSetting.h"

#include <QUrlQuery>
#include <QJsonArray>

#include "include/configs/common/utils.h"



namespace Configs {
    bool xrayTLS::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("sni")) serverName = query.queryItemValue("sni");
        if (query.hasQueryItem("peer")) serverName = query.queryItemValue("peer");
        if (query.hasQueryItem("server_name")) serverName = query.queryItemValue("server_name");
        if (query.hasQueryItem("allowInsecure")) allowInsecure = query.queryItemValue("allowInsecure").replace("1", "true") == "true";
        if (query.hasQueryItem("allow_insecure")) allowInsecure = query.queryItemValue("allow_insecure").replace("1", "true") == "true";
        if (query.hasQueryItem("insecure")) allowInsecure = query.queryItemValue("insecure").replace("1", "true") == "true";
        if (query.hasQueryItem("alpn")) alpn = query.queryItemValue("alpn").split(",");
        if (query.hasQueryItem("fp")) fingerprint = query.queryItemValue("fp");
        return true;
    }

    bool xrayTLS::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("serverName")) serverName = object["serverName"].toString();
        if (object.contains("allowInsecure")) allowInsecure = object["allowInsecure"].toBool();
        if (object.contains("alpn")) alpn = QJsonArray2QListString(object["alpn"].toArray());
        if (object.contains("fingerprint")) fingerprint = object["fingerprint"].toString();
        return true;
    }

    QString xrayTLS::ExportToLink() {
        QUrlQuery query;
        query.addQueryItem("sni", serverName);
        if (allowInsecure) query.addQueryItem("allowInsecure", "1");
        if (!alpn.isEmpty()) query.addQueryItem("alpn", alpn.join(","));
        if (!fingerprint.isEmpty()) query.addQueryItem("fp", fingerprint);
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayTLS::ExportToJson() {
        QJsonObject object;
        object["serverName"] = serverName;
        if (allowInsecure) object["allowInsecure"] = allowInsecure;
        if (!alpn.isEmpty()) {
            object["alpn"] = QListStr2QJsonArray(alpn);
        }
        if (!fingerprint.isEmpty()) object["fingerprint"] = fingerprint;
        return object;
    }

    BuildResult xrayTLS::Build() {
        auto obj = ExportToJson();
        if (fingerprint.isEmpty() && !Configs::dataManager->settingsRepo->utlsFingerprint.isEmpty()) obj["fingerprint"] = Configs::dataManager->settingsRepo->utlsFingerprint;
        return {obj, ""};
    }

    bool xrayReality::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("sni")) serverName = query.queryItemValue("sni");
        if (query.hasQueryItem("peer")) serverName = query.queryItemValue("peer");
        if (query.hasQueryItem("pbk")) password = query.queryItemValue("pbk");
        if (query.hasQueryItem("fp")) fingerprint = query.queryItemValue("fp");
        if (query.hasQueryItem("sid")) shortId = query.queryItemValue("sid");
        if (query.hasQueryItem("spx")) spiderX = query.queryItemValue("spx");
        return true;

    }

    bool xrayReality::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("serverName")) serverName = object["serverName"].toString();
        if (object.contains("fingerprint")) fingerprint = object["fingerprint"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("shortId")) shortId = object["shortId"].toString();
        if (object.contains("shortId")) shortId = object["shortId"].toString();
        if (object.contains("spiderX")) spiderX = object["spiderX"].toString();
        return true;
    }

    QString xrayReality::ExportToLink() {
        QUrlQuery query;
        query.addQueryItem("sni", serverName);
        if (!fingerprint.isEmpty()) query.addQueryItem("fp", fingerprint);
        if (!password.isEmpty()) query.addQueryItem("pbk", password);
        if (!shortId.isEmpty()) query.addQueryItem("sid", shortId);
        if (!spiderX.isEmpty()) query.addQueryItem("spx", spiderX);
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayReality::ExportToJson() {
        QJsonObject object;
        object["serverName"] = serverName;
        if (!fingerprint.isEmpty()) object["fingerprint"] = fingerprint;
        if (!password.isEmpty()) object["password"] = password;
        if (!shortId.isEmpty()) object["shortId"] = shortId;
        if (!spiderX.isEmpty()) object["spiderX"] = spiderX;
        return object;
    }

    BuildResult xrayReality::Build() {
        auto obj = ExportToJson();
        if (fingerprint.isEmpty() && !Configs::dataManager->settingsRepo->utlsFingerprint.isEmpty()) obj["fingerprint"] = Configs::dataManager->settingsRepo->utlsFingerprint;
        return {obj, ""};
    }

    bool xrayXHTTP::ParseExtraJson(QString str) {
        str = str.replace('\'', '"').replace("True", "true").replace("False", "false");
        auto obj = QString2QJsonObject(str);
        if (obj.isEmpty()) return false;

        if (obj.contains("headers") && obj["headers"].isArray()) {
            headers = QJsonArray2QListString(obj["headers"].toArray());
        }
        if (obj.contains("xPaddingBytes")) {
            xPaddingBytes = obj["xPaddingBytes"].toVariant().toString();
        }
        if (obj.contains("noGRPCHeader")) noGRPCHeader = obj["noGRPCHeader"].toBool();
        if (obj.contains("scMaxEachPostBytes")) {
            scMaxEachPostBytes = obj["scMaxEachPostBytes"].toVariant().toString();
        }
        if (obj.contains("scMinPostsIntervalMs")) {
            scMinPostsIntervalMs = obj["scMinPostsIntervalMs"].toVariant().toString();
        }
        if (obj.contains("downloadSettings")) {
            if (obj["downloadSettings"].isObject()) {
                downloadSettings = QJsonObject2QString(obj["downloadSettings"].toObject(), true);
            } else if (obj["downloadSettings"].isString()) {
                downloadSettings = obj["downloadSettings"].toString();
            }
        }
        if (auto xmuxObj = obj["xmux"].toObject(); !xmuxObj.isEmpty()) {
            if (xmuxObj.contains("maxConcurrency")) {
                maxConcurrency = xmuxObj["maxConcurrency"].toVariant().toString();
            }
            if (xmuxObj.contains("maxConnections")) {
                maxConnections = xmuxObj["maxConnections"].toVariant().toString();
            }
            if (xmuxObj.contains("cMaxReuseTimes")) {
                cMaxReuseTimes = xmuxObj["cMaxReuseTimes"].toVariant().toString();
            }
            if (xmuxObj.contains("hMaxRequestTimes")) {
                hMaxRequestTimes = xmuxObj["hMaxRequestTimes"].toVariant().toString();
            }
            if (xmuxObj.contains("hMaxReusableSecs")) {
                hMaxReusableSecs = xmuxObj["hMaxReusableSecs"].toVariant().toString();
            }
            if (xmuxObj.contains("hKeepAlivePeriod")) {
                hKeepAlivePeriod = xmuxObj["hKeepAlivePeriod"].toVariant().toLongLong();
            }
        }
        return true;
    }


    bool xrayXHTTP::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("host")) host = query.queryItemValue("host");
        if (query.hasQueryItem("path")) path = query.queryItemValue("path", QUrl::FullyDecoded);
        if (query.hasQueryItem("mode")) mode = query.queryItemValue("mode");
        if (query.hasQueryItem("extra")) ParseExtraJson(query.queryItemValue("extra", QUrl::FullyDecoded));
        if (query.hasQueryItem("headers")) {
            auto raw = query.queryItemValue("headers", QUrl::FullyDecoded);
            headers = raw.split("|");
            if (headers.length()%2 != 0) {
                MW_show_log("Failed to import invalid headers: " + raw);
                headers.clear();
            }
        }
        if (query.hasQueryItem("x_padding_bytes")) xPaddingBytes = query.queryItemValue("x_padding_bytes");
        if (query.hasQueryItem("no_grpc_header")) noGRPCHeader = query.queryItemValue("no_grpc_header").replace("1", "true") == "true";

        if (query.hasQueryItem("sc_max_each_post_bytes")) scMaxEachPostBytes = query.queryItemValue("sc_max_each_post_bytes");
        if (query.hasQueryItem("sc_min_posts_interval_ms")) scMinPostsIntervalMs = query.queryItemValue("sc_min_posts_interval_ms");

        if (query.hasQueryItem("max_concurrency")) maxConcurrency = query.queryItemValue("max_concurrency");
        if (query.hasQueryItem("max_connections")) maxConnections = query.queryItemValue("max_connections");

        if (query.hasQueryItem("max_reuse_times")) cMaxReuseTimes = query.queryItemValue("max_reuse_times");

        if (query.hasQueryItem("max_request_times")) hMaxRequestTimes = query.queryItemValue("max_request_times");
        if (query.hasQueryItem("max_reusable_secs")) hMaxReusableSecs = query.queryItemValue("max_reusable_secs");
        if (query.hasQueryItem("keep_alive_period")) hKeepAlivePeriod = query.queryItemValue("keep_alive_period").toLongLong();
        return true;
    }

    bool xrayXHTTP::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("host")) host = object["host"].toString();
        if (object.contains("path")) path = object["path"].toString();
        if (object.contains("mode")) mode = object["mode"].toString();
        if (auto exObj = object["extra"].toObject(); !exObj.isEmpty()) {
            ParseExtraJson(QJsonObject2QString(exObj, true));
        }
        return true;
    }

    QString xrayXHTTP::ExportToLink() {
        QUrlQuery query;
        if (!host.isEmpty()) query.addQueryItem("host", host);
        if (!path.isEmpty()) query.addQueryItem("path", path);
        if (!mode.isEmpty()) query.addQueryItem("mode", mode);
        auto jsonExport = ExportToJson();
        if (jsonExport.contains("extra") && jsonExport["extra"].isObject()) {
            auto exObj = jsonExport["extra"].toObject();
            if (!exObj.isEmpty()) {
                query.addQueryItem("extra", QJsonObject2QString(exObj, true));
            }
        }
        if (!headers.isEmpty()) query.addQueryItem("headers", headers.join("|"));
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayXHTTP::ExportToJson() {
        QJsonObject obj;
        if (!host.isEmpty()) obj["host"] = host;
        if (!path.isEmpty()) obj["path"] = path;
        if (!mode.isEmpty()) obj["mode"] = mode;

        QJsonObject extraObj;
        if (!headers.isEmpty()) extraObj["headers"] = qStringListToJsonObject(headers);
        if (!xPaddingBytes.isEmpty()) extraObj["xPaddingBytes"] = xPaddingBytes;
        if (noGRPCHeader) extraObj["noGRPCHeader"] = true;
        if (!scMaxEachPostBytes.isEmpty()) extraObj["scMaxEachPostBytes"] = scMaxEachPostBytes;
        if (!scMinPostsIntervalMs.isEmpty()) extraObj["scMinPostsIntervalMs"] = scMinPostsIntervalMs;
        if (!downloadSettings.isEmpty()) {
            if (auto dsObj = QString2QJsonObject(downloadSettings); !dsObj.isEmpty()) {
                extraObj["downloadSettings"] = dsObj;
            }
        }
        QJsonObject xmuxObj;
        if (!maxConcurrency.isEmpty()) xmuxObj["maxConcurrency"] = maxConcurrency;
        if (!maxConnections.isEmpty()) xmuxObj["maxConnections"] = maxConnections;
        if (!cMaxReuseTimes.isEmpty()) xmuxObj["cMaxReuseTimes"] = cMaxReuseTimes;
        if (!hMaxRequestTimes.isEmpty()) xmuxObj["hMaxRequestTimes"] = hMaxRequestTimes;
        if (!hMaxReusableSecs.isEmpty()) xmuxObj["hMaxReusableSecs"] = hMaxReusableSecs;
        if (hKeepAlivePeriod > 0) xmuxObj["hKeepAlivePeriod"] = hKeepAlivePeriod;
        if (!xmuxObj.isEmpty()) extraObj["xmux"] = xmuxObj;
        if (!extraObj.isEmpty()) obj["extra"] = extraObj;
        return obj;
    }

    BuildResult xrayXHTTP::Build() {
        return {ExportToJson(), ""};
    }

    bool xrayStreamSetting::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("type")) network = query.queryItemValue("type").replace("tcp", "raw");
        if (network != "raw" && network != "xhttp") return false;
        if (query.hasQueryItem("security")) security = query.queryItemValue("security");
        if (security == "tls") TLS->ParseFromLink(link);
        else if (security == "reality") reality->ParseFromLink(link);
        xhttp->ParseFromLink(link);

        return true;
    }

    bool xrayStreamSetting::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;

        if (object.contains("network")) network = object.value("network").toString();
        if (network != "raw" && network != "xhttp") return false;
        if (object.contains("security")) security = object.value("security").toString();
        if (security == "tls" && object["tlsSettings"].isObject()) TLS->ParseFromJson(object["tlsSettings"].toObject());
        else if (security == "reality" && object["realitySettings"].isObject()) reality->ParseFromJson(object["realitySettings"].toObject());
        if (network == "xhttp" && object["xhttpSettings"].isObject()) xhttp->ParseFromJson(object["xhttpSettings"].toObject());
        return true;
    }

    QString xrayStreamSetting::ExportToLink() {
        QUrlQuery query;
        if (!network.isEmpty()) query.addQueryItem("type", network);
        if (!security.isEmpty()) query.addQueryItem("security", security);
        if (security == "tls") mergeUrlQuery(query, TLS->ExportToLink());
        if (security == "reality") mergeUrlQuery(query, reality->ExportToLink());
        if (network == "xhttp") mergeUrlQuery(query, xhttp->ExportToLink());
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayStreamSetting::ExportToJson() {
        QJsonObject object;
        object["network"] = network;
        object["security"] = security;
        if (security == "tls") object["tlsSettings"] = TLS->ExportToJson();
        else if (security == "reality") object["realitySettings"] = reality->ExportToJson();
        if (network == "xhttp") object["xhttpSettings"] = xhttp->ExportToJson();
        return object;
    }

    BuildResult xrayStreamSetting::Build() {
        return {ExportToJson(), ""};
    }
}
