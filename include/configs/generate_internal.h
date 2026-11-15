#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <map>
#include <memory>
#include <optional>

#include "include/configs/generate.h"
#include "include/configs/common/ResolverSource.h"
#include "include/database/entities/Profile.h"
#include "include/database/entities/RouteProfile.h"
#include "include/global/Utils.hpp"

namespace Configs {
    namespace gen {

        // ------------------------------------------------------------- tags
        namespace tags {
            constexpr auto proxy = "proxy";
            constexpr auto direct = "direct";
            constexpr auto warpBypass = "warp-bypass";
            // Not bridgePrefix below, which names the unrelated sing-box <-> Xray socks bridges.
            constexpr auto l3Direct = "l3-direct";

            constexpr auto dnsRemote = "dns-remote";
            constexpr auto dnsDirect = "dns-direct";
            constexpr auto dnsLocal = "dns-local";
            constexpr auto dnsFake = "dns-fake";
            constexpr auto dnsTailscale = "dns-tailscale";
            constexpr auto dnsHosts = "dns-hosts";
            constexpr auto dnsVpnPrefix = "dns-vpn";
            constexpr auto dnsEchPrefix = "dns-ech";

            constexpr auto dnsIn = "dns-in";
            constexpr auto dnsSystem = "dns-system";
            constexpr auto mixedIn = "mixed-in";
            constexpr auto tunIn = "tun-in";
            constexpr auto xrayFullConfigIn = "throne-bridge";

            constexpr auto adblockRuleSet = "throne-adblocksingbox";
            constexpr auto privateRangesRuleSet = "throne-private-ranges";

            constexpr auto mainChainPrefix = "config";
            constexpr auto routeChainPrefix = "route";
            constexpr auto auxEndpointPrefix = "aux";
            constexpr auto poolChainPrefix = "pool";
            constexpr auto selectorUpstreamPrefix = "upstream";
            constexpr auto chainSelector = "auto-selector";
            constexpr auto testChainPrefix = "proxy";
            constexpr auto testXrayFullPrefix = "xrayfull";
            constexpr auto bridgePrefix = "bridge";
        }

        // -------------------------------------------------- prefixed selectors

        struct DomainSelectors {
            QJsonArray ruleSets;
            QJsonArray domains;
            QJsonArray suffixes;
            QJsonArray keywords;
            QJsonArray regexes;

            [[nodiscard]] bool hasInlineConditions() const {
                return !domains.isEmpty() || !suffixes.isEmpty() || !keywords.isEmpty() || !regexes.isEmpty();
            }
        };

        // ---------------------------------------------------------- build state

        struct DNSDeps {
            bool needDirectDnsRules = false;
            DomainSelectors direct;
            bool needProxyDnsRules = false;
            DomainSelectors proxy;
        };

        struct TunDeps {
            QJsonArray directIPSets;
            QJsonArray directIPCIDRs;
            QStringList bypassedPrivateRanges;
            bool privateRangesAsRuleSet = false;
        };

        struct RoutingDeps {
            QStringList neededRuleSets;
            std::map<int, QString> outboundMap;
            struct RouteOutboundGroup {
                QList<int> hopIDs;
                std::shared_ptr<Profile> chainWrapper;
                std::shared_ptr<Profile> profile;
            };
            QList<RouteOutboundGroup> routeOutboundGroups;
            struct AuxEndpointGroup {
                QList<int> hopIDs;
                QList<int> innerHopIndexes;
                std::shared_ptr<Profile> endpoint;
            };
            QList<AuxEndpointGroup> auxEndpointGroups;
            QList<QString> endpointRuleTags;
        };

        struct BuildPrerequisites {
            DNSDeps dns;
            TunDeps tun;
            RoutingDeps routing;
        };

        struct BridgeEndpoint {
            int port = -1;
            QString auth;
            QString host = "127.0.0.1";
        };

        struct Bridge {
            enum class Kind { SingToXray, XrayToSing, XrayFullConfig };
            Kind kind;
            BridgeEndpoint endpoint;
            // SingToXray: the Xray outbound it feeds; XrayToSing: the sing-box outbound it routes into; unused for XrayFullConfig.
            QString target;
        };

        // ByPort is the test build's: ConfigDump masks its bridge-<port> tags.
        enum class BridgeTagScheme { ByIngress, ByPort };

        class BridgeSet {
        public:
            void add(Bridge bridge) { items_.append(std::move(bridge)); }

