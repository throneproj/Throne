#include "include/configs/generate.h"
#include "include/configs/generate_internal.h"
#include "include/api/RPC.h"
#include "include/configs/AutoSelectorPlan.h"
#include "include/configs/common/ResolverSource.h"
#include "include/configs/common/utils.h"
#include "include/global/Configs.hpp"

#include <QApplication>
#include <QFileInfo>
#include <QHostAddress>
#include <QMutex>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QThreadPool>


#include "include/database/GroupsRepo.h"
#include "include/database/IpListsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"


#include "include/database/entities/Profile.h"
#include "include/global/OtpPlaceholder.hpp"
#include "include/global/RunningProfiles.hpp"
#include "include/global/VpnCredentialOverride.hpp"
#ifdef Q_OS_LINUX
#include "include/sys/linux/systemChecks.h"
#endif

#include <algorithm>
#include <optional>
#include <string_view>

namespace Configs {
    using namespace gen;

    namespace {

        thread_local int offlineValidationDepth = 0;

    } // namespace

    namespace gen {

        // ------------------------------------------------------- small sections

        void buildLogSection(BuildContext &ctx) {
            ctx.result->coreConfig.insert("log", QJsonObject{{"level", dataManager->settingsRepo->log_level}});
        }

        void buildNTPSection(BuildContext &ctx) {
            const auto &settings = *dataManager->settingsRepo;
            if (!settings.enable_ntp) return;
            ctx.result->coreConfig["ntp"] = QJsonObject{
                {"enabled", true},
                {"server", settings.ntp_server_address},
                {"server_port", settings.ntp_server_port},
                {"interval", settings.ntp_interval},
                {"detour", (settings.ntp_outbound == tags::proxy && ctx.policy.ntpViaProxy) ? tags::proxy : tags::direct},
            };
        }

        void buildCertificateSection(BuildContext &ctx) {
            ctx.result->coreConfig.insert("certificate",
                QJsonObject{{"store", dataManager->settingsRepo->use_mozilla_certs ? "mozilla" : "system"}});
        }

        // Concurrent because the RPC channel multiplexes by request id and the core answers each in its own goroutine.
        QSet<int> invalidProfileIDs(const QList<int> &ids)
        {
            QSet<int> invalid;
            if (ids.isEmpty()) return invalid;
            QMutex mu;
            QThreadPool pool;
            pool.setMaxThreadCount(10);
            // The offline scope is thread-local; carry it into the pool threads.
            const bool offline = offlineValidationDepth > 0;
            for (int id : ids)
            {
                pool.start([&, id] {
                    std::optional<ScopedOfflineValidation> offlineScope;
                    if (offline) offlineScope.emplace();
                    const auto ent = getProfile(id);
                    if (ent == nullptr || IsValid(ent)) return;
                    QMutexLocker lock(&mu);
                    invalid.insert(id);
                });
            }
            pool.waitForDone();
            return invalid;
        }

    } // namespace gen

    namespace {

        // -------------------------------------------------------- experimental

        void buildExperimentalSection(BuildContext &ctx) {
            if (!ctx.policy.controlPlane) return;
            const auto &settings = *dataManager->settingsRepo;

            QJsonObject experimentalObj;
            // Only the yacd listener now; the stats tracker comes from buildServicesSection.
            if (settings.core_box_clash_api > 0) {
                // An empty host would listen on every interface.
                QString listenHost = settings.core_box_clash_listen_addr.trimmed();
                if (listenHost.isEmpty()) listenHost = "127.0.0.1";
                else if (isIPv6Literal(listenHost, true)) listenHost = "[" + listenHost + "]";
                experimentalObj["clash_api"] = QJsonObject{
                    {"external_controller", listenHost + ":" + Int2String(settings.core_box_clash_api)},
                    {"secret", settings.core_box_clash_api_secret},
                    {"external_ui", "dashboard"},
                };
            }

            // enabled is unconditional: the same file backs the remote rule-set cache and the auto-selector's last pick.
            experimentalObj["cache_file"] = QJsonObject{
                {"enabled", true},
                {"store_fakeip", settings.dns_persist_cache},
                {"store_dns", settings.dns_persist_cache}
            };

            ctx.result->coreConfig["experimental"] = experimentalObj;
        }

        // ------------------------------------------------------------- services

        // The core builds the traffic tracker from the mere presence of an api service.
        void buildServicesSection(BuildContext &ctx) {
            if (!ctx.policy.controlPlane) return;
            const auto &settings = *dataManager->settingsRepo;

            const bool dashboard = settings.core_box_api_port > 0;
            if (!dashboard && !settings.enable_stats) return;

            QJsonObject api = {
                {"type", "api"},
                {"listen", "127.0.0.1"},
                {"listen_port", dashboard ? settings.core_box_api_port : 0},
                {"secret", settings.core_box_api_secret},
            };
            if (dashboard) {
                // Defaults to "*", i.e. any page the user visits could reach loopback.
                api["access_control_allow_origin"] = QJsonArray{
                    "http://127.0.0.1:" + Int2String(settings.core_box_api_port)
                };
                api["dashboard"] = QJsonObject{
                    {"enabled", true},
                    {"path", apiDashboardDir},
                };
            }

            ctx.result->coreConfig["services"] = QJsonArray{api};
        }

    } // namespace

