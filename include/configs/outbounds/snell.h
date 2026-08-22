#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    // Client versions the core can speak; the library ships a v5 server only.
    inline QStringList snellVersions = {"4", "6"};
    inline QStringList snellObfsModes = {"", "none", "http", "tls"};
    inline QStringList snellV6Modes = {"", "default", "unshaped", "unsafe-raw"};
    inline QStringList snellNetworks = {"", "tcp", "udp"};

    class snell : public outbound
    {
        public:
        int version = 4;
        QString psk;
        QString userkey;
        bool reuse = false;
        QString network;
        // v4 only
        QString obfs_mode;
        QString obfs_host;
        // v6 only
        QString mode;

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
    };
}
