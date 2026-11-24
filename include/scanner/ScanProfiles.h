#pragma once

#include <QJsonObject>
#include <QString>
#include <memory>

#include "include/database/entities/IpScan.h"
#include "include/database/entities/Profile.h"

namespace Scanner {
    // dnstt | masterdns | xdns for a profile a DNS resolver scan can probe for; "" otherwise.
    [[nodiscard]] QString DnsTunnelProtocol(const Configs::Profile &profile);

    [[nodiscard]] bool IsDnsTunnelBase(const Configs::Profile &profile);

    // A WireGuard or AmneziaWG profile whose peer is Cloudflare WARP, or a MASQUE profile.
    [[nodiscard]] bool IsWarpScanProfile(const Configs::Profile &profile);

    // "wireguard" | "masque" for a WARP scan profile; "" otherwise.
    [[nodiscard]] QString WarpCarrier(const Configs::Profile &profile);

    // The profile's own sing-box endpoint object for the core's WARP probe; nothing is overridden.
    [[nodiscard]] QJsonObject WarpProbeEndpoint(const Configs::Profile &profile, QString *error);

    // Always a deep copy, never the live repo object.
    std::shared_ptr<Configs::Profile> ResolveScanBase(const Configs::IpScan &scan, QString *error);
} // namespace Scanner
