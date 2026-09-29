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
        const auto security = query.queryItemValue("security");
        // sing-box's http transport speaks the raw HTTP header only in plaintext; TLS (which a bare sni also enables) turns it into h2
        const bool rawHttpOverTls = (transport.isEmpty() || transport == "tcp" || transport == "raw")
                                    && query.queryItemValue("headerType") == "http"
                                    && ((!security.isEmpty() && security != "none")
                                        || !query.queryItemValue("sni").isEmpty()
                                        || !query.queryItemValue("peer").isEmpty());

        if (dataManager->settingsRepo->xray_vless_preference == Xray::AllVLESS
            || rawHttpOverTls
            || transport == "xhttp"
            || query.hasQueryItem("fm")
            || query.hasQueryItem("finalmask")
            || (security == "reality" && dataManager->settingsRepo->xray_vless_preference == Xray::XhttpAndReality)
            || (query.queryItemValue("encryption") != "none" && query.queryItemValue("encryption") != "")
            || query.queryItemValue("extra") != "") return true;
        return false;
    }

    QString toAceHost(const QString& host)
    {
        // the http transport and Xray's raw header carry a comma list of hosts
        if (host.contains(',')) {
            auto parts = host.split(',');
            for (auto& part : parts) part = toAceHost(part.trimmed());
            return parts.join(',');
        }
        bool ascii = true;
        for (const auto ch : host) {
            if (ch.unicode() > 0x7F) {
                ascii = false;
                break;
            }
        }
        if (ascii) return host;
        // toAce is empty for IP literals and for names it rejects
        const auto ace = QString::fromLatin1(QUrl::toAce(host));
        return ace.isEmpty() ? host : ace;
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
