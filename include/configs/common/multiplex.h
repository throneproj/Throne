#pragma once
#include "include/configs/baseConfig.h"
#include "include/global/Validation.hpp"

namespace Configs
{
    inline QStringList muxProtocols = {"smux", "yamux", "h2mux"};

    // Index of "Custom" in the profile's Keep Default / On / Off / Custom multiplex combo.
    inline constexpr int kMuxCustom = 3;

    namespace Checks {
        QString MuxProtocol(const QString& protocol);
        // A warning: brutal is emitted and stored only under a Custom multiplex (Keep Default and Off drop it).
        QString BrutalNeedsMux(bool brutal, bool muxOn);
    }

    // A complete sing-box multiplex setup: the global preset, or a profile's own under Custom.
    struct MuxSettings
    {
        QString protocol;
        // Pool by max_connections + min_streams instead of max_streams per connection.
        bool by_connections = false;
        int max_streams = 0;
        int max_connections = 0;
        int min_streams = 0;
        bool padding = false;
        bool brutal = false;
        int brutal_up_mbps = 0;
        int brutal_down_mbps = 0;

        static MuxSettings Preset();
        void SaveAsPreset() const;
        // The enabled multiplex object for the core config.
        QJsonObject Build() const;
    };

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

        // Explicitly enabled with settings of its own is "Custom"; On without them follows the preset.
        int getMuxState() const {
            if (unspecified) return 0;
            if (!enabled) return 2;
            return HasOwnSettings() ? kMuxCustom : 1;
        }

        // Every state but Custom drops the own settings; Custom takes them from SetCustomSettings().
        void saveMuxState(int state) {
            unspecified = state == 0;
            enabled = state == 1 || state == kMuxCustom;
            if (state != kMuxCustom) SetCustomSettings({});
        }

        bool HasOwnSettings() const;
        bool IsCustom() const { return !unspecified && enabled && HasOwnSettings(); }
        // The own settings with unset ones taken from the preset; the preset alone when there are none.
        MuxSettings CustomSettings() const;
        void SetCustomSettings(const MuxSettings& settings);

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