    std::shared_ptr<BuildConfigResult> BuildSingBoxConfig(const std::shared_ptr<Profile>& ent, std::optional<int> routeId) {
        if (ent->type == "custom")
        {
            auto res = std::make_shared<BuildConfigResult>();
            res->involvedProfiles = {ent->id};
            auto custom = ent->Custom();
            if (custom == nullptr)
            {
                res->error = "Corrupted data, needed custom ent, got nullptr";
                return res;
            }
            if (custom->type == Custom::CustomFullConfig)
            {
                res->coreConfig = custom->Build().object;
                // macOS points the system DNS at the TUN address once the core starts; a custom config
                // brings its own tun inbound, so read the address from there like the generated one.
                for (const auto item : res->coreConfig["inbounds"].toArray()) {
                    const auto inbound = item.toObject();
                    if (inbound["type"].toString() != "tun") continue;
                    const auto address = inbound["address"];
                    QStringList addresses;
                    if (address.isString()) addresses << address.toString();
                    else for (const auto entry : address.toArray()) addresses << entry.toString();
                    for (const auto &cidr : addresses) {
                        if (cidr.contains(':')) continue;
                        res->tunIPv4CIDR = cidr;
                        break;
                    }
                    if (!res->tunIPv4CIDR.isEmpty()) break;
                }
                return res;
            }
        }

        const GenerateEndpointScope endpointScope;
        BuildContext ctx;
        ctx.ent = ent;
        ctx.routeId = routeId ? *routeId : RouteIdFor(ent->gid);
        ctx.result->routeId = ctx.routeId;
        const ScopedBuildRoute routeScope(ctx.routeId);
        ctx.result->involvedProfiles = {ent->id};
        buildProfileSink = &ctx.result->involvedProfiles;
        const auto clearProfileSink = qScopeGuard([] { buildProfileSink = nullptr; });

        auto failed = [&ctx] {
            if (ctx.error.isEmpty()) return false;
            MW_show_log("Config build error:" + ctx.error);
            ctx.result->error = ctx.error;
            return true;
        };

        calculatePrerequisites(ctx);
        if (failed()) return ctx.result;

        buildLogSection(ctx);
        buildNTPSection(ctx);
        buildCertificateSection(ctx);
        buildInboundSection(ctx);
        if (failed()) return ctx.result;

        buildOutboundsSection(ctx);
        if (failed()) return ctx.result;

        // Ordered after the outbounds: it needs the tags the VPN endpoint hops landed on.
        buildDNSSection(ctx, ctx.needs, ctx.vpnAuxTags);
        if (failed()) return ctx.result;

        buildRouteSection(ctx, ctx.needs, ctx.vpnAuxTags);
        if (failed()) return ctx.result;

        buildExperimentalSection(ctx);
        if (failed()) return ctx.result;

        buildServicesSection(ctx);
        if (failed()) return ctx.result;

        buildXrayConfig(ctx);
        if (failed()) return ctx.result;

        if (!ctx.result->xrayConfig.isEmpty() || !ctx.result->xrayFullConfigs.isEmpty())
            ctx.result->xrayDnsStrategy = getXrayOutboundDomainStrategy();

        return ctx.result;
    }

    RuleTestBuild BuildRuleTestConfig(int routeId, int preferredProfileId) {
        RuleTestBuild res;
        if (!dataManager->routesRepo->HasRouteProfile(routeId)) {
            res.error = QObject::tr("The routing profile no longer exists.");
            return res;
        }
        auto canBuildRoutes = [](const std::shared_ptr<Profile> &ent) {
            if (ent == nullptr || ent->outbound == nullptr || ent->type == "autoselector") return false;
            if (ent->type != "custom") return true;
            const auto custom = ent->Custom();
            return custom != nullptr && custom->type != Custom::CustomFullConfig;
        };

        QList<int> candidates{preferredProfileId, dataManager->settingsRepo->remember_id};
        auto ids = dataManager->profilesRepo->GetAllProfileIds();
        std::sort(ids.begin(), ids.end());
        candidates.append(ids);

        const ScopedOfflineValidation offlineValidation;
        const ScopedOtpSuppression otpSuppression;
        constexpr int maxAttempts = 4;
        int attempts = 0;
        QSet<int> tried;
        for (const int id : candidates) {
            if (id < 0 || tried.contains(id)) continue;
            tried.insert(id);
            const auto ent = dataManager->profilesRepo->GetProfile(id);
            if (!canBuildRoutes(ent)) continue;
            auto build = BuildSingBoxConfig(ent, routeId);
            if (build->error.isEmpty()) return {{}, build, id};
            if (res.error.isEmpty()) res.error = build->error;
            if (++attempts >= maxAttempts) break;
        }
        if (res.error.isEmpty()) res.error = QObject::tr("No profile can be used to build the routing configuration: add a proxy profile first.");
        return res;
    }

