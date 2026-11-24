#include "include/scanner/ScanProfiles.h"

#include <QObject>

#include "include/configs/common/DnsTunnel.h"
#include "include/configs/common/Protocols.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"

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

    bool IsWarpScanProfile(const Configs::Profile &profile) {
        if (profile.outbound == nullptr) return false;
        if (profile.Masque() != nullptr) return true;
        const auto *wireguard = profile.Wireguard();
        if (wireguard == nullptr || wireguard->peer == nullptr) return false;
        if (wireguard->peer->public_key.trimmed() == QLatin1String(kScanProfilesWarpPeerKey)) return true;
        return wireguard->peer->address.trimmed().endsWith(QLatin1String(kScanProfilesWarpHostSuffix), Qt::CaseInsensitive);
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
} // namespace Scanner
