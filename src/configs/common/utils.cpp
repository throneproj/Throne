#include "include/configs/common/utils.h"

#include "include/global/Configs.hpp"
#include <QRegularExpression>

namespace Configs
{
    namespace
    {
        constexpr qint64 XrayFragmentRangeMax = 2147483647;
    }

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

        if (dataManager->settingsRepo->xray_vless_preference == Xray::AllVLESS
            || query.queryItemValue("type") == "xhttp"
            || (query.queryItemValue("security") == "reality" && dataManager->settingsRepo->xray_vless_preference == Xray::XhttpAndReality)
            || (query.queryItemValue("encryption") != "none" && query.queryItemValue("encryption") != "")
            || query.queryItemValue("extra") != "") return true;
        return false;
    }

    QString getHeadersString(QStringList headers) {
        QString result;
        if (headers.length()%2 != 0) {
            return "";
        }
        for (int i=0;i<headers.length();i+=2) {
            result += headers[i]+"=";
            result += "\""+headers[i+1]+"\" ";
        }
        return result;
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

    bool normalizeXrayFragmentRange(const QString& value, qint64 minimum,
                                    QString* normalized) {
        static const QRegularExpression pattern(R"(^([0-9]+)(?:-([0-9]+))?$)");
        const auto match = pattern.match(value.trimmed());
        if (!match.hasMatch()) return false;

        bool fromOk = false;
        bool toOk = false;
        const qint64 from = match.captured(1).toLongLong(&fromOk);
        const qint64 to = match.captured(2).isEmpty()
            ? from
            : match.captured(2).toLongLong(&toOk);
        if (match.captured(2).isEmpty()) toOk = true;
        if (!fromOk || !toOk || from < minimum || to < from ||
            to > XrayFragmentRangeMax) {
            return false;
        }

        *normalized = from == to
            ? QString::number(from)
            : QString("%1-%2").arg(from).arg(to);
        return true;
    }

    bool normalizeXrayFragmentRangeList(const QString& value, bool positiveLast,
                                        QString* normalized) {
        const QStringList parts = value.split(',', Qt::KeepEmptyParts);
        if (parts.isEmpty()) return false;

        QStringList result;
        result.reserve(parts.size());
        for (qsizetype i = 0; i < parts.size(); ++i) {
            QString range;
            const qint64 minimum = positiveLast && i == parts.size() - 1 ? 1 : 0;
            if (!normalizeXrayFragmentRange(parts.at(i), minimum, &range)) return false;
            result.append(range);
        }
        *normalized = result.join(',');
        return true;
    }
}
