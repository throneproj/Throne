#pragma once

#include "include/global/Configs.hpp"
#include "include/stats/traffic/TrafficData.hpp"
#include "include/configs/proxy/AbstractBean.hpp"
#include "include/configs/proxy/ExtraCore.h"

namespace Configs {
    class SocksHttpBean;

    class ShadowSocksBean;

    class VMessBean;

    class TrojanVLESSBean;

    class NaiveBean;

    class QUICBean;

    class AnyTLSBean;

    class WireguardBean;

    class TailscaleBean;

    class SSHBean;

    class CustomBean;

    class ChainBean;
}; // namespace Configs

namespace Configs {
    class ProxyEntity : public JsonStore {
    public:
        QString type;

        int id = -1;
        int gid = 0;
        int latency = 0;
        QString dl_speed;
        QString ul_speed;
        std::shared_ptr<Configs::AbstractBean> bean;
        std::shared_ptr<Stats::TrafficData> traffic_data = std::make_shared<Stats::TrafficData>("");

        QString full_test_report;

        ProxyEntity(Configs::AbstractBean *bean, const QString &type_);

        [[nodiscard]] QString DisplayTestResult() const;

        [[nodiscard]] QColor DisplayLatencyColor() const;

        [[nodiscard]] Configs::ChainBean *ChainBean() const {
            return (Configs::ChainBean *) bean.get();
        };

        [[nodiscard]] Configs::SocksHttpBean *SocksHTTPBean() const {
            return (Configs::SocksHttpBean *) bean.get();
        };

        [[nodiscard]] Configs::ShadowSocksBean *ShadowSocksBean() const {
            return (Configs::ShadowSocksBean *) bean.get();
        };

        [[nodiscard]] Configs::VMessBean *VMessBean() const {
            return (Configs::VMessBean *) bean.get();
        };

        [[nodiscard]] Configs::TrojanVLESSBean *TrojanVLESSBean() const {
            return (Configs::TrojanVLESSBean *) bean.get();
        };

        [[nodiscard]] Configs::NaiveBean *NaiveBean() const {
            return (Configs::NaiveBean *) bean.get();
        };

        [[nodiscard]] Configs::QUICBean *QUICBean() const {
            return (Configs::QUICBean *) bean.get();
        };

        [[nodiscard]] Configs::AnyTLSBean *AnyTLSBean() const {
            return (Configs::AnyTLSBean *) bean.get();
        };

        [[nodiscard]] Configs::WireguardBean *WireguardBean() const {
            return (Configs::WireguardBean *) bean.get();
        };

        [[nodiscard]] Configs::TailscaleBean *TailscaleBean() const
        {
            return (Configs::TailscaleBean *) bean.get();
        }

        [[nodiscard]] Configs::SSHBean *SSHBean() const {
            return (Configs::SSHBean *) bean.get();
        };

        [[nodiscard]] Configs::CustomBean *CustomBean() const {
            return (Configs::CustomBean *) bean.get();
        };

        [[nodiscard]] Configs::ExtraCoreBean *ExtraCoreBean() const {
            return (Configs::ExtraCoreBean *) bean.get();
        };
    };
} // namespace Configs
