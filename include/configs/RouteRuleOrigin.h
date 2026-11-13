#pragma once

#include <QList>
#include <QString>

namespace Configs
{
    enum class RouteRuleOriginKind {
        ProfileRule,
        RawRule,
        XrayBridge,
        ExtraCore,
        SystemDns,
        Sniff,
        TorOnion,
        Resolve,
        DnsHijack,
        DnsInboundReject,
        AdBlock,
        EndpointGate,
        L3Bridge,
        TunnelGate,
        BlockFinal,
    };

    // profileRule indexes RouteProfile::Rules, or a raw profile's rules array; name is the rule's name, or the aux tag for EndpointGate.
    struct RouteRuleOrigin {
        RouteRuleOriginKind kind = RouteRuleOriginKind::ProfileRule;
        int profileRule = -1;
        QString name;
    };
}
