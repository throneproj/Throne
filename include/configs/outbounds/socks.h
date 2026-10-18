#pragma once

#include "include/configs/common/Outbound.h"

namespace Configs
{
    namespace Checks {
        // SOCKS4 sends only a user ID, and SOCKS5 sends the password only together with a user name.
        QString SocksAuth(int version, const QString &username, const QString &password);
    }

    class socks : public outbound
    {
        public:
        QString username;
        QString password;
        int version = 5;
        // With version 4: SOCKS4a, the server resolves host names (plain SOCKS4 makes the core resolve them).
        bool socks4a = false;
        bool uot = false;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;

        void Validate(Validation::Issues &issues) override;
    };
}
