#pragma once
#include "include/configs/common/Outbound.h"
#include "QJsonArray"

namespace Configs
{
    class chain : public outbound
    {
        public:
        QList<int> list; // from in to out

        QString DisplayType() override { return QObject::tr("Chain Proxy"); };

        QString DisplayAddress() override { return ""; };

        bool ParseFromJson(const QJsonObject &object) override {
            if (object.isEmpty()) return false;
            if (object.contains("name")) name = object["name"].toString();
            if (object.contains("list")) list = QJsonArray2QListInt(object["list"].toArray());
            return true;
        }

        QJsonObject ExportToJson() override {
            QJsonObject object;
            object["name"] = name;
            object["type"] = "chain";
            object["list"] = QListInt2QJsonArray(list);
            return object;
        }

        BuildResult Build() override
        {
            return {{}, "Cannot call Build on chain config"};
        }
    };
}
