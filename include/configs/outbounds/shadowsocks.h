#pragma once

#include "include/configs/common/multiplex.h"
#include "include/configs/common/Outbound.h"

namespace Configs
{
    inline QStringList shadowsocksMethods = {"2022-blake3-aes-128-gcm", "2022-blake3-aes-256-gcm", "2022-blake3-chacha20-poly1305", "none", "aes-128-gcm", "aes-192-gcm", "aes-256-gcm", "chacha20-ietf-poly1305", "xchacha20-ietf-poly1305", "aes-128-ctr", "aes-192-ctr", "aes-256-ctr", "aes-128-cfb", "aes-192-cfb", "aes-256-cfb", "rc4-md5", "chacha20-ietf", "xchacha20"};

    class shadowsocks : public outbound
    {
        public:
        QString method;
        QString password;
        QString plugin;
        QString plugin_opts;
        bool uot = false;
        std::shared_ptr<Multiplex> multiplex = std::make_shared<Multiplex>();

        bool HasMux() override {
            return true;
        }

        std::shared_ptr<Multiplex> GetMux() override {
            return multiplex;
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromSIP008(const QJsonObject& object);
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        QString DisplayType() override;
    };
}
