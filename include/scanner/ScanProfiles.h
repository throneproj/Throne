#pragma once

#include <QJsonObject>
#include <QString>
#include <memory>

#include "include/database/entities/IpList.h"
#include "include/database/entities/IpScan.h"
#include "include/database/entities/Profile.h"

namespace Scanner {
    // dnstt | masterdns | xdns for a profile a DNS resolver scan can probe for; "" otherwise.
    [[nodiscard]] QString DnsTunnelProtocol(const Configs::Profile &profile);

    [[nodiscard]] bool IsDnsTunnelBase(const Configs::Profile &profile);

    // True for the public key every Cloudflare WARP WireGuard peer uses.
    [[nodiscard]] bool IsWarpPeerKey(const QString &publicKey);

    // A WireGuard or AmneziaWG profile whose peer is Cloudflare WARP, or a MASQUE profile.
    [[nodiscard]] bool IsWarpScanProfile(const Configs::Profile &profile);

    // The AmneziaWG junk WARP accepts (WarpPresets::kJunk*), with the rest of the AmneziaWG set cleared.
    void ApplyWarpJunk(Configs::wireguard &outbound);

    // Points a WARP profile at entry with the junk (unless it has AmneziaWG) or HTTP/2 the run needed; an endpoint source
    // that would replace the address becomes Own. False for a non-WARP profile or an entry that is not one address.
    bool ApplyWarpEndpoint(Configs::Profile &profile, const Configs::IpListEntry &entry, bool junk, bool http2);

    // "wireguard" | "masque" for a WARP scan profile; "" otherwise.
    [[nodiscard]] QString WarpCarrier(const Configs::Profile &profile);

    // The profile's own sing-box endpoint object for the core's WARP probe; nothing is overridden.
    [[nodiscard]] QJsonObject WarpProbeEndpoint(const Configs::Profile &profile, QString *error);

    // Always a deep copy, never the live repo object.
    std::shared_ptr<Configs::Profile> ResolveScanBase(const Configs::IpScan &scan, QString *error);

    // A deep copy of a WARP profile, an editor's unsaved draft too; nullptr with *error set otherwise.
    std::shared_ptr<Configs::Profile> CloneWarpScanBase(const std::shared_ptr<Configs::Profile> &profile, QString *error);
} // namespace Scanner
