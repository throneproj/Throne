#pragma once

#include <QString>

// One Tun-owning Throne per host: the Tun name, the dns-in port and the sing-tun tables are host-global.
namespace Sys::HostTun {
    inline constexpr char kTunName[] = "throne-tun";

    // Why starting Tun here would collide with another Tun owner or an interface holding the Tun address (the text says which
    // and names the API key to change: tun_ipv4_cidr, or dns_in_port for the port), plain English; empty when free. Meaningful only while this
    // process runs no Tun. Linux: /sys/class/net/throne-tun exists. Windows: an interface named throne-tun* that is Up and
    // Running (stale adapters are down). Every OS: an Up non-loopback interface carrying tunIPv4Cidr's host address.
    // dnsInPort > 0 also probes that 127.0.0.1:dnsInPort can be bound (pass 0 while a profile of ours runs). Main thread.
    QString Conflict(const QString &tunIPv4Cidr, int dnsInPort);
} // namespace Sys::HostTun
