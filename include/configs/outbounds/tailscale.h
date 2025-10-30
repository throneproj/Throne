#pragma once
#include "include/configs/baseConfig.h"
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class tailscale : public baseConfig, public outboundMeta
    {
        public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();

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

        tailscale()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));
            _add(new configItem("state_directory", &state_directory, itemType::string));
            _add(new configItem("auth_key", &auth_key, itemType::string));
            _add(new configItem("control_url", &control_url, itemType::string));
            _add(new configItem("ephemeral", &ephemeral, itemType::boolean));
            _add(new configItem("hostname", &hostname, itemType::string));
            _add(new configItem("accept_routes", &accept_routes, itemType::boolean));
            _add(new configItem("exit_node", &exit_node, itemType::string));
            _add(new configItem("exit_node_allow_lan_access", &exit_node_allow_lan_access, itemType::boolean));
            _add(new configItem("advertise_routes", &advertise_routes, itemType::stringList));
            _add(new configItem("advertise_exit_node", &advertise_exit_node, itemType::boolean));
            _add(new configItem("globalDNS", &globalDNS, itemType::boolean));
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