    ScopedOfflineValidation::ScopedOfflineValidation() { offlineValidationDepth++; }

    ScopedOfflineValidation::~ScopedOfflineValidation() { offlineValidationDepth--; }

    bool IsValid(const std::shared_ptr<Profile>& ent, bool *coreUnreachable)
    {
        const ScopedOtpSuppression otpSuppression;
        const bool offline = offlineValidationDepth > 0;
        const auto coreCallFailed = [coreUnreachable](const QString &resp) {
            MW_show_log("Failed to Call the Core: " + resp);
            if (coreUnreachable != nullptr) *coreUnreachable = true;
            return false;
        };
        if (ent->type == "autoselector")
        {
            const auto plan = PlanAutoSelector(ent);
            if (!plan.error.isEmpty())
            {
                MW_show_log("Invalid auto selector: " + plan.error);
                return false;
            }
            return !plan.build.isEmpty();
        }
        if (ent->type == "chain")
        {
            auto chain = ent->Chain();
            if (chain == nullptr)
            {
                MW_show_log("Corrupted data, needed chain ent, got nullptr");
                return false;
            }
            for (int eId : chain->list)
            {
                auto e = getProfile(eId);
                if (e == nullptr)
                {
                    MW_show_log("Null ent in validator");
                    return false;
                }
                if (!IsValid(e, coreUnreachable))
                {
                    MW_show_log("Invalid ent in chain: ID=" + QString::number(eId));
                    return false;
                }
            }
            return true;
        }
        QJsonObject conf;
        bool fullConf = false;
        if (ent->type == "custom")
        {
            auto custom = ent->Custom();
            if (custom == nullptr)
            {
                MW_show_log("Corrupted data in isValid, needed custom ent, got nullptr");
                return false;
            }
            if (custom->type == Custom::CustomFullConfig)
            {
                conf = QString2QJsonObject(custom->config);
                fullConf = true;
            }
            if (custom->type == Custom::CustomXrayFullConfig)
            {
                auto xrayConf = QString2QJsonObject(custom->config);
                if (xrayConf.isEmpty()) {
                    MW_show_log("Custom Xray full config is not valid JSON");
                    return false;
                }
                // Throne never runs these; it prepends its own bridge inbound instead.
                xrayConf.remove("inbounds");
                if (offline) return true;
                bool ok;
                auto resp = API::defaultClient->CheckConfig(&ok, QJsonObject2QString(xrayConf, true), true);
                if (!ok) return coreCallFailed(resp);
                if (resp.isEmpty()) return true;
                // Left to fail at test time so handleXrayGeoAssetError() can name the missing category.
                if (resp.contains("geoip.dat") || resp.contains("geosite.dat")) return true;
                MW_show_log("Invalid Xray ent " + ent->outbound->name + ": " + resp);
                return false;
            }
        }
        // Xray outbounds carry only a dummy sing-box Build(); validate the real one via the Xray core.
        if (!fullConf && ent->outbound->IsXray())
        {
            auto [out, err] = ent->outbound->BuildXray();
            if (!err.isEmpty())
            {
                MW_show_log("Invalid Xray ent " + ent->outbound->name + ": " + err);
                return false;
            }
            if (offline) return true;
            QJsonObject xrayConf{
                {"outbounds", QJsonArray{out}},
            };
            bool ok;
            auto resp = API::defaultClient->CheckConfig(&ok, QJsonObject2QString(xrayConf, true), true);
            if (!ok) return coreCallFailed(resp);
            if (resp.isEmpty()) return true;
            MW_show_log("Invalid Xray ent " + ent->outbound->name + ": " + resp);
            return false;
        }
        if (!fullConf)
        {
            auto out = ent->outbound->Build();
            if (!out.error.isEmpty())
            {
                // A bad global fragment setting fails every fragmenting profile alike; "invalid" here would delete them all.
                if (const auto fragmentError = FragmentSettingsError(); !fragmentError.isEmpty() && out.error == fragmentError)
                {
                    MW_show_log("Could not check ent " + ent->outbound->name + ": " + out.error);
                    return true;
                }
                MW_show_log("Invalid ent " + ent->outbound->name + ": " + out.error);
                return false;
            }
            auto outArr = QJsonArray{out.object};
            auto key = ent->outbound->IsEndpoint() ? "endpoints" : "outbounds";
            conf = {
                {key, outArr},
                };
        }
        if (offline) return true;
        bool ok;
        conf.insert("log", QJsonObject{{"level", dataManager->settingsRepo->log_level}});
        auto resp = API::defaultClient->CheckConfig(&ok, QJsonObject2QString(conf, true));
        if (!ok) return coreCallFailed(resp);
        if (resp.isEmpty()) return true;
        MW_show_log("Invalid ent " + ent->outbound->name + ": " + resp);
        return false;
    }
}
