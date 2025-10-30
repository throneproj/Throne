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

        shadowsocks() : outbound()
        {
            _add(new configItem("method", &method, string));
            _add(new configItem("password", &password, string));
            _add(new configItem("plugin", &plugin, string));
            _add(new configItem("plugin_opts", &plugin_opts, string));
            _add(new configItem("uot", &uot, itemType::boolean));
            _add(new configItem("multiplex", dynamic_cast<JsonStore *>(multiplex.get()), jsonStore));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        // outboundMeta overrides
        QString DisplayAddress() override;
        QString DisplayName() override;
        QString DisplayType() override;
        QString DisplayTypeAndName() override;
        bool IsEndpoint() override;
    };
}
