#pragma once
#include "include/configs/baseConfig.h"
#include "include/global/Validation.hpp"

namespace Configs
{
    inline QStringList muxProtocols = {"smux", "yamux", "h2mux"};

    namespace Checks {
        QString MuxProtocol(const QString& protocol);
        // A warning for a speed Build sends as 1 Mbps; text that is not a number is left to an Int rule.
        QString BrutalSpeed(const QString& text);
        // A warning: brutal is emitted and stored only while multiplex is explicitly On (Keep Default drops it).
        QString BrutalNeedsMux(bool brutal, bool muxOn);
    }

    class TcpBrutal : public baseConfig
    {
    public:
        bool enabled = false;
        int up_mbps = 0;
        int down_mbps = 0;

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
        bool unspecified = true; // tri-state defaults to "Keep Default"
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
            unspecified = false;
            if (state == 1) {
                enabled = true;
                return;
            }
            enabled = false;
            if (state == 0) unspecified = true;
        }

        // On, or Keep Default with the global multiplex default on.
        bool EffectivelyOn() const;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        void Validate(Validation::Issues& issues);
    };
}
