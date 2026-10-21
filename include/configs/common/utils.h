#pragma once
#include <QJsonObject>
#include <QJsonValue>
#include <QUrlQuery>

#include <climits>

namespace Configs
{
    // Import gate for an optional value: a non-empty rule message is reported as ignored and the value is not kept.
    bool AcceptOptional(const QString& field, const QString& message, const QString& value = {});

    // A whole number in [min, max]; anything else leaves target unchanged and is reported.
    void ReadQueryInt(const QUrlQuery& query, const QString& key, int& target, int min = 0, int max = INT_MAX);

    // sing-box Listable values: one string or an array of strings; other JSON types are reported and skipped.
    QStringList JsonStringList(const QJsonValue& value, const QString& field);

    // Builds up to 1.3.2 saved translated combo labels ("BBR v2", "нативный", #1943); maps one back to its identifier, else returns it trimmed.
    QString LegacyComboValue(const QString& value, const QStringList& identifiers);

    namespace Checks {
        // The editors' header text: Name=value pairs separated by spaces, values optionally quoted.
        QString HeaderPairs(const QString& text);

        // An HTTP Host header: host[:port], comma separated when allowList.
        QString HostHeader(const QString& text, bool allowList);

        // An SNI: a domain name or IP address, without a scheme or port.
        QString ServerName(const QString& text);
    }

    void mergeUrlQuery(QUrlQuery& baseQuery, const QString& strQuery);

    QString formDecodedQueryValue(const QUrlQuery& query, const QString& key);

    void mergeJsonObjects(QJsonObject& baseObject, const QJsonObject& obj);

    QStringList jsonObjectToQStringList(const QJsonObject& obj);

    QJsonObject qStringListToJsonObject(const QStringList& list);

    bool useXrayVless(const QString& link);

    QString toAceHost(const QString& host);

    // Xray's own private list (common/geodata/consts.go): loopback, LAN, CGNAT and reserved ranges, local-only names.
    bool IsPrivateHost(const QString& host);

    QString getHeadersString(const QStringList& headers);

    QStringList parseHeaderPairs(const QString& rawHeader);
}
