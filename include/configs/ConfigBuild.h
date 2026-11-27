#pragma once
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

#include "include/configs/RouteRuleOrigin.h"
#include "include/database/entities/Profile.h"

namespace Configs
{
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
        // Members dial through chain hops in front of the selector.
        bool upstream = false;
    };

    class BuildConfigResult {
    public:
        QString error;
        QJsonObject coreConfig;
        QString tunIPv4CIDR;
        bool isXrayNeeded = false;
        // Generated, or the started profile's custom Xray full config with the generated parts merged in.
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
        // The routing profile it was built with; -1 for a custom full config.
        int routeId = -1;
        // The Xray sidecar's outbound resolution, read under the build's routing profile; empty without Xray.
        QString xrayDnsStrategy;
        // One per route.rules entry; empty for a custom full config.
        QList<RouteRuleOrigin> routeRuleOrigins;
    };

    // RPC deadlines cover the slowest possible tag; the core applies this floor per finalmask route.
    inline constexpr int kFinalMaskTestMinTimeoutMs = 10000;

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

    // routeId overrides the routing profile, which otherwise follows ent's group (RouteIdFor).
    std::shared_ptr<BuildConfigResult> BuildSingBoxConfig(const std::shared_ptr<Profile> &ent, std::optional<int> routeId = std::nullopt);

    struct RuleTestBuild {
        QString error;
        std::shared_ptr<BuildConfigResult> build;
        int profileId = -1;
    };

    // UI thread. Offline and consumes no OTP code; built from preferredProfileId when it can build routes, else from another profile.
    RuleTestBuild BuildRuleTestConfig(int routeId, int preferredProfileId);

    // Consumes no OTP code. coreUnreachable is set when a false came from a failed core call rather than from the config.
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

    // Checks a throwaway copy so the live profile's latency is never touched; empty = valid.
    QString ValidateScanBase(const std::shared_ptr<Profile> &base);

    // Worker thread only; group landing/front hops are not applied.
    ScanTestBuild BuildScanTestConfig(const std::shared_ptr<Profile> &base, const QList<ScanTestTarget> &targets);

    struct ChainStepProbe {
        // Index into the top-to-bottom hop list; hops.size() stands for the group's landing proxy.
        int hop = -1;
        // The hop dialed by itself rather than through every hop above it.
        bool alone = false;
    };

    struct ChainStepTestBuild {
        QString error;
        std::shared_ptr<BuildTestConfigResult> build;
        QHash<QString, ChainStepProbe> tag2probe;
        QSet<int> invalidHops;
        QStringList vpnEndpointTags;
        bool hasLanding = false;
    };

    // Worker thread only. Unsaved hops, top first; the group's front proxy leads every probe.
    ChainStepTestBuild BuildChainStepTestConfig(const QList<int> &hops, int gid);
}
