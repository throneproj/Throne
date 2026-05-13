#include "include/configs/common/xrayStreamSetting.h"
#include <QUrlQuery>
#include <QJsonArray>
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        QString qs(const std::string& value) {
            return QString::fromStdString(value);
        }

        bool hasText(const std::string& value) {
            return !value.empty();
        }

        void addXmuxField(QJsonObject& xmux, const QString& key, const std::string& value) {
            if (hasText(value)) xmux[key] = qs(value);
        }

        void addXPaddingFields(QJsonObject& extra, bool obfsMode, const std::string& key,
                               const std::string& header, const std::string& placement,
                               const std::string& method) {
            if (obfsMode) extra["xPaddingObfsMode"] = true;
            if (hasText(key)) extra["xPaddingKey"] = qs(key);
            if (hasText(header)) extra["xPaddingHeader"] = qs(header);
            if (hasText(placement)) extra["xPaddingPlacement"] = qs(placement);
            if (hasText(method)) extra["xPaddingMethod"] = qs(method);
        }

        QJsonObject buildXmuxObject(const clash::xhttpReuseSettings& reuse) {
            QJsonObject xmux;
            addXmuxField(xmux, "maxConcurrency", reuse.max_concurrency);
            addXmuxField(xmux, "maxConnections", reuse.max_connections);
            addXmuxField(xmux, "cMaxReuseTimes", reuse.c_max_reuse_times);
            addXmuxField(xmux, "hMaxRequestTimes", reuse.h_max_request_times);
            addXmuxField(xmux, "hMaxReusableSecs", reuse.h_max_reusable_secs);
            if (hasText(reuse.h_keep_alive_period)) {
                xmux["hKeepAlivePeriod"] = qs(reuse.h_keep_alive_period).toLongLong();
            }
            return xmux;
        }

        QJsonObject buildDownloadSettingsObject(const clash::xhttpDownloadSettings& settings) {
            QJsonObject object;
            if (hasText(settings.server)) object["address"] = qs(settings.server);
            object["port"] = int(settings.port);
            object["network"] = "xhttp";

            if (!settings.reality_opts.public_key.empty()) {
                object["security"] = "reality";
                QJsonObject reality;
                reality["show"] = false;
                if (hasText(settings.servername)) reality["serverName"] = qs(settings.servername);
                if (hasText(settings.client_fingerprint)) reality["fingerprint"] = qs(settings.client_fingerprint);
                reality["publicKey"] = qs(settings.reality_opts.public_key);
                if (hasText(settings.reality_opts.short_id)) reality["shortId"] = qs(settings.reality_opts.short_id);
                object["realitySettings"] = reality;
            } else if (settings.tls) {
                object["security"] = "tls";
                QJsonObject tls;
                if (hasText(settings.servername)) tls["serverName"] = qs(settings.servername);
                tls["allowInsecure"] = false;
                if (!settings.alpn.empty()) {
                    QJsonArray alpn;
                    for (const auto& item : settings.alpn) alpn.append(qs(item));
                    tls["alpn"] = alpn;
                }
                if (hasText(settings.client_fingerprint)) tls["fingerprint"] = qs(settings.client_fingerprint);
                object["tlsSettings"] = tls;
            }

            QJsonObject xhttp;
            if (hasText(settings.host)) xhttp["host"] = qs(settings.host);
            if (hasText(settings.path)) xhttp["path"] = qs(settings.path);
            xhttp["mode"] = hasText(settings.mode) ? qs(settings.mode) : "auto";

            QJsonObject extra;
            addXPaddingFields(extra, settings.x_padding_obfs_mode, settings.x_padding_key,
                              settings.x_padding_header, settings.x_padding_placement,
                              settings.x_padding_method);
            if (auto xmux = buildXmuxObject(settings.reuse_settings); !xmux.isEmpty()) extra["xmux"] = xmux;
            if (!extra.isEmpty()) xhttp["extra"] = extra;

            object["xhttpSettings"] = xhttp;
            return object;
        }
    }

    bool xrayTLS::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("sni")) serverName = query.queryItemValue("sni");
        if (query.hasQueryItem("peer")) serverName = query.queryItemValue("peer");
        if (query.hasQueryItem("server_name")) serverName = query.queryItemValue("server_name");
        if (query.hasQueryItem("allowInsecure")) allowInsecure = query.queryItemValue("allowInsecure").replace("1", "true") == "true";
        if (query.hasQueryItem("allow_insecure")) allowInsecure = query.queryItemValue("allow_insecure").replace("1", "true") == "true";
        if (query.hasQueryItem("insecure")) allowInsecure = query.queryItemValue("insecure").replace("1", "true") == "true";
        if (query.hasQueryItem("alpn")) alpn = query.queryItemValue("alpn", QUrl::FullyDecoded).split(",");
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

    bool xrayTLS::ParseFromClash(const clash::Proxies& object) {
        if (!object.servername.empty()) {
            serverName = QString::fromStdString(object.servername);
        } else if (!object.sni.empty()) {
            serverName = QString::fromStdString(object.sni);
        } else {
            serverName = QString::fromStdString(object.server);
        }
        allowInsecure = object.skip_cert_verify;
        for (const auto& s : object.alpn) {
            alpn.append(QString::fromStdString(s));
        }
        if (!object.client_fingerprint.empty()) fingerprint = QString::fromStdString(object.client_fingerprint);
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
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("sni")) serverName = query.queryItemValue("sni");
        if (query.hasQueryItem("peer")) serverName = query.queryItemValue("peer");
        if (query.hasQueryItem("pbk")) password = query.queryItemValue("pbk");
        if (query.hasQueryItem("fp")) fingerprint = query.queryItemValue("fp");
        if (query.hasQueryItem("sid")) shortId = query.queryItemValue("sid");
        if (query.hasQueryItem("spx")) spiderX = query.queryItemValue("spx", QUrl::FullyDecoded);
        return true;

    }

    bool xrayReality::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("serverName")) serverName = object["serverName"].toString();
        if (object.contains("fingerprint")) fingerprint = object["fingerprint"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("shortId")) shortId = object["shortId"].toString();
        if (object.contains("spiderX")) spiderX = object["spiderX"].toString();
        return true;
    }

    bool xrayReality::ParseFromClash(const clash::Proxies& object) {
        if (!object.servername.empty()) {
            serverName = QString::fromStdString(object.servername);
        } else if (!object.sni.empty()) {
            serverName = QString::fromStdString(object.sni);
        } else {
            serverName = QString::fromStdString(object.server);
        }
        if (!object.client_fingerprint.empty()) fingerprint = QString::fromStdString(object.client_fingerprint);
        if (!object.reality_opts.public_key.empty()) password = QString::fromStdString(object.reality_opts.public_key);
        if (!object.reality_opts.short_id.empty()) shortId = QString::fromStdString(object.reality_opts.short_id);
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
        if (obj.contains("xPaddingObfsMode")) xPaddingObfsMode = obj["xPaddingObfsMode"].toBool();
        if (obj.contains("xPaddingKey")) {
            xPaddingKey = obj["xPaddingKey"].toString();
        }
        if (obj.contains("xPaddingHeader")) {
            xPaddingHeader = obj["xPaddingHeader"].toString();
        }
        if (obj.contains("xPaddingPlacement")) {
            xPaddingPlacement = obj["xPaddingPlacement"].toString();
        }
        if (obj.contains("xPaddingMethod")) {
            xPaddingMethod = obj["xPaddingMethod"].toString();
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
                hKeepAlivePeriodSet = true;
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
        if (query.hasQueryItem("keep_alive_period")) {
            hKeepAlivePeriod = query.queryItemValue("keep_alive_period").toLongLong();
            hKeepAlivePeriodSet = true;
        }
        return true;
    }

    bool xrayXHTTP::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("host")) host = object["host"].toString();
        if (object.contains("path")) path = object["path"].toString();
        if (object.contains("mode")) mode = object["mode"].toString();
        ParseExtraJson(QJsonObject2QString(object, true));
        if (auto exObj = object["extra"].toObject(); !exObj.isEmpty()) {
            ParseExtraJson(QJsonObject2QString(exObj, true));
        }
        return true;
    }

    bool xrayXHTTP::ParseFromClash(const clash::Proxies& object) {
        const auto& opts = object.xhttp_opts;
        host = qs(opts.host);
        path = qs(opts.path);
        mode = hasText(opts.mode) ? qs(opts.mode) : "auto";

        xPaddingObfsMode = opts.x_padding_obfs_mode;
        xPaddingKey = qs(opts.x_padding_key);
        xPaddingHeader = qs(opts.x_padding_header);
        xPaddingPlacement = qs(opts.x_padding_placement);
        xPaddingMethod = qs(opts.x_padding_method);
        scMinPostsIntervalMs = qs(opts.sc_min_posts_interval_ms);

        maxConcurrency = qs(opts.reuse_settings.max_concurrency);
        maxConnections = qs(opts.reuse_settings.max_connections);
        cMaxReuseTimes = qs(opts.reuse_settings.c_max_reuse_times);
        hMaxRequestTimes = qs(opts.reuse_settings.h_max_request_times);
        hMaxReusableSecs = qs(opts.reuse_settings.h_max_reusable_secs);
        if (hasText(opts.reuse_settings.h_keep_alive_period)) {
            hKeepAlivePeriod = qs(opts.reuse_settings.h_keep_alive_period).toLongLong();
            hKeepAlivePeriodSet = true;
        }

        if (opts.has_download_settings) {
            downloadSettings = QJsonObject2QString(buildDownloadSettingsObject(opts.download_settings), true);
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
        if (xPaddingObfsMode) extraObj["xPaddingObfsMode"] = true;
        if (!xPaddingKey.isEmpty()) extraObj["xPaddingKey"] = xPaddingKey;
        if (!xPaddingHeader.isEmpty()) extraObj["xPaddingHeader"] = xPaddingHeader;
        if (!xPaddingPlacement.isEmpty()) extraObj["xPaddingPlacement"] = xPaddingPlacement;
        if (!xPaddingMethod.isEmpty()) extraObj["xPaddingMethod"] = xPaddingMethod;
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
        if (hKeepAlivePeriodSet) xmuxObj["hKeepAlivePeriod"] = hKeepAlivePeriod;
        if (!xmuxObj.isEmpty()) extraObj["xmux"] = xmuxObj;
        if (!extraObj.isEmpty()) obj["extra"] = extraObj;
        return obj;
    }

    BuildResult xrayXHTTP::Build() {
        return {ExportToJson(), ""};
    }

    bool xrayWS::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());
        if (query.hasQueryItem("host")) host = query.queryItemValue("host");
        if (query.hasQueryItem("path")) path = query.queryItemValue("path", QUrl::FullyDecoded);
        if (query.hasQueryItem("ed")) ed = query.queryItemValue("ed").toInt();
        if (query.hasQueryItem("headers")) {
            auto raw = query.queryItemValue("headers", QUrl::FullyDecoded);
            headers = raw.split("|");
            if (headers.length()%2 != 0) {
                MW_show_log("Failed to import invalid headers: " + raw);
                headers.clear();
            }
        }
        if (query.hasQueryItem("heartbeat_period")) heartbeatPeriod = query.queryItemValue("heartbeat_period").toInt();
        return true;
    }

    bool xrayWS::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("path")) {
            path = object["path"].toString();
            if (path.contains("?ed=")) {
                auto spl = path.split("?ed=");
                path = spl[0];
                ed = spl[1].toInt();
            }
        }
        if (object.contains("host")) host = object["host"].toString();
        if (object.contains("headers") && object["headers"].isObject())
            headers = jsonObjectToQStringList(object["headers"].toObject());
        if (object.contains("heartbeatPeriod")) heartbeatPeriod = object["heartbeatPeriod"].toInt(0);
        return true;
    }

    bool xrayWS::ParseFromClash(const clash::Proxies& object) {
        if (!object.ws_opts.path.empty()) {
            path = QString::fromStdString(object.ws_opts.path);
            if (path.contains("?ed=")) {
                auto spl = path.split("?ed=");
                path = spl[0];
                ed = spl[1].toInt();
            }
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
        return true;
    }

    QString xrayWS::ExportToLink() {
        QUrlQuery query;
        if (!host.isEmpty()) query.addQueryItem("host", host);
        if (!path.isEmpty() && path != "/") query.addQueryItem("path", path);
        if (ed > 0) query.addQueryItem("ed", QString::number(ed));
        if (!headers.isEmpty()) query.addQueryItem("headers", headers.join("|"));
        if (heartbeatPeriod > 0) query.addQueryItem("heartbeat_period", QString::number(heartbeatPeriod));
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayWS::ExportToJson() {
        QJsonObject obj;
        if (!path.isEmpty()) {
            auto fullPath = path;
            if (ed > 0) fullPath += "?ed=" + QString::number(ed);
            obj["path"] = fullPath;
        }
        if (!host.isEmpty()) obj["host"] = host;
        if (!headers.isEmpty()) obj["headers"] = qStringListToJsonObject(headers);
        if (heartbeatPeriod > 0) obj["heartbeatPeriod"] = heartbeatPeriod;
        return obj;
    }

    BuildResult xrayWS::Build() {
        return {ExportToJson(), ""};
    }

    bool xrayHttpUpgrade::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());
        if (query.hasQueryItem("host")) host = query.queryItemValue("host");
        if (query.hasQueryItem("path")) path = query.queryItemValue("path", QUrl::FullyDecoded);
        if (query.hasQueryItem("ed")) ed = query.queryItemValue("ed").toInt();
        if (query.hasQueryItem("headers")) {
            auto raw = query.queryItemValue("headers", QUrl::FullyDecoded);
            headers = raw.split("|");
            if (headers.size() % 2 != 0) headers.clear();
        }
        return true;
    }

    bool xrayHttpUpgrade::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("path")) {
            path = object["path"].toString();
            if (path.contains("?ed=")) {
                auto spl = path.split("?ed=");
                path = spl[0];
                ed = spl[1].toInt();
            }
        }
        if (object.contains("host")) host = object["host"].toString();
        if (object.contains("headers") && object["headers"].isObject())
            headers = jsonObjectToQStringList(object["headers"].toObject());
        return true;
    }

    bool xrayHttpUpgrade::ParseFromClash(const clash::Proxies& object) {
        if (!object.ws_opts.path.empty()) {
            path = QString::fromStdString(object.ws_opts.path);
            if (path.contains("?ed=")) {
                auto spl = path.split("?ed=");
                path = spl[0];
                ed = spl[1].toInt();
            }
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
        return true;
    }

    QString xrayHttpUpgrade::ExportToLink() {
        QUrlQuery query;
        if (!host.isEmpty()) query.addQueryItem("host", host);
        if (!path.isEmpty() && path != "/") query.addQueryItem("path", path);
        if (ed > 0) query.addQueryItem("ed", QString::number(ed));
        if (!headers.isEmpty()) query.addQueryItem("headers", headers.join("|"));
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayHttpUpgrade::ExportToJson() {
        QJsonObject obj;
        if (!path.isEmpty()) {
            auto fullPath = path;
            if (ed > 0) fullPath += "?ed=" + QString::number(ed);
            obj["path"] = fullPath;
        }
        if (!host.isEmpty()) obj["host"] = host;
        if (!headers.isEmpty()) obj["headers"] = qStringListToJsonObject(headers);
        return obj;
    }

    BuildResult xrayHttpUpgrade::Build() {
        return {ExportToJson(), ""};
    }

    bool xrayGRPC::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());
        if (query.hasQueryItem("authority")) authority = query.queryItemValue("authority", QUrl::FullyDecoded);
        if (query.hasQueryItem("serviceName")) serviceName = query.queryItemValue("serviceName", QUrl::FullyDecoded);
        if (query.hasQueryItem("mode") && query.queryItemValue("mode") == "multi") multiMode = true;
        return true;
    }

    bool xrayGRPC::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("authority")) authority = object["authority"].toString();
        if (object.contains("serviceName")) serviceName = object["serviceName"].toString();
        if (object.contains("multiMode")) multiMode = object["multiMode"].toBool();
        return true;
    }

    bool xrayGRPC::ParseFromClash(const clash::Proxies& object) {
        if (!object.grpc_opts.grpc_service_name.empty()) serviceName = QString::fromStdString(object.grpc_opts.grpc_service_name);
        return true;
    }

    QString xrayGRPC::ExportToLink() {
        QUrlQuery query;
        if (!authority.isEmpty()) query.addQueryItem("authority", authority);
        if (!serviceName.isEmpty()) query.addQueryItem("serviceName", serviceName);
        if (multiMode) query.addQueryItem("mode", "multi");
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayGRPC::ExportToJson() {
        QJsonObject obj;
        if (!authority.isEmpty()) obj["authority"] = authority;
        if (!serviceName.isEmpty()) obj["serviceName"] = serviceName;
        if (multiMode) obj["multiMode"] = multiMode;
        return obj;
    }

    BuildResult xrayGRPC::Build() {
        return {ExportToJson(), ""};
    }

    bool xrayStreamSetting::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("type")) network = query.queryItemValue("type").replace("tcp", "raw");
        if (!Configs::XrayNetworks.contains(network)) return false;
        if (query.hasQueryItem("security")) security = query.queryItemValue("security");
        if (security == "tls") TLS->ParseFromLink(link);
        else if (security == "reality") reality->ParseFromLink(link);
        if (network == "xhttp") xhttp->ParseFromLink(link);
        else if (network == "ws") ws->ParseFromLink(link);
        else if (network == "httpupgrade") httpupgrade->ParseFromLink(link);
        else if (network == "grpc") grpc->ParseFromLink(link);

        return true;
    }

    bool xrayStreamSetting::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;

        if (object.contains("network")) network = object.value("network").toString();
        if (!Configs::XrayNetworks.contains(network)) return false;
        if (object.contains("security")) security = object.value("security").toString();
        if (security == "tls" && object["tlsSettings"].isObject()) TLS->ParseFromJson(object["tlsSettings"].toObject());
        else if (security == "reality" && object["realitySettings"].isObject()) reality->ParseFromJson(object["realitySettings"].toObject());
        if (network == "xhttp" && object["xhttpSettings"].isObject()) xhttp->ParseFromJson(object["xhttpSettings"].toObject());
        if (network == "ws" && object["wsSettings"].isObject()) ws->ParseFromJson(object["wsSettings"].toObject());
        if (network == "httpupgrade" && object["httpupgradeSettings"].isObject()) httpupgrade->ParseFromJson(object["httpupgradeSettings"].toObject());
        if (network == "grpc" && object["grpcSettings"].isObject()) grpc->ParseFromJson(object["grpcSettings"].toObject());
        return true;
    }

    bool xrayStreamSetting::ParseFromClash(const clash::Proxies& object) {
        if (!object.network.empty()) network = QString::fromStdString(object.network);
        if (network != "raw" && network != "ws" && network != "grpc" && network != "xhttp") return false;
        if (object.tls) {
            if (object.reality_opts.public_key.empty()) {
                security = "tls";
                TLS->ParseFromClash(object);
            } else {
                security = "reality";
                reality->ParseFromClash(object);
            }
        }
        if (network == "xhttp") xhttp->ParseFromClash(object);
        if (network == "ws") {
            if (object.ws_opts.v2ray_http_upgrade) {
                network = "httpupgrade";
                httpupgrade->ParseFromClash(object);
            } else {
                ws->ParseFromClash(object);
            }
        }
        if (network == "grpc") grpc->ParseFromClash(object);
        return true;
    }

    QString xrayStreamSetting::ExportToLink() {
        QUrlQuery query;
        if (!network.isEmpty()) query.addQueryItem("type", network);
        if (!security.isEmpty()) query.addQueryItem("security", security);
        if (security == "tls") mergeUrlQuery(query, TLS->ExportToLink());
        if (security == "reality") mergeUrlQuery(query, reality->ExportToLink());
        if (network == "xhttp") mergeUrlQuery(query, xhttp->ExportToLink());
        if (network == "ws") mergeUrlQuery(query, ws->ExportToLink());
        if (network == "httpupgrade") mergeUrlQuery(query, httpupgrade->ExportToLink());
        if (network == "grpc") mergeUrlQuery(query, grpc->ExportToLink());
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayStreamSetting::ExportToJson() {
        QJsonObject object;
        object["network"] = network;
        object["security"] = security;
        if (security == "tls") object["tlsSettings"] = TLS->ExportToJson();
        else if (security == "reality") object["realitySettings"] = reality->ExportToJson();
        if (network == "xhttp") object["xhttpSettings"] = xhttp->ExportToJson();
        if (network == "ws") object["wsSettings"] = ws->ExportToJson();
        if (network == "httpupgrade") object["httpupgradeSettings"] = httpupgrade->ExportToJson();
        if (network == "grpc") object["grpcSettings"] = grpc->ExportToJson();
        return object;
    }


    QString getXrayOutboundDomainStrategy() {
        auto strategy = Configs::dataManager->settingsRepo->direct_dns_strategy;
        if (strategy == "prefer_ipv4") return "UseIPv4v6";
        if (strategy == "prefer_ipv6") return "UseIPv6v4";
        if (strategy == "ipv4_only") return "ForceIPv4";
        if (strategy == "ipv6_only") return "ForceIPv6";
        return "UseIP";
    }

    BuildResult xrayStreamSetting::Build() {
        auto obj = ExportToJson();
        obj["sockopt"] = QJsonObject{
            {"domainStrategy", getXrayOutboundDomainStrategy()}
        };
        return {obj, ""};
    }
}
