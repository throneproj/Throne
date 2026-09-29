#include "include/configs/common/xrayMultiplex.h"

#include <QUrlQuery>

namespace Configs {
    bool xrayMultiplex::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        if (query.hasQueryItem("mux")) enabled = query.queryItemValue("mux").replace("1", "true") == "true", useDefault = false;
        if (query.hasQueryItem("mux_concurrency")) concurrency = query.queryItemValue("mux_concurrency").toInt();
        if (query.hasQueryItem("mux_xudp_concurrency")) xudpConcurrency = query.queryItemValue("mux_xudp_concurrency").toInt();
        return true;
    }

    bool xrayMultiplex::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool(), useDefault = false;
        if (object.contains("concurrency")) concurrency = object["concurrency"].toInt();
        if (object.contains("xudpConcurrency")) xudpConcurrency = object["xudpConcurrency"].toInt();
        return true;
    }

    bool xrayMultiplex::ParseFromClash(const clash::Proxies& object) {
        enabled = object.smux.enabled;
        useDefault = false;
        if (object.smux.max_streams > 0) {
            concurrency = object.smux.max_streams;
        }
        return true;
    }

    QString xrayMultiplex::ExportToLink() {
        QUrlQuery query;
        if (useDefault) return "";
        query.addQueryItem("mux", enabled ? "true" : "false");
        if (enabled) {
            if (concurrency > 0) query.addQueryItem("mux_concurrency", QString::number(concurrency));
            if (xudpConcurrency > 0) query.addQueryItem("mux_xudp_concurrency", QString::number(xudpConcurrency));
        }
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayMultiplex::ExportToJson() {
        QJsonObject object;
        // Tri-state: omit only for "Keep Default" (useDefault); On/Off are persisted explicitly.
        if (useDefault) return object;
        object["enabled"] = enabled;
        if (!enabled) return object;
        if (concurrency > 0) object["concurrency"] = concurrency;
        if (xudpConcurrency > 0) object["xudpConcurrency"] = xudpConcurrency;
        return object;
    }

    BuildResult xrayMultiplex::Build() {
        auto obj = ExportToJson();
        if (useDefault && Configs::dataManager->settingsRepo->xray_mux_default_on) obj["enabled"] = true;
        if (!obj["enabled"].toBool()) return {{}, ""};
        if (Configs::dataManager->settingsRepo->xray_mux_concurrency > 0 && concurrency <= 0) obj["concurrency"] = Configs::dataManager->settingsRepo->xray_mux_concurrency;
        if (xudpConcurrency > 0) obj["xudpConcurrency"] = xudpConcurrency;
        return {obj, ""};
    }
}
