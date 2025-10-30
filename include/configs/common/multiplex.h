#pragma once
#include "include/configs/baseConfig.h"

namespace Configs
{
    inline QStringList muxProtocols = {"smux", "yamux", "h2mux"};

    class TcpBrutal : public baseConfig
    {
    public:
        bool enabled = false;
        int up_mbps = 0;
        int down_mbps = 0;

        TcpBrutal()
        {
            _add(new configItem("enabled", &enabled, boolean));
            _add(new configItem("up_mbps", &up_mbps, integer));
            _add(new configItem("down_mbps", &down_mbps, integer));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class Multiplex : public baseConfig
    {
        public:
        bool enabled = false;
        bool unspecified = false;
        QString protocol;
        int max_connections = 0;
        int min_streams = 0;
        int max_streams = 0;
        bool padding = false;
        std::shared_ptr<TcpBrutal> brutal = std::make_shared<TcpBrutal>();

        Multiplex()
        {
            _add(new configItem("enabled", &enabled, boolean));
            _add(new configItem("unspecified", &unspecified, boolean));
            _add(new configItem("protocol", &protocol, string));
            _add(new configItem("max_connections", &max_connections, integer));
            _add(new configItem("min_streams", &min_streams, integer));
            _add(new configItem("max_streams", &max_streams, integer));
            _add(new configItem("padding", &padding, boolean));
            _add(new configItem("brutal", dynamic_cast<JsonStore *>(brutal.get()), jsonStore));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
