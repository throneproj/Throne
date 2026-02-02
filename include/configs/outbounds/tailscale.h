#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class tailscale : public outbound
    {
        public:
        QString state_directory = "$HOME/.tailscale";
        QString auth_key;
        QString control_url = "https://controlplane.tailscale.com";
        bool ephemeral = false;
        QString hostname;
        bool accept_routes = false;
        QString exit_node;
        bool exit_node_allow_lan_access = false;
        QStringList advertise_routes;
        bool advertise_exit_node = false;
        bool globalDNS = false;

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        void SetAddress(QString newAddr) override;
        QString GetAddress() override;
        QString DisplayAddress() override;
        QString DisplayType() override;
        bool IsEndpoint() override;
    };
}