            [[nodiscard]] QJsonArray singInbounds(BridgeTagScheme scheme) const;
            [[nodiscard]] QJsonArray singRouteRules(BridgeTagScheme scheme) const;
            [[nodiscard]] QJsonArray xrayInbounds() const;
            [[nodiscard]] QJsonArray xrayRouteRules() const;

        private:
            QList<Bridge> items_;
        };

        enum class BuildPurpose { Run, Test };
        // What a test build leaves out or does differently; the bean-facing environment (OTP, VPN challenge) is not here.
        struct BuildPolicy { bool listeners, controlPlane, fullRouting, fullDns, dnsAsIfTun, reportBadDns, ntpViaProxy; };

        constexpr BuildPolicy PolicyFor(BuildPurpose purpose) {
            const bool run = purpose == BuildPurpose::Run;
            return {run, run, run, run, !run, run, run};
        }

        static_assert(PolicyFor(BuildPurpose::Run).listeners && !PolicyFor(BuildPurpose::Test).listeners);
        static_assert(PolicyFor(BuildPurpose::Run).controlPlane && !PolicyFor(BuildPurpose::Test).controlPlane);
        static_assert(PolicyFor(BuildPurpose::Run).fullRouting && !PolicyFor(BuildPurpose::Test).fullRouting);
        static_assert(PolicyFor(BuildPurpose::Run).fullDns && !PolicyFor(BuildPurpose::Test).fullDns);
        static_assert(!PolicyFor(BuildPurpose::Run).dnsAsIfTun && PolicyFor(BuildPurpose::Test).dnsAsIfTun);
        static_assert(PolicyFor(BuildPurpose::Run).reportBadDns && !PolicyFor(BuildPurpose::Test).reportBadDns);
        static_assert(PolicyFor(BuildPurpose::Run).ntpViaProxy && !PolicyFor(BuildPurpose::Test).ntpViaProxy);

        // What a chain's hops ask of DNS and route; merge() keeps the order the hops wrote them in.
        struct HopNeeds {
            // Names whose HTTPS record a hop's ECH needs to fetch; they must not resolve over the proxy they unlock.
            QStringList echQueryNames;               // append-unique
            QMap<QString, QString> echResolvers;     // first insert wins
            // DNS tunnel domains; a loopback stub resolver forwarding them back through the Tun must not reach dns-remote.
            QStringList dnsTunnelDomains;            // append-unique
            // tag -> "openvpn" | "openconnect".
            QMap<QString, QString> vpnEndpointTags;  // insert, last wins
            // The main-profile tunnel set to Strict tunnel DNS; its resolvers take every remote query.
            QString vpnStrictTag;                    // last non-empty writer wins
            QList<QString> vpnGateTags;              // append, duplicates kept
            void merge(const HopNeeds &later);
        };

        struct BuildContext {
            BuildPolicy policy = PolicyFor(BuildPurpose::Run);
            bool tunEnabled = false;
            bool l3Bridge = false;
            bool isResolvedUsed = false;
            bool proxyUsesXray = false;
            std::shared_ptr<Profile> ent = std::make_shared<Profile>(nullptr, nullptr);
            // The routing profile this build uses (BuildSingBoxConfig only).
            int routeId = -1;
            // That routing profile's DNS; routeOwnsDns also overrides the global DNS object.
            RouteDns dns;
            bool routeOwnsDns = false;
            BuildPrerequisites prerequisites;
            osType os = getOS();

            QString error;
            QJsonArray outbounds;
            QJsonArray endpoints;
            QJsonArray xrayOutbounds;
            // The outbounds stage's accumulators; DNS and route take them as arguments.
            HopNeeds needs;
            QList<QString> vpnAuxTags;
            BridgeSet bridges;
            std::shared_ptr<BuildConfigResult> result = std::make_shared<BuildConfigResult>();
        };

        extern thread_local QSet<int> *buildProfileSink;
        extern thread_local const QHash<int, std::shared_ptr<Profile>> *buildProfileOverrides;

        struct GenerateEndpointEntry {
            std::shared_ptr<Profile> live;
            // nullptr: the live profile is built as it is.
            std::shared_ptr<Profile> clone;
        };

        struct GenerateEndpointCache {
            QHash<int, EndpointResolution> lists;
            QHash<int, GenerateEndpointEntry> profiles;
            QSet<int> loggedLists;
        };

        extern thread_local GenerateEndpointCache *generateEndpointCache;

        // Nested builds on one thread share the outermost cache.
        class GenerateEndpointScope {
        public:
            GenerateEndpointScope() : previous_(generateEndpointCache) {
                if (previous_ == nullptr) generateEndpointCache = &cache_;
            }

