#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/utils.h"

namespace Configs {
    class direct : public outbound {
    public:
        QString DisplayType() override { return QObject::tr("Direct"); }

        bool HasServerEndpoint() override { return false; }

        // Not a tunneling protocol, so a security verdict would only mislead.
        SecurityInfo GetSecurity() override { return {}; }

        QString DisplayAddress() override {
            if (!dialFields->bind_interface.isEmpty()) return "Bind NIC: " + dialFields->bind_interface;
            if (!dialFields->inet4_bind_address.isEmpty()) return "Bind Addr4: " + dialFields->inet4_bind_address;
            if (!dialFields->inet6_bind_address.isEmpty()) return "Bind Addr6" + dialFields->inet6_bind_address;
            return "Empty";
        }

        bool ParseFromJson(const QJsonObject& object) override {
            if (object.isEmpty()) return false;
            if (object.contains("tag")) name = object["tag"].toString();
            dialFields->ParseFromJson(object);
            return true;
        }

        QJsonObject ExportToJson() override {
            QJsonObject object;
            object["type"] = "direct";
            if (!name.isEmpty()) object["tag"] = name;
            mergeJsonObjects(object, dialFields->ExportToJson());
            return object;
        }

        BuildResult Build() override {
            QJsonObject object;
            object["type"] = "direct";
            mergeJsonObjects(object, dialFields->Build().object);
            return {object, ""};
        }
    };
}
