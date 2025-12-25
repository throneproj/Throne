#include "include/configs/common/utils.h"

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
        auto query = QUrlQuery(url.query(QUrl::ComponentFormattingOption::FullyDecoded));

        if (query.queryItemValue("type") == "xhttp"
            || query.queryItemValue("security") == "reality"
            || (query.queryItemValue("encryption") != "none" && query.queryItemValue("encryption") != "")
            || query.queryItemValue("extra") != "") return true;
        return false;
    }
}
