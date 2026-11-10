#include "include/scanner/ScanProfiles.h"

#include <QObject>

#include "include/configs/common/DnsTunnel.h"
#include "include/configs/generate.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"

namespace Scanner {
    namespace {
        constexpr auto kScanProfilesMasqueSni = "consumer-masque.cloudflareclient.com";

        void scanProfilesSetError(QString *error, const QString &text) {
            if (error != nullptr) *error = text;
        }

        std::shared_ptr<Configs::Profile> scanProfilesClone(const std::shared_ptr<Configs::Profile> &source) {
            auto clone = Configs::ProfilesRepo::NewProfile(source->type);
            if (clone->outbound == nullptr || clone->outbound->invalid) return nullptr;
            if (!clone->outbound->ParseFromJson(source->outbound->ExportToJson())) return nullptr;
            clone->id = -1;
            clone->gid = source->gid;
            clone->name = source->name;
            clone->outbound->profile_id = source->id;
            return clone;
        }

        QStringList scanProfilesAddresses(const Configs_network::WarpIdentity &identity) {
            QStringList addresses;
            if (!identity.ipv4.isEmpty()) addresses << identity.ipv4 + "/32";
            if (!identity.ipv6.isEmpty()) addresses << identity.ipv6 + "/128";
            return addresses;
        }

        std::shared_ptr<Configs::Profile> scanProfilesBuiltin(const QString &transport, QString *error) {
            const auto &settings = *Configs::dataManager->settingsRepo;
            auto profile = Configs::ProfilesRepo::NewProfile(transport);
            QString host;
            int port = 0;
            if (transport == QLatin1String("masque")) {
                if (settings.warp_masque_private_key.isEmpty() || settings.warp_masque_peer_public_key.isEmpty() ||
                    settings.warp_masque_ifc_addrs.isEmpty()) {
                    scanProfilesSetError(error, QObject::tr("The built-in WARP MASQUE identity has not been generated yet (Routing Settings)"));
                    return nullptr;
                }
                auto *masque = profile->Masque();
                masque->private_key = settings.warp_masque_private_key;
                masque->peer_public_key = settings.warp_masque_peer_public_key;
                masque->address = settings.warp_masque_ifc_addrs;
                Configs::SplitWarpEndpoint(settings.warp_masque_ep, 443, host, port);
                masque->server = host;
                masque->server_port = port;
                masque->tls->server_name = settings.warp_masque_sni;
                return profile;
            }
            if (settings.warp_private_key.isEmpty() || settings.warp_public_key.isEmpty() || settings.warp_ifc_addrs.isEmpty()) {
                scanProfilesSetError(error, QObject::tr("The built-in WARP WireGuard identity has not been generated yet (Routing Settings)"));
                return nullptr;
            }
            auto *wireguard = profile->Wireguard();
            wireguard->private_key = settings.warp_private_key;
            wireguard->address = settings.warp_ifc_addrs;
            wireguard->peer->public_key = settings.warp_public_key;
            wireguard->peer->reserved = QStringList2QListInt(settings.warp_reserved);
            wireguard->peer->persistent_keepalive = "10";
            Configs::SplitWarpEndpoint(settings.warp_ep, 2408, host, port);
            wireguard->SetAddress(host);
            wireguard->SetPort(port);
            return profile;
        }
    } // namespace

    bool IsScanBaseType(const QString &type) {
        return !type.isEmpty() && type != QLatin1String("custom") && type != QLatin1String("extracore") &&
               type != QLatin1String("tailscale") && type != QLatin1String("autoselector") && type != QLatin1String("direct") &&
               type != QLatin1String("dnstt") && type != QLatin1String("masterdns");
    }

    QString DnsTunnelProtocol(const Configs::Profile &profile) {
        if (profile.outbound == nullptr || !Configs::IsDnsTunnelProfile(profile)) return {};
        if (profile.type == QLatin1String("dnstt") || profile.type == QLatin1String("masterdns")) return profile.type;
        return QStringLiteral("xdns");
    }

    bool IsDnsTunnelBase(const Configs::Profile &profile) {
        return !DnsTunnelProtocol(profile).isEmpty();
    }

    bool IsGenericScanBase(const Configs::Profile &profile) {
        return IsScanBaseType(profile.type) && !IsDnsTunnelBase(profile);
    }

