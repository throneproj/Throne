#pragma once
#include <QCoreApplication>
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

        bool HasServerEndpoint() override { return false; }

        // No security of its own; it inherits whatever its hops use.
        SecurityInfo GetSecurity() override { return {}; }

        // Holds no secret itself; every hop is checked separately.
        bool SupportsCredentialStrip() const override { return true; }

        // The hop rules need the local profiles; EditChain checks them at save.
        void Validate(Validation::Issues &issues) override
        {
            outbound::Validate(issues);
            if (list.isEmpty()) {
                issues << Validation::Issue{Validation::Severity::Error, "list",
                                            QCoreApplication::translate("Validation", "Add at least one profile to the chain."), {}};
            }
        }

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
