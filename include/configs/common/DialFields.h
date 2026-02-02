#pragma once
#include "include/configs/baseConfig.h"

namespace Configs
{
    class DialFields : public baseConfig
    {
        public:
        bool reuse_addr = false;
        QString connect_timeout;
        bool tcp_fast_open = false;
        bool tcp_multi_path = false;
        bool udp_fragment = false;

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
