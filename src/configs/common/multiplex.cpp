#include "include/configs/common/multiplex.h"
#include "include/configs/common/utils.h"



namespace Configs {
    bool TcpBrutal::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("brutal_enabled")) enabled = query.queryItemValue("brutal_enabled") == "true";
        if (query.hasQueryItem("brutal_up_mbps")) up_mbps = query.queryItemValue("brutal_up_mbps").toInt();
        if (query.hasQueryItem("brutal_down_mbps")) down_mbps = query.queryItemValue("brutal_down_mbps").toInt();
        return true;
    }
    bool TcpBrutal::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        if (object.contains("up_mbps")) up_mbps = object["up_mbps"].toInt();
        if (object.contains("down_mbps")) down_mbps = object["down_mbps"].toInt();
        return true;
    }
    QString TcpBrutal::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        query.addQueryItem("brutal_enabled", "true");
        if (up_mbps > 0) query.addQueryItem("brutal_up_mbps", QString::number(up_mbps));
        if (down_mbps > 0) query.addQueryItem("brutal_down_mbps", QString::number(down_mbps));
        return QUrlQuery(query).toString();
    }
    QJsonObject TcpBrutal::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        // sing-box expects both values it seems
        object["up_mbps"] = up_mbps <= 0 ? 1 : up_mbps;
        object["down_mbps"] = down_mbps <= 0 ? 1 : down_mbps;
        return object;
    }
    BuildResult TcpBrutal::Build()
    {
        return {ExportToJson(), ""};
    }

    bool Multiplex::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("mux")) enabled = query.queryItemValue("mux") == "true";
        else unspecified = true;
        if (query.hasQueryItem("mux_protocol")) protocol = query.queryItemValue("mux_protocol");
        if (query.hasQueryItem("mux_max_connections")) max_connections = query.queryItemValue("mux_max_connections").toInt();
        if (query.hasQueryItem("mux_min_streams")) min_streams = query.queryItemValue("mux_min_streams").toInt();
        if (query.hasQueryItem("mux_max_streams")) max_streams = query.queryItemValue("mux_max_streams").toInt();
        if (query.hasQueryItem("mux_padding")) padding = query.queryItemValue("mux_padding") == "true";
        brutal->ParseFromLink(link);
        return true;
    }
    bool Multiplex::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty())
        {
            unspecified = true;
            return false;
        }
        if (object.contains("enabled")) enabled = object["enabled"].toBool();
        else unspecified = true;
        if (object.contains("protocol")) protocol = object["protocol"].toString();
        if (object.contains("max_connections")) max_connections = object["max_connections"].toInt();
        if (object.contains("min_streams")) min_streams = object["min_streams"].toInt();
        if (object.contains("max_streams")) max_streams = object["max_streams"].toInt();
        if (object.contains("padding")) padding = object["padding"].toBool();
        if (object.contains("brutal")) brutal->ParseFromJson(object["brutal"].toObject());
        return true;
    }
    QString Multiplex::ExportToLink()
    {
        QUrlQuery query;
        if (!enabled) return "";
        query.addQueryItem("mux", "true");
        if (!protocol.isEmpty()) query.addQueryItem("mux_protocol", protocol);
        if (max_connections > 0) query.addQueryItem("mux_max_connections", QString::number(max_connections));
        if (min_streams > 0) query.addQueryItem("mux_min_streams", QString::number(min_streams));
        if (max_streams > 0) query.addQueryItem("mux_max_streams", QString::number(max_streams));
        if (padding) query.addQueryItem("mux_padding", "true");
        mergeUrlQuery(query, brutal->ExportToLink());
        return query.toString();
    }
    QJsonObject Multiplex::ExportToJson()
    {
        QJsonObject object;
        if (!enabled) return object;
        object["enabled"] = enabled;
        if (!protocol.isEmpty()) object["protocol"] = protocol;
        if (max_connections > 0) object["max_connections"] = max_connections;
        if (min_streams > 0) object["min_streams"] = min_streams;
        if (max_streams > 0) object["max_streams"] = max_streams;
        if (padding) object["padding"] = padding;
        if (brutal->enabled) object["brutal"] = brutal->ExportToJson();
        return object;
    }
    BuildResult Multiplex::Build()
    {
        auto obj = ExportToJson();
        if (unspecified && Configs::dataManager->settingsRepo->mux_default_on) obj["enabled"] = true;
        if (!obj["enabled"].toBool()) return {{}, ""};
        if (protocol.isEmpty()) obj["protocol"] = Configs::dataManager->settingsRepo->mux_protocol;
        if (max_streams == 0 && max_connections == 0 && min_streams == 0) obj["max_streams"] = Configs::dataManager->settingsRepo->mux_concurrency;
        if (Configs::dataManager->settingsRepo->mux_padding) obj["padding"] = true;
        return {obj, ""};
    }
}


