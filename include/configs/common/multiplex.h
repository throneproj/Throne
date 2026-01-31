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

        int getMuxState() {
            if (enabled) return 1;
            if (!unspecified) return 2;
            return 0;
        }

        void saveMuxState(int state) {
            if (state == 1) {
                enabled = true;
                return;
            }
            enabled = false;
            if (state == 0) unspecified = true;
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };
}
