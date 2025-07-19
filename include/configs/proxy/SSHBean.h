#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class SSHBean : public AbstractBean {
    public:
        QString user = "root";
        QString password;
        QString privateKey;
        QString privateKeyPath;
        QString privateKeyPass;
        QStringList hostKey;
        QStringList hostKeyAlgs;
        QString clientVersion;

        SSHBean() : AbstractBean(0) {
            _add(new configItem("user", &user, itemType::string));
            _add(new configItem("password", &password, itemType::string));
            _add(new configItem("privateKey", &privateKey, itemType::string));
            _add(new configItem("privateKeyPath", &privateKeyPath, itemType::string));
            _add(new configItem("privateKeyPass", &privateKeyPass, itemType::string));
            _add(new configItem("hostKey", &hostKey, itemType::stringList));
            _add(new configItem("hostKeyAlgs", &hostKeyAlgs, itemType::stringList));
            _add(new configItem("clientVersion", &clientVersion, itemType::string));
        };

        QString DisplayType() override { return "SSH"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;
    };
}
