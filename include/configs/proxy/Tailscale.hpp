#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class TailscaleBean : public AbstractBean {
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

        explicit TailscaleBean() : AbstractBean(0) {
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
        };

        QString DisplayType() override { return "Tailscale"; }

        QString DisplayAddress() override {return control_url; }

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;

        bool IsEndpoint() override {return true;}
    };
} // namespace Configs
