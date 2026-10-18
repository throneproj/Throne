#pragma once
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include "include/database/entities/Profile.h"

namespace Configs
{
    // Not "dashboard": that one is the Clash external_ui dir, holding a different UI.
    inline constexpr auto apiDashboardDir = "sb-dashboard";

    class ExtraCoreData
    {
        public:
        QString path;
        QString args;
        QString config;
        bool noLog = false;
    };

    enum class TrafficChainKind { Main, Route, Aux };

    struct TrafficChainGroup {
        QString watchTag;
        QList<std::shared_ptr<Profile>> profiles;
        TrafficChainKind kind = TrafficChainKind::Main;
        int profileId = -1;
        QString name;
    };

    struct AutoSelectorBuildInfo {
        QString groupTag;
        std::shared_ptr<Profile> profile;
        QList<QPair<QString, std::shared_ptr<Profile>>> members;
    };

    class BuildConfigResult {
    public:
        QString error;
        QJsonObject coreConfig;
        QString tunIPv4CIDR;
        bool isXrayNeeded = false;
        QJsonObject xrayConfig;
        // Opaque full configs, one instance each; never merged into xrayConfig.
        QStringList xrayFullConfigs;
        std::shared_ptr<ExtraCoreData> extraCoreData = std::make_shared<ExtraCoreData>();

        QList<TrafficChainGroup> chainGroups;
        QList<AutoSelectorBuildInfo> autoSelectors;
        // Endpoint hop tag -> profile id, so a live status can be named after its profile.
        QMap<QString, int> vpnEndpointProfiles;
        // Rule-addressable outbound tag -> the profile it stands for, so connections are named after it.
        QHash<QString, QString> outboundNames;
        // Every profile the config was built from, chain hops and route members included.
        QSet<int> involvedProfiles;
    };

    class BuildTestConfigResult {
    public:
        QString error;
        QMap<int, QString> fullConfigs;
        QStringList xrayFullConfigs;
        QMap<QString, int> tag2entID;
        QJsonObject coreConfig;
        QJsonObject xrayConfig;
        bool isXrayNeeded = false;
        QStringList outboundTags;
        QString xrayDnsStrategy;
    };

    inline QString get_jsdelivr_link(QString link)
    {
        if(Configs::dataManager->settingsRepo->ruleset_mirror == Mirrors::GITHUB)
            return link;
        if(auto url = QUrl(link); url.isValid() && url.host() == "raw.githubusercontent.com")
        {
            QStringList list = url.path().split('/');
            QString result;
            switch(Configs::dataManager->settingsRepo->ruleset_mirror) {
            case Mirrors::GCORE: result = "https://gcore.jsdelivr.net/gh"; break;
            case Mirrors::QUANTIL: result = "https://quantil.jsdelivr.net/gh"; break;
            case Mirrors::FASTLY: result = "https://fastly.jsdelivr.net/gh"; break;
            case Mirrors::CDN: result = "https://cdn.jsdelivr.net/gh"; break;
            default: result = "https://testingcf.jsdelivr.net/gh";
            }

            int index = 0;
            foreach(QString item, list)
            {
                if(!item.isEmpty())
                {
                    if(index == 2)
                        result += "@" + item;
                    else
                        result += "/" + item;
                    index++;
                }
            }
            return result;
        }
        return link;
    }

    constexpr int warpProfileID = -2408;

    struct PredefinedDNSEntry {
        QString domain;
        QStringList v4;
        QStringList v6;
    };

    // Hosts-file syntax: "<address> <domain> [domain...]", '#' comments, repeated domains accumulate.
    bool ParsePredefinedDNS(const QStringList &lines, QList<PredefinedDNSEntry> &out, QString *error = nullptr);

    // sing-box duration grammar: one or more "<number><unit>" with unit ns/us/ms/s/m/h/d.
    bool IsValidDuration(const QString &text);

