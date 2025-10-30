#include "include/configs/common/DialFields.h"

#include <QUrlQuery>

namespace Configs {
    bool DialFields::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.hasQueryItem("reuse_addr")) reuse_addr = query.queryItemValue("reuse_addr") == "true";
        if (query.hasQueryItem("connect_timeout")) connect_timeout = query.queryItemValue("connect_timeout");
        if (query.hasQueryItem("tcp_fast_open")) tcp_fast_open = query.queryItemValue("tcp_fast_open") == "true";
        if (query.hasQueryItem("tcp_multi_path")) tcp_multi_path = query.queryItemValue("tcp_multi_path") == "true";
        if (query.hasQueryItem("udp_fragment")) udp_fragment = query.queryItemValue("udp_fragment") == "true";
        return true;
    }
    bool DialFields::ParseFromJson(const QJsonObject& object)
    {
        if (object == nullptr) return false;

        if (object.contains("reuse_addr")) reuse_addr = object["reuse_addr"].toBool();
        if (object.contains("connect_timeout")) connect_timeout = object["connect_timeout"].toString();
        if (object.contains("tcp_fast_open")) tcp_fast_open = object["tcp_fast_open"].toBool();
        if (object.contains("tcp_multi_path")) tcp_multi_path = object["tcp_multi_path"].toBool();
        if (object.contains("udp_fragment")) udp_fragment = object["udp_fragment"].toBool();
        return true;
    }
    QString DialFields::ExportToLink()
    {
        QUrlQuery query;
        if (reuse_addr) query.addQueryItem("reuse_addr", "true");
        if (!connect_timeout.isEmpty()) query.addQueryItem("connect_timeout", connect_timeout);
        if (tcp_fast_open) query.addQueryItem("tcp_fast_open", "true");
        if (tcp_multi_path) query.addQueryItem("tcp_multi_path", "true");
        if (udp_fragment) query.addQueryItem("udp_fragment", "true");
        return query.toString();
    }
    QJsonObject DialFields::ExportToJson()
    {
        QJsonObject object;
        if (reuse_addr) object["reuse_addr"] = reuse_addr;
        if (!connect_timeout.isEmpty()) object["connect_timeout"] = connect_timeout;
        if (tcp_fast_open) object["tcp_fast_open"] = tcp_fast_open;
        if (tcp_multi_path) object["tcp_multi_path"] = tcp_multi_path;
        if (udp_fragment) object["udp_fragment"] = udp_fragment;
        return object;
    }
    BuildResult DialFields::Build()
    {
        return {ExportToJson(), ""};
    }
}