    QStringList ResolverScanLevels(const QString &protocol) {
        if (protocol == QLatin1String("dnstt")) return {QStringLiteral("poll"), QStringLiteral("qname"), QStringLiteral("handshake")};
        if (protocol == QLatin1String("masterdns")) return {QStringLiteral("quick"), QStringLiteral("full")};
        if (protocol == QLatin1String("xdns")) return {QStringLiteral("poll"), QStringLiteral("qname")};
        return {};
    }

    QString ResolverScanDefaultLevel(const QString &protocol) {
        return protocol == QLatin1String("masterdns") ? QStringLiteral("quick") : protocol.isEmpty() ? QString() : QStringLiteral("qname");
    }

    int ResolverScanDefaultTimeoutMs(const QString &protocol, const QString &level) {
        // A silent exchange costs 2 x 5 s (dnstt, xdns) or 2 x 2 s per domain (masterdns); the searches add several.
        if (protocol == QLatin1String("masterdns")) return level == QLatin1String("full") ? 90000 : 20000;
        if (level == QLatin1String("poll")) return 15000;
        if (level == QLatin1String("handshake")) return 120000;
        return protocol == QLatin1String("xdns") ? 60000 : 90000;
    }

    QString WarpTransportOf(const QString &mode) {
        return mode == QLatin1String("masque") ? QStringLiteral("masque") : QStringLiteral("wireguard");
    }

    std::shared_ptr<Configs::Profile> ResolveScanBase(const Configs::IpScan &scan, QString *error) {
        if (scan.kind == Configs::IpScan::Kind::Warp) return BuildWarpTemplate(scan.config.warp, error);
        const bool resolver = scan.kind == Configs::IpScan::Kind::Resolver;
        const int profileId = resolver ? scan.config.resolver.profileId : scan.config.config.profileId;
        const auto live = profileId >= 0 ? Configs::dataManager->profilesRepo->GetProfile(profileId) : nullptr;
        if (live == nullptr || live->outbound == nullptr) {
            scanProfilesSetError(error, resolver ? QObject::tr("Choose the DNS tunnel profile to find resolvers for")
                                                 : QObject::tr("Choose a profile for the config test"));
            return nullptr;
        }
        if (resolver && !IsDnsTunnelBase(*live)) {
            scanProfilesSetError(error, QObject::tr("Only DNSTT, MasterDNS and XDNS profiles can be used to find resolvers"));
            return nullptr;
        }
        if (!resolver && !IsGenericScanBase(*live)) {
            scanProfilesSetError(error, QObject::tr("This type of profile cannot be pointed at scanned addresses"));
            return nullptr;
        }
        auto clone = scanProfilesClone(live);
        if (clone == nullptr) scanProfilesSetError(error, QObject::tr("The profile for the config test could not be copied"));
        return clone;
    }

