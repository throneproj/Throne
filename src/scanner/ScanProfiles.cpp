#include "include/scanner/ScanProfiles.h"

#include <QObject>

#include "include/configs/EndpointOverride.h"
#include "include/configs/common/DnsTunnel.h"
#include "include/configs/common/Protocols.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include "include/scanner/IpListParse.h"
#include "include/scanner/WarpPresets.h"

namespace Scanner {
    namespace {
        // Cloudflare's WARP peer key, the same for every WARP account.
        constexpr auto kScanProfilesWarpPeerKey = "bmXOC+F1FxEMF9dyiK2H5/1SUtzH0JuVo51h2wPfgyo=";
        constexpr auto kScanProfilesWarpHostSuffix = ".cloudflareclient.com";

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

        QString scanProfilesNotWarp() {
            return QObject::tr("A WARP scan needs a WARP profile: WireGuard, AmneziaWG or MASQUE");
        }
    } // namespace

    QString DnsTunnelProtocol(const Configs::Profile &profile) {
        if (profile.outbound == nullptr || !Configs::IsDnsTunnelProfile(profile)) return {};
        if (Configs::Protocols::Is(profile.type, Configs::Protocols::DnsTunnel)) return profile.type;
        return QStringLiteral("xdns");
    }

    bool IsDnsTunnelBase(const Configs::Profile &profile) {
        return !DnsTunnelProtocol(profile).isEmpty();
    }

    bool IsWarpPeerKey(const QString &publicKey) {
        return publicKey.trimmed() == QLatin1String(kScanProfilesWarpPeerKey);
    }

    bool IsWarpScanProfile(const Configs::Profile &profile) {
        if (profile.outbound == nullptr) return false;
        if (profile.Masque() != nullptr) return true;
        const auto *wireguard = profile.Wireguard();
        if (wireguard == nullptr || wireguard->peer == nullptr) return false;
        if (IsWarpPeerKey(wireguard->peer->public_key)) return true;
        return wireguard->peer->address.trimmed().endsWith(QLatin1String(kScanProfilesWarpHostSuffix), Qt::CaseInsensitive);
    }

    void ApplyWarpJunk(Configs::wireguard &outbound) {
        outbound.enable_amnezia = true;
        outbound.jc = WarpPresets::kJunkCount;
        outbound.jmin = WarpPresets::kJunkMin;
        outbound.jmax = WarpPresets::kJunkMax;
        // Leftover AmneziaWG fields would change the wire format Cloudflare expects.
        outbound.s1 = outbound.s2 = outbound.s3 = outbound.s4 = 0;
        for (auto *field : {&outbound.h1, &outbound.h2, &outbound.h3, &outbound.h4, &outbound.i1, &outbound.i2, &outbound.i3,
                            &outbound.i4, &outbound.i5, &outbound.header_protection_key, &outbound.content_padding_addition,
                            &outbound.rekey_after_time, &outbound.rekey_timeout, &outbound.reject_after_time,
                            &outbound.keepalive_timeout, &outbound.max_handshake_attempts})
            field->clear();
        outbound.random_trailers = false;
        outbound.disable_cookies = false;
    }

    bool ApplyWarpEndpoint(Configs::Profile &profile, const Configs::IpListEntry &entry, bool junk, bool http2) {
        if (!IsWarpScanProfile(profile)) return false;
        const auto canonical = NormalizeCidr(entry.cidr.trimmed());
        if (canonical.isEmpty() || !IsHostCidr(canonical)) return false;
        auto &out = *profile.outbound;
        auto previous = out.GetAddress().trimmed();
        UnwrapIPV6Host(previous);
        // The run probed with the old host name as SNI, as CloneProfileWithServer keeps it.
        if (!previous.isEmpty() && !IsIpAddress(previous) && out.HasTLS()) {
            if (const auto tls = out.GetTLS(); tls != nullptr && tls->server_name.isEmpty()) tls->server_name = previous;
        }
        out.SetAddress(canonical.section(QLatin1Char('/'), 0, 0));
        if (entry.port > 0 && entry.port <= 65535) out.SetPort(entry.port);
        if (auto *wireguard = profile.Wireguard(); wireguard != nullptr && junk && !wireguard->enable_amnezia) ApplyWarpJunk(*wireguard);
        if (auto *masque = profile.Masque(); masque != nullptr && http2) {
            masque->http_version = 2;
            masque->disable_version_fallback = false;
        }
        if (!Configs::ResolveEndpointSource(Configs::EffectiveEndpointSource(profile)).address.isEmpty())
            profile.endpoint.mode = Configs::EndpointSource::Mode::Own;
        return true;
    }

    QString WarpCarrier(const Configs::Profile &profile) {
        if (!IsWarpScanProfile(profile)) return {};
        return profile.Masque() != nullptr ? QStringLiteral("masque") : QStringLiteral("wireguard");
    }

    QJsonObject WarpProbeEndpoint(const Configs::Profile &profile, QString *error) {
        if (!IsWarpScanProfile(profile)) {
            scanProfilesSetError(error, scanProfilesNotWarp());
            return {};
        }
        const auto built = profile.outbound->Build();
        if (!built.error.isEmpty()) {
            scanProfilesSetError(error, built.error);
            return {};
        }
        if (built.object.isEmpty()) scanProfilesSetError(error, QObject::tr("The WARP profile could not be built"));
        return built.object;
    }

    std::shared_ptr<Configs::Profile> ResolveScanBase(const Configs::IpScan &scan, QString *error) {
        const bool warp = scan.kind == Configs::IpScan::Kind::Warp;
        const int profileId = warp ? scan.config.warp.profileId : scan.config.resolver.profileId;
        const auto live = profileId >= 0 ? Configs::dataManager->profilesRepo->GetProfile(profileId) : nullptr;
        if (live == nullptr || live->outbound == nullptr) {
            scanProfilesSetError(error, warp ? QObject::tr("Choose the WARP profile to scan with")
                                             : QObject::tr("Choose the DNS tunnel profile to find resolvers for"));
            return nullptr;
        }
        if (warp && !IsWarpScanProfile(*live)) {
            scanProfilesSetError(error, scanProfilesNotWarp());
            return nullptr;
        }
        if (!warp && !IsDnsTunnelBase(*live)) {
            scanProfilesSetError(error, QObject::tr("Only DNSTT, MasterDNS and XDNS profiles can be used to find resolvers"));
            return nullptr;
        }
        auto clone = scanProfilesClone(live);
        if (clone == nullptr) scanProfilesSetError(error, QObject::tr("The profile could not be copied for the scan"));
        return clone;
    }

    std::shared_ptr<Configs::Profile> CloneWarpScanBase(const std::shared_ptr<Configs::Profile> &profile, QString *error) {
        if (profile == nullptr || !IsWarpScanProfile(*profile)) {
            scanProfilesSetError(error, scanProfilesNotWarp());
            return nullptr;
        }
        auto clone = scanProfilesClone(profile);
        if (clone == nullptr) scanProfilesSetError(error, QObject::tr("The profile could not be copied for the scan"));
        return clone;
    }
} // namespace Scanner
