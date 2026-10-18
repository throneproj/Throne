#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class tailscale : public outbound
    {
        public:
        bool HasServerEndpoint() override { return false; }

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

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        void SetAddress(QString newAddr) override;
        QString GetAddress() override;
        QString DisplayAddress() override;
        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
        bool IsEndpoint() override;
        void Validate(Validation::Issues& issues) override;
    };

    namespace Checks {
        QString TailscaleControlUrl(const QString& text);
        // One advertised route: a prefix, and not the default route.
        QString TailscaleRoute(const QString& item);
        QString TailscaleExitNode(const QString& exitNode, bool advertiseExitNode);
    }
}


