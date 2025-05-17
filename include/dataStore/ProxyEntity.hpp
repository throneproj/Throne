#pragma once

#include "include/global/NekoGui.hpp"
#include "include/stats/traffic/TrafficData.hpp"
#include "include/configs/proxy/AbstractBean.hpp"
#include "include/configs/proxy/ExtraCore.h"

namespace NekoGui_fmt {
    class SocksHttpBean;

    class ShadowSocksBean;

    class VMessBean;

    class TrojanVLESSBean;

    class NaiveBean;

    class QUICBean;

    class WireguardBean;

    class SSHBean;

    class CustomBean;

    class ChainBean;
}; // namespace NekoGui_fmt

namespace NekoGui {
    class ProxyEntity : public JsonStore {
    public:
        QString type;

        int id = -1;
        int gid = 0;
        int latency = 0;
        QString dl_speed;
        QString ul_speed;
        std::shared_ptr<NekoGui_fmt::AbstractBean> bean;
        std::shared_ptr<NekoGui_traffic::TrafficData> traffic_data = std::make_shared<NekoGui_traffic::TrafficData>("");

        QString full_test_report;

        ProxyEntity(NekoGui_fmt::AbstractBean *bean, const QString &type_);

        [[nodiscard]] QString DisplayTestResult() const;

        [[nodiscard]] QColor DisplayLatencyColor() const;

        [[nodiscard]] NekoGui_fmt::ChainBean *ChainBean() const {
            return (NekoGui_fmt::ChainBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::SocksHttpBean *SocksHTTPBean() const {
            return (NekoGui_fmt::SocksHttpBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::ShadowSocksBean *ShadowSocksBean() const {
            return (NekoGui_fmt::ShadowSocksBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::VMessBean *VMessBean() const {
            return (NekoGui_fmt::VMessBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::TrojanVLESSBean *TrojanVLESSBean() const {
            return (NekoGui_fmt::TrojanVLESSBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::NaiveBean *NaiveBean() const {
            return (NekoGui_fmt::NaiveBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::QUICBean *QUICBean() const {
            return (NekoGui_fmt::QUICBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::WireguardBean *WireguardBean() const {
            return (NekoGui_fmt::WireguardBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::SSHBean *SSHBean() const {
            return (NekoGui_fmt::SSHBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::CustomBean *CustomBean() const {
            return (NekoGui_fmt::CustomBean *) bean.get();
        };

        [[nodiscard]] NekoGui_fmt::ExtraCoreBean *ExtraCoreBean() const {
            return (NekoGui_fmt::ExtraCoreBean *) bean.get();
        };
    };
} // namespace NekoGui