    std::shared_ptr<Configs::Profile> BuildWarpTemplate(const Configs::ScanWarpOptions &opt, QString *error) {
        const auto transport = WarpTransportOf(opt.mode);
        std::shared_ptr<Configs::Profile> profile;
        if (opt.identity == QLatin1String("profile")) {
            const auto source = opt.profileId >= 0 ? Configs::dataManager->profilesRepo->GetProfile(opt.profileId) : nullptr;
            if (source == nullptr || source->outbound == nullptr) {
                scanProfilesSetError(error, QObject::tr("Choose the WARP profile to take the identity from"));
                return nullptr;
            }
            if (source->type != transport) {
                scanProfilesSetError(error, transport == QLatin1String("masque")
                                                ? QObject::tr("This mode needs a MASQUE profile")
                                                : QObject::tr("This mode needs a WireGuard profile"));
                return nullptr;
            }
            profile = scanProfilesClone(source);
        } else if (opt.identity == QLatin1String("builtin")) {
            profile = scanProfilesBuiltin(transport, error);
            if (profile == nullptr) return nullptr;
        } else {
            if (opt.generatedIdentity.isEmpty() || opt.generatedMode != transport) {
                scanProfilesSetError(error, QObject::tr("Register a new identity for this mode"));
                return nullptr;
            }
            profile = Configs::ProfilesRepo::NewProfile(transport);
            if (profile->outbound == nullptr || !profile->outbound->ParseFromJson(opt.generatedIdentity)) profile = nullptr;
        }
        if (profile == nullptr || profile->outbound == nullptr) {
            scanProfilesSetError(error, QObject::tr("The WARP identity could not be read; register a new one"));
            return nullptr;
        }
        profile->outbound->dialFields->quic_opener = opt.quicOpener;

        if (auto *wireguard = profile->Wireguard(); wireguard != nullptr) {
            if (opt.mtu > 0) wireguard->mtu = opt.mtu;
            wireguard->enable_amnezia = opt.mode == QLatin1String("amneziawg");
            wireguard->jc = opt.jc;
            wireguard->jmin = opt.jmin;
            wireguard->jmax = opt.jmax;
            wireguard->i1 = opt.i1;
            wireguard->i2 = opt.i2;
            wireguard->i3 = opt.i3;
            wireguard->i4 = opt.i4;
            wireguard->i5 = opt.i5;
            // Cloudflare speaks vanilla WireGuard: anything that changes the wire format fails every target.
            wireguard->s1 = wireguard->s2 = wireguard->s3 = wireguard->s4 = 0;
            wireguard->h1.clear();
            wireguard->h2.clear();
            wireguard->h3.clear();
            wireguard->h4.clear();
            wireguard->header_protection_key.clear();
            wireguard->content_padding_addition.clear();
            wireguard->random_trailers = false;
        }
        if (auto *masque = profile->Masque(); masque != nullptr) {
            if (opt.mtu > 0) masque->mtu = opt.mtu;
            masque->tls->enabled = true;
            if (!opt.sni.trimmed().isEmpty()) masque->tls->server_name = opt.sni.trimmed();
            else if (masque->tls->server_name.isEmpty()) masque->tls->server_name = kScanProfilesMasqueSni;
            switch (opt.httpMode) {
                case 1:
                    masque->http_version = 3;
                    masque->disable_version_fallback = true;
                    break;
                case 2:
                    masque->http_version = 2;
                    masque->disable_version_fallback = false;
                    break;
                default:
                    masque->http_version = 0;
                    masque->disable_version_fallback = false;
                    break;
            }
        }
        // Every target replaces the endpoint; a placeholder only keeps the template valid for CheckConfig.
        if (profile->outbound->GetAddress().trimmed().isEmpty()) {
            const bool masque = profile->Masque() != nullptr;
            profile->outbound->SetAddress(masque ? QStringLiteral("162.159.198.2") : QStringLiteral("162.159.192.1"));
            if (profile->outbound->GetPort().toInt() <= 0) profile->outbound->SetPort(masque ? 443 : 2408);
        }
        profile->name = QStringLiteral("WARP scan");
        profile->outbound->name = profile->name;
        profile->id = -1;
        return profile;
    }

    QJsonObject WarpIdentityToBeanJson(const QString &transport, const Configs_network::WarpIdentity &identity) {
        QString host;
        int port = 0;
        if (transport == QLatin1String("masque")) {
            auto profile = Configs::ProfilesRepo::NewProfile(QStringLiteral("masque"));
            auto *masque = profile->Masque();
            masque->private_key = identity.privateKey;
            masque->peer_public_key = identity.peerPublicKey;
            masque->address = scanProfilesAddresses(identity);
            masque->mtu = 1280;
            Configs::SplitWarpEndpoint(identity.endpoint, 443, host, port);
            masque->server = host;
            masque->server_port = port;
            return masque->ExportToJson();
        }
        auto profile = Configs::ProfilesRepo::NewProfile(QStringLiteral("wireguard"));
        auto *wireguard = profile->Wireguard();
        wireguard->private_key = identity.privateKey;
        wireguard->address = scanProfilesAddresses(identity);
        wireguard->mtu = 1280;
        wireguard->peer->public_key = identity.peerPublicKey;
        wireguard->peer->reserved = identity.reserved;
        wireguard->peer->persistent_keepalive = "10";
        Configs::SplitWarpEndpoint(identity.endpoint, 2408, host, port);
        wireguard->SetAddress(host);
        wireguard->SetPort(port);
        return wireguard->ExportToJson();
    }
} // namespace Scanner
