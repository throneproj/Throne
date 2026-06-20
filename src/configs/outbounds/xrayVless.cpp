#include "include/configs/outbounds/xrayVless.h"

#include <QUrlQuery>

#include "include/configs/common/utils.h"

namespace Configs {
    bool xrayVless::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        uuid = url.userName();
        encryption = GetQueryValue(query, "encryption", "none");
        flow = GetQueryValue(query, "flow", "");
        streamSetting->ParseFromLink(link);
        multiplex->ParseFromLink(link);
        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool xrayVless::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty() || object["protocol"].toString() != "vless") return false;
        if (object.contains("tag")) name = object["tag"].toString();
        if (object.contains("import_source")) import_source = object["import_source"].toString();
        if (auto settingsObj = object["settings"].toObject(); !settingsObj.isEmpty()) {
            if (settingsObj.contains("address")) server = settingsObj["address"].toString();
            if (settingsObj.contains("port")) server_port = settingsObj["port"].toInt();
            if (settingsObj.contains("flow")) flow = settingsObj["flow"].toString();
            if (settingsObj.contains("id")) uuid = settingsObj["id"].toString();
            if (settingsObj.contains("encryption")) encryption = settingsObj["encryption"].toString();
        }
        if (auto streamSettings = object["streamSettings"].toObject(); !streamSettings.isEmpty()) {
            streamSetting->ParseFromJson(streamSettings);
        }
        if (auto muxObj = object["mux"].toObject(); !muxObj.isEmpty()) {
            multiplex->ParseFromJson(muxObj);
        }
        return true;
    }

    bool xrayVless::ParseFromClash(const clash::Proxies& object) {
        if (object.type != "vless") return false;
        outbound::ParseFromClash(object);
        uuid = QString::fromStdString(object.uuid);
        if (!object.flow.empty()) flow = QString::fromStdString(object.flow);
        if (!object.encryption.empty()) encryption = QString::fromStdString(object.encryption);
        streamSetting->ParseFromClash(object);
        multiplex->ParseFromClash(object);
        return true;
    }

    QString xrayVless::ExportToLink() {
        QUrl url;
        QUrlQuery query;
        url.setScheme("vless");
        url.setUserName(uuid);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        query.addQueryItem("encryption", encryption);
        if (!flow.isEmpty()) query.addQueryItem("flow", flow);

        mergeUrlQuery(query, streamSetting->ExportToLink());
        mergeUrlQuery(query, multiplex->ExportToLink());

        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayVless::ExportToJson() {
        QJsonObject object;
        if (!name.isEmpty()) object["tag"] = name;
        if (!import_source.isEmpty()) object["import_source"] = import_source;
        object["protocol"] = "vless";
        QJsonObject settings;
        settings["address"] = server;
        settings["port"] = server_port;
        settings["id"] = uuid;
        settings["encryption"] = encryption;
        if (!flow.isEmpty() && flow != "none") settings["flow"] = flow;
        object["settings"] = settings;
        if (auto streamObj = streamSetting->ExportToJson(); !streamObj.isEmpty()) object["streamSettings"] = streamObj;
        if (auto muxObj = multiplex->ExportToJson(); !muxObj.isEmpty()) object["mux"] = muxObj;
        return object;
    }

    BuildResult xrayVless::Build() {
        QJsonObject object;
        object["type"] = "socks";
        object["server"] = "127.0.0.1";
        return {object, ""};
    }

    BuildResult xrayVless::BuildXray() {
        QJsonObject object;
        object["protocol"] = "vless";
        QJsonObject settings;
        settings["address"] = server;
        settings["port"] = server_port;
        settings["id"] = uuid;
        settings["encryption"] = encryption;
        if (!flow.isEmpty() && flow != "none") settings["flow"] = flow;
        object["settings"] = settings;
        if (auto streamObj = streamSetting->Build().object; !streamObj.isEmpty()) object["streamSettings"] = streamObj;
        if (auto muxObj = multiplex->Build().object; !muxObj.isEmpty()) object["mux"] = muxObj;
        return {object, ""};
    }
}
