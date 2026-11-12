#pragma once
#include "include/configs/baseConfig.h"
#include "include/global/Validation.hpp"

#include <optional>

namespace Configs {
    inline QStringList xrayXudpProxyUDP443Modes = {"reject", "allow", "skip"};

    // Index of "Custom" in the profile's Keep Default / On / Off / Custom Xray mux combo.
    inline constexpr int kXrayMuxCustom = 3;

    // Xray decodes every mux count into an int16.
    inline constexpr int kXrayMuxMaxCount = 32767;

    namespace Checks {
        QString XrayXudpProxyUDP443(const QString& mode);
    }

    // A complete Xray mux setup: the global preset, or a profile's own under Custom.
    struct XrayMuxSettings
    {
        // Streams per connection; 0 is Xray's 8 and -1 sends TCP outside the mux.
        int concurrency = 8;
        // Throne's Xray fork caps the pool here; 0 leaves it unbounded.
        int max_connections = 0;
        // 0 carries UDP on the TCP connections, -1 sends it outside the mux.
        int xudp_concurrency = 16;
        QString xudp_proxy_udp443 = "reject";

        static XrayMuxSettings Preset();
        void SaveAsPreset() const;
        // The enabled mux object for the Xray config.
        QJsonObject Build() const;
    };

    class xrayMultiplex : public baseConfig {
        public:
        bool enabled = false;
        bool useDefault = true;
        // Unset ones follow the preset; Xray gives 0 and -1 meanings of their own, so they cannot stand for unset.
        std::optional<int> concurrency;
        std::optional<int> maxConnections;
        std::optional<int> xudpConcurrency;
        QString xudpProxyUDP443;

        // Explicitly enabled with settings of its own is "Custom"; On without them follows the preset.
        int getMuxState() const {
            if (useDefault) return 0;
            if (!enabled) return 2;
            return HasOwnSettings() ? kXrayMuxCustom : 1;
        }

        // Every state but Custom drops the own settings; Custom takes them from SetCustomSettings().
        void saveMuxState(int state) {
            useDefault = state == 0;
            enabled = state == 1 || state == kXrayMuxCustom;
            if (state != kXrayMuxCustom) ClearOwnSettings();
        }

        bool HasOwnSettings() const;
        bool IsCustom() const { return !useDefault && enabled && HasOwnSettings(); }
        // The own settings with unset ones taken from the preset; the preset alone when there are none.
        XrayMuxSettings CustomSettings() const;
        void SetCustomSettings(const XrayMuxSettings& settings);
        void ClearOwnSettings();

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
        // Keep Default resolves to defaultOn.
        QJsonObject BuildMux(bool defaultOn);

        void Validate(Validation::Issues& issues);

        private:
        void dropLegacyXudpDefault();
    };
}
