#include "include/configs/common/Protocols.h"

#include <algorithm>

#include <QCoreApplication>

#include "include/configs/common/OutboundFactory.h"
#include "include/configs/outbounds/socks.h"
#include "include/configs/outbounds/http.h"
#include "include/configs/outbounds/shadowsocks.h"
#include "include/configs/outbounds/chain.h"
#include "include/configs/outbounds/autoselector.h"
#include "include/configs/outbounds/vmess.h"
#include "include/configs/outbounds/trojan.h"
#include "include/configs/outbounds/vless.h"
#include "include/configs/outbounds/xrayVless.h"
#include "include/configs/outbounds/hysteria.h"
#include "include/configs/outbounds/tuic.h"
#include "include/configs/outbounds/juicity.h"
#include "include/configs/outbounds/trusttunnel.h"
#include "include/configs/outbounds/anyTLS.h"
#include "include/configs/outbounds/mieru.h"
#include "include/configs/outbounds/snell.h"
#include "include/configs/outbounds/shadowtls.h"
#include "include/configs/outbounds/wireguard.h"
#include "include/configs/outbounds/masque.h"
#include "include/configs/outbounds/openvpn.h"
#include "include/configs/outbounds/openconnect.h"
#include "include/configs/outbounds/tailscale.h"
#include "include/configs/outbounds/ssh.h"
#include "include/configs/outbounds/custom.h"
#include "include/configs/outbounds/extracore.h"
#include "include/configs/outbounds/naive.h"
#include "include/configs/outbounds/direct.h"
#include "include/configs/outbounds/dnstt.h"
#include "include/configs/outbounds/masterdns.h"

namespace Configs::Protocols {
    namespace {
        template <class T>
        outbound *protocolsCreate() {
            return new T();
        }

