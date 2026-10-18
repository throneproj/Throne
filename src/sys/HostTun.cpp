#include "include/sys/HostTun.hpp"

#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTcpServer>

namespace Sys::HostTun {
    namespace {
        // The name of a live interface called throne-tun, or empty.
        QString hostTunNamedInterface() {
#if defined(Q_OS_LINUX)
            // A non-persistent tun vanishes with the fd that created it, so its sysfs entry means a live owner.
            if (QFileInfo::exists(QStringLiteral("/sys/class/net/") + QLatin1String(kTunName))) return QLatin1String(kTunName);
#elif defined(Q_OS_WIN)
            for (const auto &netif : QNetworkInterface::allInterfaces()) {
                const auto flags = netif.flags();
                // A crash leaves "throne-tun N" adapters behind, but they are down.
                if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)) continue;
                if (netif.humanReadableName().startsWith(QLatin1String(kTunName), Qt::CaseInsensitive) ||
                    netif.name().startsWith(QLatin1String(kTunName), Qt::CaseInsensitive)) {
                    return netif.humanReadableName();
                }
            }
#endif
            return {};
        }

        // The Up, non-loopback interface that already carries address, or an invalid one.
        QNetworkInterface hostTunAddressOwner(const QHostAddress &address) {
            for (const auto &netif : QNetworkInterface::allInterfaces()) {
                const auto flags = netif.flags();
                if (!flags.testFlag(QNetworkInterface::IsUp) || flags.testFlag(QNetworkInterface::IsLoopBack)) continue;
                for (const auto &entry : netif.addressEntries()) {
                    if (entry.ip().isEqual(address, QHostAddress::ConvertV4MappedToIPv4)) return netif;
                }
            }
            return {};
        }

        // Bridges, veth and NICs report Ethernet, tun/wg devices Unknown; Ieee80211 is an alias of Wifi.
        bool hostTunLanLike(const QNetworkInterface &netif) {
            const auto type = netif.type();
            return type == QNetworkInterface::Ethernet || type == QNetworkInterface::Wifi;
        }

        // Only "in use" counts: another error (a privileged port, no loopback) is not a second Tun owner.
        bool hostTunPortTaken(int port) {
            QTcpServer probe;
            if (probe.listen(QHostAddress(QHostAddress::LocalHost), static_cast<quint16>(port))) {
                probe.close();
                return false;
            }
            return probe.serverError() == QAbstractSocket::AddressInUseError;
        }
    } // namespace

    QString Conflict(const QString &tunIPv4Cidr, int dnsInPort) {
        if (const auto name = hostTunNamedInterface(); !name.isEmpty()) {
            return QStringLiteral("the network interface %1 already exists: another Throne (or another program) runs Tun on this "
                                  "computer")
                .arg(name);
        }
        const QHostAddress address(tunIPv4Cidr.section(QLatin1Char('/'), 0, 0).trimmed());
        if (address.protocol() == QAbstractSocket::IPv4Protocol) {
            if (const auto owner = hostTunAddressOwner(address); owner.isValid()) {
                // Still refused: the Tun's prefix route would take that interface's subnet over.
                if (hostTunLanLike(owner)) {
                    return QStringLiteral("the Tun address %1 is already used by the network interface %2 (a LAN, bridge or container "
                                          "network such as Docker's); set tun_ipv4_cidr to an unused range (thronectl settings set "
                                          "tun_ipv4_cidr=CIDR)")
                        .arg(address.toString(), owner.humanReadableName());
                }
                return QStringLiteral("the Tun address %1 is already on the network interface %2: another Throne or VPN runs on this "
                                      "computer; stop it or change tun_ipv4_cidr")
                    .arg(address.toString(), owner.humanReadableName());
            }
        }
        if (dnsInPort > 0 && dnsInPort <= 65535 && hostTunPortTaken(dnsInPort)) {
            return QStringLiteral("port %1 on 127.0.0.1 (the core's DNS server, dns_in_port) is in use (another Throne?); stop that "
                                  "program or change dns_in_port")
                .arg(dnsInPort);
        }
        return {};
    }
} // namespace Sys::HostTun