            ~GenerateEndpointScope() { generateEndpointCache = previous_; }

            GenerateEndpointScope(const GenerateEndpointScope &) = delete;
            GenerateEndpointScope &operator=(const GenerateEndpointScope &) = delete;

        private:
            GenerateEndpointCache cache_;
            GenerateEndpointCache *previous_;
            const ResolverSourceScope resolvers_;
        };

        enum class GenerateServerless { None, Missing, NoServer, Realm };

        struct ChainBuildRequest {
            QList<int> hopIDs;
            QString prefix;
            bool includeProxy = false;
            bool link = true;
            int startSuffix = 0;
            // Pre-probed bridge ports; -1 lets the chain probe its own.
            int singToXrayPort = -1;
            int xrayToSingPort = -1;
            int xrayFullConfigPort = -1;
            // Sibling configs from one subscription repeat these ports and would fail to bind.
            bool soleXrayInbound = false;
            bool warpWrap = false;
            bool auxiliary = false;
            QSet<QString> addressableTags;
            // The directly dialed hop detours here instead; an Xray one is bridged back into sing-box for it.
            QString tailDetour;
            bool tailResolvesViaDns = false;
        };

        struct ChainOutput {
            QString ingressTag;
            QJsonArray outbounds, endpoints, xrayOutbounds;
            QList<Bridge> bridges;
            // The user's Xray full config, when the chain exits through one.
            QJsonObject xrayFullConfig;
            std::optional<TrafficChainGroup> traffic;
            QMap<QString, int> vpnEndpointProfiles;
            HopNeeds needs;
            QString error;
        };

        void buildLogSection(BuildContext &ctx);
        void buildNTPSection(BuildContext &ctx);
        void buildCertificateSection(BuildContext &ctx);
        QSet<int> invalidProfileIDs(const QList<int> &ids);

        QString hopTag(const QString &prefix, int index);
        std::shared_ptr<Profile> generateEndpointLive(const std::shared_ptr<Profile> &ent);
        QString profileDisplayName(const std::shared_ptr<Profile> &ent);
        std::shared_ptr<Profile> getProfile(int id);
        bool isCustomFullConfig(const std::shared_ptr<Profile> &profile);
        bool isXrayFullConfig(const std::shared_ptr<Profile> &profile);
        bool usesXrayCore(const std::shared_ptr<Profile> &profile);
        bool torExit();
        bool exitWrapperOn();
        std::shared_ptr<Profile> getWarpProfile();
        std::shared_ptr<Profile> resolveExtraCoreProfile(const std::shared_ptr<Profile> &ent);
        QString scanCloneBlocker(const std::shared_ptr<Profile> &profile);
        std::shared_ptr<Profile> scanDeepClone(const std::shared_ptr<Profile> &base);

        std::shared_ptr<Profile> chainSelectorHop(const std::shared_ptr<Profile> &ent);
        void calculatePrerequisites(BuildContext &ctx);
        QList<int> unwrapChain(int entID);

        bool isCanonicalDecimal(const QString &text, int maxDigits);
        bool isCanonicalIPv4(const QString &text);
        bool isIPv6Literal(const QString &text, bool allowZone);
        QJsonArray extraCoreProcessPaths(const QString &corePath);
        QJsonObject directDomainResolver();
        void buildDNSSection(BuildContext &ctx, const HopNeeds &needs, const QList<QString> &vpnAuxTags, bool useDnsObj = true);

        QString missingHopError(const BuildContext &ctx, int id);
        bool resolvesHostnamesViaDnsRules(const Profile &hop);
        void collectEchQueryName(HopNeeds &needs, const Profile &hop);
        ChainOutput buildOutboundChain(const BuildContext &ctx, const ChainBuildRequest &req);
        // Never touches traffic or xrayFullConfig: each caller decides about those.
        void absorbChain(BuildContext &ctx, const ChainOutput &out);

        void buildOutboundsSection(BuildContext &ctx);

        QString routeExcludePrefix(const QString &range);
        void buildInboundSection(BuildContext &ctx);
        void buildRouteSection(BuildContext &ctx, const HopNeeds &needs, const QList<QString> &vpnAuxTags);

        QJsonObject xraySocksInbound(const QString &tag, const BridgeEndpoint &bridge);
        QJsonObject socksBridgeOutbound(const BridgeEndpoint &bridge);
        void buildXrayConfig(BuildContext &ctx);

    } // namespace gen
}
