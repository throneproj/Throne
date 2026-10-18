#pragma once
#include "include/configs/baseConfig.h"
#include "include/global/Validation.hpp"

namespace Configs
{
    namespace Checks {
        // An IPv6 literal; a zone (fe80::1%eth0) is allowed, as netip.ParseAddr takes it.
        QString BindAddress6(const QString& text);
    }

    class DialFields : public baseConfig
    {
        public:
        bool reuse_addr = false;
        QString connect_timeout;
        bool tcp_fast_open = false;
        bool tcp_multi_path = false;
        bool udp_fragment = false;
        QString bind_interface;
        QString inet4_bind_address;
        QString inet6_bind_address;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        void Validate(Validation::Issues& issues);
    };
}