    // port 0 means the type's default; iface is empty for dhcp://auto.
    struct DnsAddress {
        QString type;
        QString server;
        int port = 0;
        QString path;
        QString iface;
    };

    // "local...", "dhcp://auto|<iface>", "udp|tcp|tls|quic://host[:port]", "https|h3://host[:port][/path]" or a bare "host[:port]" (udp).
    bool ParseDnsAddress(const QString &text, DnsAddress *out, QString *why = nullptr);

    std::shared_ptr<BuildConfigResult> BuildSingBoxConfig(const std::shared_ptr<Profile> &ent);

    // coreUnreachable is set when a false came from a failed core call rather than from the config.
    bool IsValid(const std::shared_ptr<Profile> &ent, bool *coreUnreachable = nullptr);

    // While alive, IsValid() on this thread runs only its local checks and counts the core's CheckConfig as passed.
    class ScopedOfflineValidation {
    public:
        ScopedOfflineValidation();
        ~ScopedOfflineValidation();
        ScopedOfflineValidation(const ScopedOfflineValidation &) = delete;
        ScopedOfflineValidation &operator=(const ScopedOfflineValidation &) = delete;
    };

    // Eligible: an openvpn/openconnect profile, or a chain whose exit hop is one, never the reverse.
    bool CanBeAuxEndpoint(const std::shared_ptr<Profile> &ent);

    // Hops behind the exit of a chain endpoint that are endpoints themselves, exit-first.
    QList<int> AuxEndpointInnerHops(int endpointProfileID);

    std::shared_ptr<BuildTestConfigResult> BuildTestConfig(const QList<std::shared_ptr<Profile> > &profiles);

    struct ScanTestTarget {
        QString address;
        // 0 = keep the profile's port.
        int port = 0;
    };

    struct ScanTestBuild {
        QString error;
        std::shared_ptr<BuildTestConfigResult> build;
        QHash<QString, int> tag2target;
        QStringList vpnEndpointTags;
        QList<int> unsupported;
    };

    // Keeps the original domain as SNI/Host; nullptr when the type has no single dialable server.
    std::shared_ptr<Profile> CloneProfileWithServer(const std::shared_ptr<Profile> &base, const QString &address, int port);

    struct EndpointResolution {
        // Empty: the profile dials its own address.
        QString address;
        // Where the address comes from, for hints and logs, e.g. IP list “Scan CF Result”.
        QString origin;
        // Why a configured source supplied no address: a missing or empty IP list.
        QString problem;
    };

    // What `source` makes a profile dial; Inherit and Own supply nothing.
    EndpointResolution ResolveEndpointSource(const EndpointSource &source);

    // The profile's own source, or its group's while it inherits.
    EndpointSource EffectiveEndpointSource(const Profile &profile);

    // Non-empty when the profile has no single server address an endpoint source could replace.
    QString EndpointOverrideBlocker(const std::shared_ptr<Profile> &profile);

    // For display: the host the profile dials instead of its own, or empty. IP lists are cached until the next invalidation.
    QString EffectiveEndpointHost(const std::shared_ptr<Profile> &profile);

    // The profile's address as its type shows it, with the effective host in place of its own.
    QString DisplayEffectiveAddress(const std::shared_ptr<Profile> &profile);

    // Called whenever IP lists change, so displays pick up a new first entry.
    void InvalidateEndpointDisplayCache();

    // Checks a throwaway copy so the live profile's latency is never touched; empty = valid.
    QString ValidateScanBase(const std::shared_ptr<Profile> &base);

    // Worker thread only; group landing/front hops are not applied.
    ScanTestBuild BuildScanTestConfig(const std::shared_ptr<Profile> &base, const QList<ScanTestTarget> &targets);

    // Accepts host, host:port, [v6] and [v6]:port; a bare IPv6 address takes defaultPort.
    void SplitWarpEndpoint(const QString &endpoint, int defaultPort, QString &host, int &port);
}
