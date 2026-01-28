#pragma once
#include <QString>
#include <QStringList>

namespace Configs {
    namespace DomainMatcher {
        enum DomainMatcher {
            DEFAULT,
            MPH,
        };
    }

    namespace DomainStrategy {
        inline QStringList DomainStrategy = {"", "ipv4_only", "ipv6_only", "prefer_ipv4", "prefer_ipv6"};
    }

    namespace SingboxOptions {
        inline QStringList SniffProtocols = {"http", "tls", "quic", "stun", "dns", "bittorrent", "dtls", "ssh", "rdp"};
        inline QStringList ActionTypes = {"route", "reject", "hijack-dns", "route-options", "sniff", "resolve"};
        inline QStringList rejectMethods = {"default", "drop"};
    }

    namespace SniffingMode {
        enum SniffingMode {
            DISABLE,
            FOR_ROUTING,
            FOR_DESTINATION,
        };
    }

    namespace CoreType {
        enum CoreType {
            SING_BOX,
        };
    }

    namespace Information {
        inline QString HijackInfo = "Listens on the given addr:port (on Windows, port is always 53) and redirects the requests to the DNS module. Domains that match the rules will have their requests hijacked and the A and AAAA queries will be responded with the Inet4 response and Inet6 response respectively.\nThe Redirect settings sets up an inbound that listens on the given addr:port, sniffs the destination if possible and redirects the requests to their true destination.\nThe use case of these settings is apps that do not respect the system proxy for resolving their DNS requests (one such example is discord), You can hijack their DNS requests to 127.0.0.1 and then route them through the Throne tunnel. The same effect could be achieved using Tun mode, but one may not want to tunnel the whole device (For example when Gaming), this is where DNS hijack can transparently handle things.\n\nCurrently you can Automatically set the System DNS in windows.";
        inline QString SimpleRuleInfo = "You can add rules with the following format:\ndomain:<your-domain>\nsuffix:<your-domain-suffix>\nkeyword:<your-domain-keyword>\nregex:<your-domain-keyword>\nruleset:<ruleset-name> or ruleset:<remote-ruleset-URL>\nip:<ip-cidr>\nprocessName:<process name>\nprocessPath:<process path>\nRules are validated on tab change or when pressing ok.\nImportant: On saving the rules, the previous rules are discarded, meaning\nany changes made to the generated rules in the Advanced tab will be lost.";
        inline QString CustomIconManual = "To choose custom icons, you need to choose png images with an equal width and height (eg 512*512). Their names should be of \n(Dns.png, Off.png, Proxy.png, Proxy-Dns.png, Throne.png, Tun.png) So that each will be used in the appropriate state of the app. \nYou can provide a subset of the said images and only the corresponding states will be using them.\nIt is suggested that each image's size be less than 100KB.";
        inline QStringList iconNames = {"Dns.png", "Off.png", "Proxy.png", "Proxy-Dns.png", "Throne.png", "Tun.png"};
    }

    namespace TestConfig
    {
        enum SpeedTestMode
        {
            FULL,
            DL,
            UL,
            SIMPLEDL,
            COUNTRY,
        };
    }

    namespace Mirrors
    {
        enum Mirrors
        {
            GITHUB,
            CLOUDFLARE,
            GCORE,
            QUANTIL,
            FASTLY,
            CDN,
        };
    }

    namespace VPNImplementation {
        inline QStringList VPNImplementation = {"system", "gvisor", "mixed"};
    }

    namespace Xray {
        inline QStringList XrayLogLevels = {"debug", "info", "warning", "error", "none"};
    }
} // namespace Configs
