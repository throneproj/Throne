#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class ssh : public outbound
    {
        public:
        QString user;
        QString password;
        QString private_key;
        QString private_key_path;
        QString private_key_passphrase;
        QStringList host_key;
        QStringList host_key_algorithms;
        QString client_version;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
        void Validate(Validation::Issues& issues) override;

        // The core dials port 22 when none is set.
        bool ServerPortOptional() override { return true; }
    };

    namespace Checks {
        QString SshPrivateKey(const QString& text);
        // One authorized_keys line: [options] type base64 [comment], the type matching the one inside the key.
        QString SshHostKey(const QString& item);
        // Warning: servers refuse a version line without the SSH-2.0- prefix.
        QString SshClientVersion(const QString& text);
        // Warning: the core still connects, trying the "none" method.
        QString SshAuthentication(const QString& password, const QString& privateKey, const QString& privateKeyPath);
        // Warning: the core reads the inline key and ignores the file.
        QString SshKeySources(const QString& privateKey, const QString& privateKeyPath);
    }
}
