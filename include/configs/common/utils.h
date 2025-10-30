#pragma once
#include <QJsonObject>
#include <QUrlQuery>

namespace Configs
{
    void mergeUrlQuery(QUrlQuery& baseQuery, const QString& strQuery);

    void mergeJsonObjects(QJsonObject& baseObject, const QJsonObject& obj);
}
