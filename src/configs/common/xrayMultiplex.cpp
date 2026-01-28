#include "include/configs/common/xrayMultiplex.h"

#include <QUrlQuery>




namespace Configs {
    bool xrayMultiplex::ParseFromLink(const QString &link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

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

    QString xrayMultiplex::ExportToLink() {
        QUrlQuery query;
        if (!enabled) return "";
        query.addQueryItem("mux", "true");
        if (concurrency > 0) query.addQueryItem("mux_concurrency", QString::number(concurrency));
        if (xudpConcurrency > 0) query.addQueryItem("mux_xudp_concurrency", QString::number(xudpConcurrency));
        return query.toString(QUrl::FullyEncoded);
    }

    QJsonObject xrayMultiplex::ExportToJson() {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (concurrency > 0) object["concurrency"] = concurrency;
        if (xudpConcurrency > 0) object["xudpConcurrency"] = xudpConcurrency;
        return object;
    }

    BuildResult xrayMultiplex::Build() {
        auto obj = ExportToJson();
        if (useDefault && Configs::dataManager->settingsRepo->xray_mux_default_on) obj["enabled"] = true;
        if (!obj["enabled"].toBool()) return {{}, ""};
        if (Configs::dataManager->settingsRepo->xray_mux_concurrency > 0 && concurrency <= 0) obj["concurrency"] = concurrency;
        if (xudpConcurrency > 0) obj["xudpConcurrency"] = xudpConcurrency;
        return {obj, ""};
    }
}