        constexpr ProtocolInfo kProtocolsTable[] = {
            {.id = "autoselector", .create = protocolsCreate<autoSelector>, .flags = Serverless | NoScan | Composite,
             .foreignRefusal = QT_TRANSLATE_NOOP("Validation", "Refused to import an Auto Selector profile: the group it tracks belongs to another installation.")},
            {.id = "socks", .create = protocolsCreate<socks>,
             .importSchemes = {"socks5://", "socks4://", "socks4a://", "socks://"}, .importSingBox = {"socks"}, .importClash = {"socks5"},
             .filterRank = 10, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "SOCKS")},
            {.id = "http", .create = protocolsCreate<http>,
             .importSchemes = {"http://", "https://"}, .importSingBox = {"http"}, .importClash = {"http"},
             .filterRank = 11, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "HTTP")},
            {.id = "shadowsocks", .create = protocolsCreate<shadowsocks>,
             .importSchemes = {"ss://"}, .importSingBox = {"shadowsocks"}, .importClash = {"ss"},
             .filterRank = 1, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Shadowsocks")},
            {.id = "trojan", .create = protocolsCreate<Trojan>,
             .importSchemes = {"trojan://"}, .importSingBox = {"trojan"}, .importClash = {"trojan"},
             .filterRank = 4, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Trojan")},
            {.id = "vmess", .create = protocolsCreate<vmess>,
             .importSchemes = {"vmess://"}, .importSingBox = {"vmess"}, .importClash = {"vmess"},
             .filterRank = 2, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "VMess")},
            {.id = "vless", .create = protocolsCreate<vless>,
             .importSchemes = {"vless://"}, .importSingBox = {"vless"}, .importClash = {"vless"},
             .filterRank = 3, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "VLESS")},
            {.id = "xrayvless", .create = protocolsCreate<xrayVless>, .filterAs = "vless"},
            {.id = "hysteria", .alias = "hysteria2", .create = protocolsCreate<hysteria>,
             .importSchemes = {"hysteria://", "hysteria2://", "hy2://"}, .importSingBox = {"hysteria", "hysteria2"},
             .importClash = {"hysteria", "hysteria2"},
             .filterRank = 5, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Hysteria / Hysteria2")},
            {.id = "tuic", .create = protocolsCreate<tuic>,
             .importSchemes = {"tuic://"}, .importSingBox = {"tuic"}, .importClash = {"tuic"},
             .filterRank = 6, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "TUIC")},
            {.id = "juicity", .create = protocolsCreate<juicity>,
             .importSchemes = {"juicity://"}, .importSingBox = {"juicity"},
             .filterRank = 14, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Juicity")},
            {.id = "naive", .create = protocolsCreate<naive>,
             .importSchemes = {"naive+https://", "naive+quic://"}, .importSingBox = {"naive"},
             .filterRank = 16, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "NaiveProxy")},
            {.id = "trusttunnel", .create = protocolsCreate<trusttunnel>,
             .importSchemes = {"tt://"}, .importSingBox = {"trusttunnel"},
             .filterRank = 17, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "TrustTunnel")},
            {.id = "anytls", .create = protocolsCreate<anyTLS>,
             .importSchemes = {"anytls://"}, .importSingBox = {"anytls"}, .importClash = {"anytls"},
             .filterRank = 7, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "AnyTLS")},
            {.id = "mieru", .create = protocolsCreate<mieru>,
             .importSchemes = {"mierus://", "mieru://"}, .importSingBox = {"mieru"},
             .filterRank = 15, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Mieru")},
            {.id = "snell", .create = protocolsCreate<snell>,
             .importSchemes = {"snell://"}, .importSingBox = {"snell"}, .importClash = {"snell"},
             .filterRank = 13, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Snell")},
            {.id = "shadowtls", .create = protocolsCreate<shadowtls>,
             .importSchemes = {"shadowtls://"}, .importSingBox = {"shadowtls"},
             .filterRank = 8, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "ShadowTLS")},
            {.id = "wireguard", .create = protocolsCreate<wireguard>,
             .importSchemes = {"awg://", "wg://", "wireguard://"}, .importSingBox = {"wireguard"}, .importClash = {"wireguard"},
             .filterRank = 9, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "WireGuard / AmneziaWG")},
            {.id = "masque", .create = protocolsCreate<masque>,
             .importSingBox = {"masque"}, .importClash = {"masque"},
             .filterRank = 18, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "MASQUE")},
            {.id = "openvpn", .create = protocolsCreate<openvpn>, .flags = VpnEndpoint,
             .filterRank = 21, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "OpenVPN")},
            {.id = "openconnect", .create = protocolsCreate<openconnect>, .flags = VpnEndpoint,
             .filterRank = 22, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "OpenConnect")},
            {.id = "tailscale", .create = protocolsCreate<tailscale>, .flags = Serverless | NoScan},
            {.id = "ssh", .create = protocolsCreate<ssh>,
             .importSchemes = {"ssh://"}, .importSingBox = {"ssh"}, .importClash = {"ssh"},
             .filterRank = 12, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "SSH")},
            // dnst:// links of the masterdns transport go to masterdns, see Subscription's Parser::link().
            {.id = "dnstt", .create = protocolsCreate<dnstt>, .flags = Serverless | NoScan | DnsTunnel,
             .importSchemes = {"dnst://", "slipnet://", "slipnet-enc://", "slipnet-bundle-enc://"}, .importSingBox = {"dnstt"},
             .filterRank = 19, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "DNSTT")},
            {.id = "masterdns", .create = protocolsCreate<masterdns>, .flags = Serverless | NoScan | DnsTunnel,
             .importSchemes = {"masterdns://", "stormdns://"}, .importSingBox = {"masterdns"},
             .filterRank = 20, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "MasterDNS")},
            {.id = "direct", .create = protocolsCreate<direct>, .flags = Serverless | NoScan | Advanced},
            {.id = "custom", .create = protocolsCreate<Custom>, .flags = Serverless | NoScan | Advanced,
             .filterRank = 23, .filterLabel = QT_TRANSLATE_NOOP("DialogEditGroupAdvanced", "Custom (JSON)")},
            {.id = "extracore", .create = protocolsCreate<extracore>, .flags = Serverless | NoScan | Advanced | RunsProgram,
             .foreignRefusal = QT_TRANSLATE_NOOP("Validation", "Refused to import an Extra Core profile: it would run a program on this computer.")},
            {.id = "chain", .create = protocolsCreate<chain>, .flags = Serverless | Advanced | Composite,
             .foreignRefusal = QT_TRANSLATE_NOOP("Validation", "Refused to import a Proxy Chain profile: its hops are profiles of another installation.")},
        };

        constexpr QLatin1String protocolsLatin1(std::string_view text) {
            return QLatin1String(text.data(), qsizetype(text.size()));
        }

        constexpr bool protocolsIdsUnique() {
            for (const auto &a : kProtocolsTable) {
                if (a.id.empty()) return false;
                for (const auto &b : kProtocolsTable) {
                    if (&a != &b && (a.id == b.id || (!a.alias.empty() && a.alias == b.alias))) return false;
                    if (!a.alias.empty() && a.alias == b.id) return false;
                }
            }
            return true;
        }

        template <std::size_t N>
        constexpr bool protocolsNamesUnique(std::array<std::string_view, N> ProtocolInfo::*column) {
            for (const auto &a : kProtocolsTable) {
                for (const auto name : a.*column) {
                    if (name.empty()) continue;
                    int count = 0;
                    for (const auto &b : kProtocolsTable) count += static_cast<int>(std::ranges::count(b.*column, name));
                    if (count != 1) return false;
                }
            }
            return true;
        }

        // A scheme with a single ':' in its "://" cannot prefix another one, so ForLink's row order never matters.
        constexpr bool protocolsSchemesWellFormed() {
            for (const auto &p : kProtocolsTable) {
                for (const auto scheme : p.importSchemes) {
                    if (!scheme.empty() && (!scheme.ends_with("://") || scheme.find(':') != scheme.size() - 3)) return false;
                }
            }
            return true;
        }

        constexpr bool protocolsFilterRanksValid() {
            int listed = 0;
            for (const auto &p : kProtocolsTable) {
                if ((p.filterRank > 0) != (p.filterLabel != nullptr) || p.filterRank < 0) return false;
                if (p.filterRank > 0) ++listed;
            }
            for (int rank = 1; rank <= listed; ++rank) {
                if (std::ranges::count(kProtocolsTable, rank, &ProtocolInfo::filterRank) != 1) return false;
            }
            for (const auto &p : kProtocolsTable) {
                if (p.filterAs.empty()) continue;
                if (p.filterRank != 0) return false;
                const bool listedTarget = std::ranges::any_of(kProtocolsTable, [&p](const ProtocolInfo &q) {
                    return q.id == p.filterAs && q.filterRank > 0;
                });
                if (!listedTarget) return false;
            }
            return true;
        }

        constexpr bool protocolsFlagsConsistent() {
            for (const auto &p : kProtocolsTable) {
                if (p.has(NoScan) && !p.has(Serverless)) return false;
                if (p.has(DnsTunnel) && !(p.has(Serverless) && p.has(NoScan))) return false;
            }
            return true;
        }

        constexpr bool protocolsRefusalsConsistent() {
            const auto unused = [](const auto &names) {
                return std::ranges::all_of(names, [](std::string_view name) { return name.empty(); });
            };
            for (const auto &p : kProtocolsTable) {
                const bool localOnly = p.has(Composite) || p.has(RunsProgram);
                if ((p.foreignRefusal != nullptr) != localOnly) return false;
                if (localOnly && !(unused(p.importSchemes) && unused(p.importSingBox) && unused(p.importClash))) return false;
            }
            return true;
        }

        constexpr bool protocolsRowHas(std::string_view id, Flag flag) {
            for (const auto &p : kProtocolsTable) {
                if (p.id == id) return p.has(flag);
            }
            return false;
        }

        static_assert(protocolsIdsUnique(), "ids and aliases must be non-empty and unique");
        static_assert(protocolsSchemesWellFormed(), "a scheme ends in \"://\" and has no other ':'");
        static_assert(protocolsNamesUnique(&ProtocolInfo::importSchemes), "a link scheme maps to one row");
        static_assert(protocolsNamesUnique(&ProtocolInfo::importSingBox), "a sing-box type maps to one row");
        static_assert(protocolsNamesUnique(&ProtocolInfo::importClash), "a Clash type maps to one row");
        static_assert(protocolsFilterRanksValid(), "filterRank runs 1..N, labelled; filterAs names a listed row");
        static_assert(protocolsFlagsConsistent(), "NoScan needs Serverless; DnsTunnel needs Serverless and NoScan");
        static_assert(protocolsRefusalsConsistent(), "exactly Composite and RunsProgram rows are refused and never imported");
        static_assert(protocolsRowHas("dnstt", DnsTunnel) && protocolsRowHas("masterdns", DnsTunnel), "the Go scanner's protocol names");
    } // namespace

    std::span<const ProtocolInfo> All() {
        return kProtocolsTable;
    }

    // starts_with("") and "" == "" hold, so every lookup skips empty slots.
    const ProtocolInfo *Find(QStringView type) {
        for (const auto &p : kProtocolsTable) {
            if (type == protocolsLatin1(p.id) || (!p.alias.empty() && type == protocolsLatin1(p.alias))) return &p;
        }
        return nullptr;
    }

    bool Is(QStringView type, Flag flag) {
        const auto *protocol = Find(type);
        return protocol != nullptr && protocol->has(flag);
    }

    QStringList IdsWith(Flag flag) {
        QStringList ids;
        for (const auto &p : kProtocolsTable) {
            if (p.has(flag)) ids << QString(p.Id());
        }
        return ids;
    }

    QString Canonical(const QString &type) {
        const auto *protocol = Find(type);
        return protocol != nullptr && type != protocol->Id() ? QString(protocol->Id()) : type;
    }

    const ProtocolInfo *ForLink(std::string_view line) {
        for (const auto &p : kProtocolsTable) {
            for (const auto scheme : p.importSchemes) {
                if (!scheme.empty() && line.starts_with(scheme)) return &p;
            }
        }
        return nullptr;
    }

    const ProtocolInfo *ForSingBox(QStringView type) {
        for (const auto &p : kProtocolsTable) {
            for (const auto name : p.importSingBox) {
                if (!name.empty() && type == protocolsLatin1(name)) return &p;
            }
        }
        return nullptr;
    }

    const ProtocolInfo *ForClash(std::string_view type) {
        for (const auto &p : kProtocolsTable) {
            for (const auto name : p.importClash) {
                if (!name.empty() && type == name) return &p;
            }
        }
        return nullptr;
    }
} // namespace Configs::Protocols

namespace Configs
{
    outbound* NewOutboundByType(const QString& type)
    {
        if (const auto* protocol = Protocols::Find(type)) return protocol->create();
        auto* ob = new outbound();
        ob->invalid = true;
        return ob;
    }

    QString RefusedForeignType(const QString& type)
    {
        const auto* protocol = Protocols::Find(type);
        if (protocol == nullptr || protocol->foreignRefusal == nullptr) return {};
        return QCoreApplication::translate("Validation", protocol->foreignRefusal);
    }
}
