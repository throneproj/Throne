#pragma once

#include <QJsonObject>
#include <QString>
#include <memory>

#include "include/configs/sub/warp.h"
#include "include/database/entities/IpScan.h"
#include "include/database/entities/Profile.h"

namespace Scanner {
    [[nodiscard]] bool IsScanBaseType(const QString &type);

    // dnstt | masterdns | xdns for a profile a DNS resolver scan can probe for; "" otherwise.
    [[nodiscard]] QString DnsTunnelProtocol(const Configs::Profile &profile);

    [[nodiscard]] bool IsDnsTunnelBase(const Configs::Profile &profile);

    // An XDNS profile dials its resolvers, never its server address, so only a resolver scan can use it.
    [[nodiscard]] bool IsGenericScanBase(const Configs::Profile &profile);

    // The probe levels of a protocol, cheapest first; the default is ResolverScanDefaultLevel.
    [[nodiscard]] QStringList ResolverScanLevels(const QString &protocol);

    [[nodiscard]] QString ResolverScanDefaultLevel(const QString &protocol);

    [[nodiscard]] int ResolverScanDefaultTimeoutMs(const QString &protocol, const QString &level);

    // Always a deep copy, never the live repo object.
    std::shared_ptr<Configs::Profile> ResolveScanBase(const Configs::IpScan &scan, QString *error);

    std::shared_ptr<Configs::Profile> BuildWarpTemplate(const Configs::ScanWarpOptions &opt, QString *error);

    // transport: "wireguard" | "masque".
    QJsonObject WarpIdentityToBeanJson(const QString &transport, const Configs_network::WarpIdentity &identity);

    [[nodiscard]] QString WarpTransportOf(const QString &mode);
} // namespace Scanner
