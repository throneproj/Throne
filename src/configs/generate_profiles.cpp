#include "include/configs/generate.h"
#include "include/configs/generate_internal.h"
#include "include/api/RPC.h"
#include "include/configs/AutoSelectorPlan.h"
#include "include/configs/common/Protocols.h"
#include "include/configs/common/ResolverSource.h"
#include "include/configs/common/utils.h"
#include "include/configs/outbounds/psiphon.h"
#include "include/configs/outbounds/tor.h"
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

    namespace gen {

        QString hopTag(const QString &prefix, int index) { return prefix + "-" + Int2String(index); }

        // ------------------------------------------------------ profile queries

        // Build-scoped, so a concurrent test build on a worker thread cannot pollute the started profile's set.
        thread_local QSet<int> *buildProfileSink = nullptr;

        // Build-scoped like buildProfileSink: scan clones shadow ids without ever reaching the repo.
        thread_local const QHash<int, std::shared_ptr<Profile>> *buildProfileOverrides = nullptr;

        // Build-scoped like buildProfileSink; without one every lookup resolves afresh and logs nothing.
        thread_local GenerateEndpointCache *generateEndpointCache = nullptr;

    } // namespace gen

    namespace {

        GenerateServerless generateServerlessReason(const std::shared_ptr<Profile> &profile) {
            if (profile == nullptr || profile->outbound == nullptr) return GenerateServerless::Missing;
            const auto &type = profile->type;
            if (Protocols::Is(type, Protocols::Serverless) || profile->outbound->IsExtraCore() ||
                profile->outbound->IsXrayFullConfig())
                return GenerateServerless::NoServer;
            if (const auto *hy = profile->Hysteria(); hy != nullptr && hy->RealmActive())
                return GenerateServerless::Realm;
            return GenerateServerless::None;
        }

        std::shared_ptr<Profile> generateEndpointApply(const std::shared_ptr<Profile> &ent) {
            auto *cache = generateEndpointCache;
            if (cache != nullptr) {
                if (const auto it = cache->profiles.constFind(ent->id); it != cache->profiles.constEnd() && it->live == ent)
                    return it->clone != nullptr ? it->clone : ent;
            }
            std::shared_ptr<Profile> clone;
            if (generateServerlessReason(ent) == GenerateServerless::None) {
                const auto source = EffectiveEndpointSource(*ent);
                const auto resolution = ResolveEndpointSource(source);
                if (!resolution.address.isEmpty()) {
                    clone = CloneProfileWithServer(ent, resolution.address, resolution.port);
                    if (clone != nullptr) clone->id = ent->id;
                } else if (!resolution.problem.isEmpty() && cache != nullptr && !cache->loggedLists.contains(source.ipListId)) {
                    cache->loggedLists.insert(source.ipListId);
                    MW_show_log(QObject::tr("%1 dials its own address: %2").arg(ent->outbound->DisplayTypeAndName(), resolution.problem));
                }
            }
            if (cache != nullptr) cache->profiles.insert(ent->id, {ent, clone});
            return clone != nullptr ? clone : ent;
        }

    } // namespace

    namespace gen {

        // Profiles a build hands out (traffic credit, selector members) must be the live ones, not their clones.
        std::shared_ptr<Profile> generateEndpointLive(const std::shared_ptr<Profile> &ent) {
            const auto *cache = generateEndpointCache;
            if (cache == nullptr || ent == nullptr) return ent;
            const auto it = cache->profiles.constFind(ent->id);
            return it != cache->profiles.constEnd() && it->clone == ent ? it->live : ent;
        }

        QString profileDisplayName(const std::shared_ptr<Profile> &ent) {
            return ent->outbound != nullptr ? ent->outbound->DisplayName() : ent->name;
        }

        std::shared_ptr<Profile> getProfile(int id) {
            if (buildProfileOverrides != nullptr) {
                if (const auto it = buildProfileOverrides->constFind(id); it != buildProfileOverrides->constEnd())
                    return it.value();
            }
            auto ent = dataManager->profilesRepo->GetProfile(id);
            if (ent == nullptr) return nullptr;
            if (buildProfileSink != nullptr) buildProfileSink->insert(id);
            return generateEndpointApply(ent);
        }

        bool isCustomFullConfig(const std::shared_ptr<Profile> &profile) {
            return profile->type == "custom" && profile->Custom() != nullptr &&
                   profile->Custom()->type == Custom::CustomFullConfig;
        }

        bool isXrayFullConfig(const std::shared_ptr<Profile> &profile) {
            return profile->outbound != nullptr && profile->outbound->IsXrayFullConfig();
        }

        bool usesXrayCore(const std::shared_ptr<Profile> &profile) {
            return profile->outbound != nullptr &&
                   (profile->outbound->IsXray() || profile->outbound->IsXrayFullConfig());
        }

    } // namespace gen

    namespace {

        // Accepts host, host:port, [v6] and [v6]:port; a bare IPv6 address has no port.
        void splitWarpEndpoint(const QString &endpoint, int defaultPort, QString &host, int &port) {
            const auto ep = endpoint.trimmed();
            host = ep;
            port = defaultPort;
            QString portText;
            if (ep.startsWith('[')) {
                const auto close = ep.indexOf(']');
                if (close < 0) return;
                host = ep.mid(1, close - 1);
                if (ep.mid(close + 1).startsWith(':')) portText = ep.mid(close + 2);
            } else if (ep.count(':') == 1) {
                const auto sep = ep.lastIndexOf(':');
                host = ep.left(sep);
                portText = ep.mid(sep + 1);
            }
            if (const auto parsed = portText.toInt(); parsed > 0 && parsed <= 65535) port = parsed;
        }

        // Tor and Psiphon profile types spell the exit_through values of the exits they duplicate; Server for any other hop.
        ExitThrough hopDuplicatesExit(const std::shared_ptr<Profile> &hop) {
            if (hop == nullptr) return ExitThrough::Server;
            const auto exit = ExitOf(hop->type);
            return exit == ExitThrough::Tor || exit == ExitThrough::Psiphon ? exit : ExitThrough::Server;
        }

        // Psiphon carries UDP through its servers' udpgw; Tor carries only TCP.
        bool hopCarriesOnlyTcp(const std::shared_ptr<Profile> &hop) {
            return hopDuplicatesExit(hop) == ExitThrough::Tor;
        }

        // WireGuard and AmneziaWG always run over UDP; MASQUE does only when it may not fall back to HTTP/2.
        bool warpNeedsUdp(const SettingsValues &settings) {
            return settings.warp_mode != QLatin1String("masque") || settings.warp_masque_http_mode == 1;
        }

        // Rides on the selected profile, which already gets past censorship, so it needs no bridges.
        std::shared_ptr<Profile> torExitProfile(const SettingsValues &settings) {
            auto torProfile = std::make_shared<Profile>();
            torProfile->type = "tor";
            torProfile->name = "Tor";
            torProfile->id = torExitProfileID;
            auto outbound = std::make_shared<tor>();
            outbound->name = "Tor";
            outbound->bridge_mode = "none";
            outbound->exit_country = settings.tor_exit_country.trimmed().toLower();
            torProfile->outbound = outbound;
            return torProfile;
        }

        std::shared_ptr<Profile> psiphonExitProfile(const SettingsValues &settings) {
            auto psiphonProfile = std::make_shared<Profile>();
            psiphonProfile->type = "psiphon";
            psiphonProfile->name = "Psiphon";
            psiphonProfile->id = psiphonExitProfileID;
            auto outbound = std::make_shared<psiphon>();
            outbound->name = "Psiphon";
            outbound->egress_region = settings.psiphon_egress_region.trimmed().toUpper();
            psiphonProfile->outbound = outbound;
            return psiphonProfile;
        }

        std::shared_ptr<Profile> warpExitProfile(const SettingsValues &settings) {
            auto warpProfile = std::make_shared<Profile>();
            warpProfile->name = "warp";
            warpProfile->id = warpProfileID;

            if (settings.warp_mode == "masque") {
                warpProfile->type = "masque";
                auto outbound = std::make_shared<masque>();
                outbound->name = "warp";
                splitWarpEndpoint(settings.warp_masque_ep, 443, outbound->server, outbound->server_port);
                outbound->private_key = settings.warp_masque_private_key;
                outbound->peer_public_key = settings.warp_masque_peer_public_key;
                outbound->address = settings.warp_masque_ifc_addrs;
                outbound->mtu = 1280;
                if (!settings.warp_masque_sni.isEmpty()) outbound->tls->server_name = settings.warp_masque_sni;
                switch (settings.warp_masque_http_mode) {
                    case 1:
                        outbound->http_version = 3;
                        outbound->disable_version_fallback = true;
                        break;
                    case 2:
                        outbound->http_version = 2;
                        break;
                    default:
                        break;
                }
                warpProfile->outbound = outbound;
                return warpProfile;
            }

            warpProfile->type = "wireguard";
            auto outbound = std::make_shared<wireguard>();
            outbound->name = "warp";
            splitWarpEndpoint(settings.warp_ep, 2408, outbound->server, outbound->server_port);
            outbound->private_key = settings.warp_private_key;
            outbound->address = settings.warp_ifc_addrs;
            auto peer = std::make_shared<Peer>();
            peer->public_key = settings.warp_public_key;
            peer->address = outbound->server;
            peer->port = outbound->server_port;
            peer->reserved = QStringList2QListInt(settings.warp_reserved);
            peer->persistent_keepalive = "10";
            outbound->peer = peer;
            outbound->mtu = 1280;

            if (settings.warp_mode == "amnezia") {
                outbound->enable_amnezia = true;
                outbound->jc = 4;
                outbound->jmin = 40;
                outbound->jmax = 70;
                // Cloudflare expects the standard WireGuard headers.
                outbound->h1 = "1";
                outbound->h2 = "2";
                outbound->h3 = "3";
                outbound->h4 = "4";
                const auto &packets = settings.warp_amnezia_packets;
                const QList<QString *> fields = {&outbound->i1, &outbound->i2, &outbound->i3, &outbound->i4, &outbound->i5};
                for (qsizetype i = 0; i < fields.size(); ++i)
                    *fields[i] = packets.value(i);
            }

            warpProfile->outbound = outbound;
            return warpProfile;
        }

    } // namespace

    namespace gen {

        ExitThrough activeExit() { return ExitOf(dataManager->settingsRepo->exit_through); }

        bool exitWrapperOn() { return activeExit() != ExitThrough::Server; }

        std::shared_ptr<Profile> exitProfile(QString *error) {
            const auto &settings = *dataManager->settingsRepo;
            std::shared_ptr<Profile> profile;
            QString refusal;
            switch (activeExit()) {
                case ExitThrough::Server:
                    break;
                case ExitThrough::Warp:
                    if (WarpConfigured(settings.warp_mode)) profile = warpExitProfile(settings);
                    else refusal = "WARP is enabled but its config has not been generated. Please generate the WARP config first in Routing Settings.";
                    break;
                case ExitThrough::Tor:
                    // A missing executable is BuildConfigResult::torMissing, which offers the download.
                    profile = torExitProfile(settings);
                    break;
                case ExitThrough::Psiphon:
                    profile = psiphonExitProfile(settings);
                    break;
            }
            if (error != nullptr) *error = refusal;
            return profile;
        }

        std::shared_ptr<Profile> exitHopOf(const std::shared_ptr<Profile> &ent) {
            // An auto selector's members leave through their own groups' landing proxies, so it has no one exit hop.
            if (ent == nullptr || ent->type == "autoselector") return nullptr;
            std::shared_ptr<Profile> hop = ent;
            if (const auto group = dataManager->groupsRepo->GetGroup(ent->gid); group != nullptr && group->landing_proxy_id >= 0) {
                hop = getProfile(group->landing_proxy_id);
            } else if (ent->type == "chain") {
                const auto *chain = ent->Chain();
                if (chain == nullptr || chain->list.isEmpty()) return nullptr;
                hop = getProfile(chain->list.last());
            }
            return hop != nullptr && hop->type == "autoselector" ? nullptr : hop;
        }

        bool exitCarriesOnlyTcp(const std::shared_ptr<Profile> &ent) {
            switch (activeExit()) {
                case ExitThrough::Server:
                    return hopCarriesOnlyTcp(exitHopOf(ent));
                case ExitThrough::Tor:
                    return true;
                case ExitThrough::Warp:
                case ExitThrough::Psiphon:
                    return false;
            }
            return false;
        }

        bool exitReachesOnion(const std::shared_ptr<Profile> &ent) {
            switch (activeExit()) {
                case ExitThrough::Server:
                    return hopDuplicatesExit(exitHopOf(ent)) == ExitThrough::Tor;
                case ExitThrough::Tor:
                    return true;
                case ExitThrough::Warp:
                case ExitThrough::Psiphon:
                    return false;
            }
            return false;
        }

        QString exitError(const std::shared_ptr<Profile> &ent) {
            const auto exit = activeExit();
            if (exit == ExitThrough::Server) return {};
            if (QString error; exitProfile(&error) == nullptr) return error;
            const auto hop = exitHopOf(ent);
            if (exit == ExitThrough::Warp) {
                if (!hopCarriesOnlyTcp(hop) || !warpNeedsUdp(*dataManager->settingsRepo)) return {};
                return QObject::tr("The selected profile leaves through %1, which carries only TCP, but WARP needs UDP in its current mode. "
                                   "In Routing Settings > Exit, set WARP to MASQUE with HTTP Version HTTP/2, or choose another exit.")
                    .arg(ExitName(hopDuplicatesExit(hop)));
            }
            if (hopDuplicatesExit(hop) != exit) return {};
            return QObject::tr("The selected profile already leaves through %1, so the %1 exit cannot run over it. "
                               "Choose another exit in Routing Settings > Exit.")
                .arg(ExitName(exit));
        }

        // ------------------------------------------------------- prerequisites

        std::shared_ptr<Profile> resolveExtraCoreProfile(const std::shared_ptr<Profile> &ent)
        {
            if (ent->outbound != nullptr && ent->outbound->IsExtraCore()) return ent;
            if (ent->type != "chain") return nullptr;
            auto chain = ent->Chain();
            if (chain == nullptr || chain->list.isEmpty()) return nullptr;
            auto firstEnt = getProfile(chain->list[0]);
            if (firstEnt != nullptr && firstEnt->outbound != nullptr && firstEnt->outbound->IsExtraCore())
                return firstEnt;
            return nullptr;
        }

        QString scanCloneBlocker(const std::shared_ptr<Profile> &profile) {
            switch (generateServerlessReason(profile)) {
                case GenerateServerless::Missing:
                    return QObject::tr("The profile no longer exists");
                case GenerateServerless::NoServer:
                    return QObject::tr("%1 profiles have no single server to scan").arg(profile->type);
                case GenerateServerless::Realm:
                    return QObject::tr("Hysteria2 realm profiles have no fixed server to scan");
                case GenerateServerless::None:
                    break;
            }
            return {};
        }

        std::shared_ptr<Profile> scanDeepClone(const std::shared_ptr<Profile> &base) {
            if (base == nullptr || base->outbound == nullptr) return nullptr;
            auto clone = ProfilesRepo::NewProfile(base->type);
            if (clone->outbound == nullptr || clone->outbound->invalid) return nullptr;
            if (!clone->outbound->ParseFromJson(base->outbound->ExportToJson())) return nullptr;
            clone->name = base->name;
            clone->gid = base->gid;
            // ResolveVpnCredentials finds the in-memory credential override by the original profile's id.
            clone->outbound->profile_id = base->id >= 0 ? base->id : base->outbound->profile_id;
            return clone;
        }

    } // namespace gen

    std::shared_ptr<Profile> CloneProfileWithServer(const std::shared_ptr<Profile> &base, const QString &address, int port)
    {
        if (!scanCloneBlocker(base).isEmpty()) return nullptr;
        auto clone = scanDeepClone(base);
        if (clone == nullptr) return nullptr;
        auto *out = clone->outbound.get();

        QString original = base->outbound->GetAddress().trimmed();
        if (auto *ovpn = clone->OpenVPN(); ovpn != nullptr) {
            // server/server_port and servers are mutually exclusive; the first remote supplies what the scan keeps.
            if (!ovpn->servers.isEmpty()) {
                const auto &first = ovpn->servers.first();
                if (original.isEmpty()) original = first->server.trimmed();
                if (first->server_port > 0) ovpn->server_port = first->server_port;
                if (!first->network.isEmpty()) ovpn->network = first->network;
            }
            ovpn->servers.clear();
            ovpn->remote_random = false;
        }
        UnwrapIPV6Host(original);

        if (!original.isEmpty() && !IsIpAddress(original)) {
            if (out->HasTLS()) {
                if (auto tls = out->GetTLS(); tls != nullptr && tls->server_name.isEmpty()) tls->server_name = original;
            }
            if (out->HasTransport()) {
                auto transport = out->GetTransport();
                if (transport != nullptr && transport->host.isEmpty() &&
                    (transport->type == "ws" || transport->type == "httpupgrade" || transport->type == "http"))
                    transport->host = original;
            }
            if (out->HasXrayStream()) {
                if (auto stream = out->GetXrayStream(); stream != nullptr) {
                    if (stream->security == "tls" && stream->TLS->serverName.isEmpty()) stream->TLS->serverName = original;
                    if (stream->security == "reality" && stream->reality->serverName.isEmpty())
                        stream->reality->serverName = original;
                    if (stream->network == "ws" && stream->ws->host.isEmpty()) stream->ws->host = original;
                    if (stream->network == "httpupgrade" && stream->httpupgrade->host.isEmpty())
                        stream->httpupgrade->host = original;
                    if (stream->network == "xhttp" && stream->xhttp->host.isEmpty()) stream->xhttp->host = original;
                }
            }
            if (auto *ocon = clone->OpenConnect(); ocon != nullptr && ocon->tls->server_name.isEmpty())
                ocon->tls->server_name = original;
        }

        if (port > 0) {
            if (auto *hy = clone->Hysteria(); hy != nullptr) {
                hy->server_ports.clear();
                hy->hop_interval.clear();
                hy->hop_interval_max.clear();
            }
            if (auto *mi = clone->Mieru(); mi != nullptr) mi->server_ports.clear();
        }

        QString host = address.trimmed();
        UnwrapIPV6Host(host);
        out->SetAddress(host);
        if (port > 0) out->SetPort(port);
        clone->id = -1;
        return clone;
    }

    QString EndpointResolution::DisplayTarget() const
    {
        return port > 0 ? DisplayAddress(address, port) : address;
    }

    EndpointResolution ResolveEndpointSource(const EndpointSource &source)
    {
        if (source.mode == EndpointSource::Mode::Address)
            return {source.address.trimmed(), 0, QObject::tr("the group's fixed address"), {}};
        if (source.mode != EndpointSource::Mode::IpList) return {};

        auto *cache = generateEndpointCache;
        if (cache != nullptr) {
            if (const auto it = cache->lists.constFind(source.ipListId); it != cache->lists.constEnd()) return it.value();
        }
        EndpointResolution res;
        if (const auto list = dataManager->ipListsRepo->GetIpListHeader(source.ipListId); list == nullptr || list->IsHidden()) {
            res.problem = QObject::tr("The IP list no longer exists");
        } else {
            res.origin = QObject::tr("IP list \"%1\"").arg(list->name);
            if (const auto best = dataManager->ipListsRepo->GetBestEntry(source.ipListId)) {
                res.address = best->cidr.section('/', 0, 0);
                res.port = best->port;
            } else
                res.problem = QObject::tr("The IP list \"%1\" is empty").arg(list->name);
        }
        if (cache != nullptr) cache->lists.insert(source.ipListId, res);
        return res;
    }

    EndpointSource EffectiveEndpointSource(const Profile &profile)
    {
        if (profile.endpoint.mode != EndpointSource::Mode::Inherit) return profile.endpoint;
        const auto group = dataManager->groupsRepo->GetGroup(profile.gid);
        return group != nullptr ? group->endpoint : EndpointSource{};
    }

    QString EndpointOverrideBlocker(const std::shared_ptr<Profile> &profile)
    {
        switch (generateServerlessReason(profile)) {
            case GenerateServerless::Missing:
                return QObject::tr("The profile no longer exists");
            case GenerateServerless::NoServer:
                return QObject::tr("%1 profiles have no single server address to replace").arg(profile->type);
            case GenerateServerless::Realm:
                return QObject::tr("Hysteria2 realm profiles have no fixed server address to replace");
            case GenerateServerless::None:
                break;
        }
        return {};
    }

    namespace {
        QMutex generateDisplayListsMutex;
        QHash<int, EndpointResolution> generateDisplayLists;

        EndpointResolution generateDisplayEndpoint(const std::shared_ptr<Profile> &profile)
        {
            if (generateServerlessReason(profile) != GenerateServerless::None) return {};
            const auto source = EffectiveEndpointSource(*profile);
            if (source.mode == EndpointSource::Mode::Address) return {source.address.trimmed()};
            if (source.mode != EndpointSource::Mode::IpList) return {};
            {
                QMutexLocker locker(&generateDisplayListsMutex);
                if (const auto it = generateDisplayLists.constFind(source.ipListId); it != generateDisplayLists.constEnd())
                    return it.value();
            }
            const auto resolution = ResolveEndpointSource(source);
            QMutexLocker locker(&generateDisplayListsMutex);
            generateDisplayLists.insert(source.ipListId, resolution);
            return resolution;
        }
    } // namespace

    QString EffectiveEndpointHost(const std::shared_ptr<Profile> &profile)
    {
        return generateDisplayEndpoint(profile).address;
    }

    QString DisplayEffectiveAddress(const std::shared_ptr<Profile> &profile)
    {
        if (profile == nullptr || profile->outbound == nullptr) return {};
        if (const auto endpoint = generateDisplayEndpoint(profile); !endpoint.address.isEmpty()) {
            if (const auto clone = CloneProfileWithServer(profile, endpoint.address, endpoint.port); clone != nullptr)
                return clone->outbound->DisplayAddress();
        }
        return profile->outbound->DisplayAddress();
    }

    void InvalidateEndpointDisplayCache()
    {
        QMutexLocker locker(&generateDisplayListsMutex);
        generateDisplayLists.clear();
    }

    void SplitWarpEndpoint(const QString &endpoint, int defaultPort, QString &host, int &port)
    {
        splitWarpEndpoint(endpoint, defaultPort, host, port);
    }

    bool WarpConfigured(const QString &mode)
    {
        const auto &settings = *dataManager->settingsRepo;
        if (mode == "masque") {
            return !settings.warp_masque_private_key.isEmpty() &&
                   !settings.warp_masque_peer_public_key.isEmpty() &&
                   !settings.warp_masque_ep.isEmpty() &&
                   !settings.warp_masque_ifc_addrs.isEmpty();
        }
        return !settings.warp_private_key.isEmpty() &&
               !settings.warp_public_key.isEmpty() &&
               !settings.warp_ep.isEmpty() &&
               !settings.warp_ifc_addrs.isEmpty() &&
               !(mode == "amnezia" && settings.warp_amnezia_packets.isEmpty());
    }
}
