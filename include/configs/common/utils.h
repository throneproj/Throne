#pragma once
#include <QJsonObject>
#include <QUrlQuery>

namespace Configs
{
    void mergeUrlQuery(QUrlQuery& baseQuery, const QString& strQuery);

    void mergeJsonObjects(QJsonObject& baseObject, const QJsonObject& obj);

    QStringList jsonObjectToQStringList(const QJsonObject& obj);

    QJsonObject qStringListToJsonObject(const QStringList& list);

    bool useXrayVless(const QString& link);

    QString getHeadersString(const QStringList& headers);

    QStringList parseHeaderPairs(const QString& rawHeader);
}
