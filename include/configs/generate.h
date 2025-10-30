#pragma once
#include <QJsonObject>

namespace Configs
{
    struct BuildResult {
        QJsonObject object;
        QString error;
    };
}
