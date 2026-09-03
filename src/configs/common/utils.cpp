#include "include/configs/common/utils.h"

#include "include/global/Configs.hpp"

namespace Configs
{
    void mergeUrlQuery(QUrlQuery& baseQuery, const QString& strQuery)
    {
        QUrlQuery query = QUrlQuery(strQuery);
        for (const auto& item : query.queryItems())
        {
            baseQuery.addQueryItem(item.first, item.second);
        }
    }

    void mergeJsonObjects(QJsonObject& baseObject, const QJsonObject& obj)
    {
        for (const auto& key : obj.keys())
        {
            baseObject[key] = obj[key];
        }
    }

    QStringList jsonObjectToQStringList(const QJsonObject& obj)
    {
        auto result = QStringList();
        for (const auto& key : obj.keys())
        {
            result << key << obj[key].toString();
        }
        return result;
    }

    QJsonObject qStringListToJsonObject(const QStringList& list)
    {
        auto result = QJsonObject();
        if (list.count() %2 != 0)
        {
            qDebug() << "QStringList of odd length in qStringListToJsonObject:" << list;
            return result;
        }
        for (int i=0;i<list.size();i+=2)
        {
            result[list[i]] = list[i+1];
        }
        return result;
    }

    // TODO add setting items and use them here
    bool useXrayVless(const QString& link) {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());
        const auto transport = query.queryItemValue("type");
        // sing-box has no equivalent of the raw HTTP header, so these must go to Xray.
        const bool rawHttp = (transport.isEmpty() || transport == "tcp" || transport == "raw")
                             && query.queryItemValue("headerType") == "http";

        if (dataManager->settingsRepo->xray_vless_preference == Xray::AllVLESS
            || rawHttp
            || transport == "xhttp"
            || query.hasQueryItem("fm")
            || query.hasQueryItem("finalmask")
            || (query.queryItemValue("security") == "reality" && dataManager->settingsRepo->xray_vless_preference == Xray::XhttpAndReality)
            || (query.queryItemValue("encryption") != "none" && query.queryItemValue("encryption") != "")
            || query.queryItemValue("extra") != "") return true;
        return false;
    }

    QString getHeadersString(const QStringList& headers) {
        QString result;
        if (headers.length()%2 != 0) {
            return "";
        }
        QStringList formatted;
        formatted.reserve(headers.length()/2);

        for (int i=0;i<headers.length();i+=2) {
            formatted.append(QStringLiteral("%1=\"%2\"").arg(headers.at(i), headers.at(i + 1)));
        }
        return formatted.join(' ');
    }

    QStringList parseHeaderPairs(const QString& rawHeader) {
        bool inQuote = false;
        QString curr;
        QStringList list;
        for (const auto &ch: rawHeader) {
            if (inQuote) {
                if (ch == '"') {
                    inQuote = false;
                    list << curr;
                    curr = "";
                    continue;
                } else {
                    curr += ch;
                    continue;
                }
            }
            if (ch == '"') {
                inQuote = true;
                continue;
            }
            if (ch == ' ') {
                if (!curr.isEmpty()) {
                    list << curr;
                    curr = "";
                }
                continue;
            }
            if (ch == '=') {
                if (!curr.isEmpty()) {
                    list << curr;
                    curr = "";
                }
                continue;
            }
            curr+=ch;
        }
        if (!curr.isEmpty()) list<<curr;

        if (list.size()%2 != 0) {
            return {};
        }

        return list;
    }
}
