#pragma once

#include <include/configs/outbounds/tailscale.h>
#include <include/configs/outbounds/wireguard.h>

#include "include/configs/common/Outbound.h"
#include "include/configs/outbounds/anyTLS.h"
#include "include/configs/outbounds/chain.h"
#include "include/configs/outbounds/custom.h"
#include "include/configs/outbounds/extracore.h"
#include "include/configs/outbounds/socks.h"
#include "include/configs/outbounds/http.h"
#include "include/configs/outbounds/hysteria.h"
#include "include/configs/outbounds/shadowsocks.h"
#include "include/configs/outbounds/ssh.h"
#include "include/configs/outbounds/trojan.h"
#include "include/configs/outbounds/tuic.h"
#include "include/configs/outbounds/vless.h"
#include "include/configs/outbounds/vmess.h"
#include "include/configs/outbounds/xrayVless.h"

#include "include/global/CountryHelper.hpp"
#include "include/stats/traffic/TrafficData.hpp"

namespace Configs {
    class Profile {
    public:
        QString type;
        QString name;

        int id = -1;
        int gid = 0;
        int latency = 0;
        QString dl_speed;
        QString ul_speed;
        QString test_country;
        std::shared_ptr<Configs::outbound> outbound;
        std::shared_ptr<Stats::TrafficData> traffic_data = std::make_shared<Stats::TrafficData>("");

        QString full_test_report;

        Profile() = default;
        Profile(Configs::outbound *outbound, const QString &type_);

        [[nodiscard]] QString DisplayTestResult() const;

        [[nodiscard]] QColor DisplayLatencyColor() const;

        [[nodiscard]] Configs::socks *Socks() const {
            return dynamic_cast<Configs::socks *>(outbound.get());
        };

        [[nodiscard]] Configs::http *Http() const {
            return dynamic_cast<Configs::http *>(outbound.get());
        };

        [[nodiscard]] Configs::shadowsocks *ShadowSocks() const {
            return dynamic_cast<Configs::shadowsocks *>(outbound.get());
        };

        [[nodiscard]] Configs::vmess *VMess() const {
            return dynamic_cast<Configs::vmess *>(outbound.get());
        };

        [[nodiscard]] Configs::Trojan *Trojan() const {
            return dynamic_cast<Configs::Trojan *>(outbound.get());
        };

        [[nodiscard]] Configs::vless *VLESS() const {
            return dynamic_cast<Configs::vless *>(outbound.get());
        };

        [[nodiscard]] Configs::xrayVless *XrayVLESS() const {
            return dynamic_cast<Configs::xrayVless *>(outbound.get());
        }

        [[nodiscard]] Configs::anyTLS *AnyTLS() const {
            return dynamic_cast<Configs::anyTLS *>(outbound.get());
        };

        [[nodiscard]] Configs::hysteria *Hysteria() const {
            return dynamic_cast<Configs::hysteria *>(outbound.get());
        };

        [[nodiscard]] Configs::ssh *SSH() const {
            return dynamic_cast<Configs::ssh *>(outbound.get());
        };

        [[nodiscard]] Configs::tailscale *Tailscale() const {
            return dynamic_cast<Configs::tailscale *>(outbound.get());
        };

        [[nodiscard]] Configs::tuic *TUIC() const {
            return dynamic_cast<Configs::tuic *>(outbound.get());
        };

        [[nodiscard]] Configs::wireguard *Wireguard() const {
            return dynamic_cast<Configs::wireguard *>(outbound.get());
        };

        [[nodiscard]] Configs::Custom *Custom() const {
            return dynamic_cast<Configs::Custom *>(outbound.get());
        };

        [[nodiscard]] Configs::chain *Chain() const {
            return dynamic_cast<Configs::chain *>(outbound.get());
        };

        [[nodiscard]] Configs::extracore *ExtraCore() const {
            return dynamic_cast<Configs::extracore *>(outbound.get());
        };
    };
    class ProfileFilter {
    public:
        static void Uniq(
            const QList<std::shared_ptr<Profile>> &in,
            QList<std::shared_ptr<Profile>> &out,
            bool keep_last = false   // def keep first
        );

        static void Common(
            const QList<std::shared_ptr<Profile>> &src,
            const QList<std::shared_ptr<Profile>> &dst,
            QList<std::shared_ptr<Profile>> &outSrc,
            QList<std::shared_ptr<Profile>> &outDst
        );

        static void OnlyInSrc(
            const QList<std::shared_ptr<Profile>> &src,
            const QList<std::shared_ptr<Profile>> &dst,
            QList<std::shared_ptr<Profile>> &out
        );

        static void OnlyInSrc_ByPointer(
            const QList<std::shared_ptr<Profile>> &src,
            const QList<std::shared_ptr<Profile>> &dst,
            QList<std::shared_ptr<Profile>> &out);
    };
} // namespace Configs
