#include "include/api/remote/Router.hpp"

#include "NkrVersion.h"

#include <QDateTime>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "include/api/RPC.h"
#include "include/api/remote/Overlay.hpp"
#include "include/configs/generate.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/database/TrafficStatsRepo.h"
#include "include/engine/EditLocks.h"
#include "include/engine/EngineControl.h"
#include "include/engine/services/ServiceTypes.h"
#include "include/global/Configs.hpp"
#include "include/stats/traffic/TrafficStatsManager.hpp"
#include "include/sys/KillSwitch.hpp"

namespace RemoteApi::RouterImpl {
    constexpr int kDefaultWaitSec = 30;
    constexpr int kMaxWaitSec = 300;
    constexpr qint64 kDefaultMaxAgeSec = 3600;
    constexpr qint64 kTrafficWindowSec = 24 * 3600;
    constexpr qint64 kLiveRatesFreshMs = 3000;
    constexpr int kTunCapsRefreshMs = 1000;

    bool matchPath(const QString &pattern, const QString &path, int *id) {
        const auto want = pattern.split(QLatin1Char('/'));
        const auto got = path.split(QLatin1Char('/'));
        if (want.size() != got.size()) return false;
        for (qsizetype i = 0; i < want.size(); ++i) {
            if (want[i] != QLatin1String("{id}")) {
                if (want[i] != got[i]) return false;
                continue;
            }
            bool ok = false;
            const int value = got[i].toInt(&ok);
            if (!ok) return false;
            *id = value;
        }
        return true;
    }

    QList<int> groupOrder() {
        auto order = Configs::dataManager->groupsRepo->GetGroupsTabOrder();
        for (const int gid : Configs::dataManager->groupsRepo->GetAllGroupIds()) {
            if (!order.contains(gid)) order.append(gid);
        }
        return order;
    }

    QString profileDisplayName(const Configs::Profile &profile) {
        return profile.outbound ? profile.outbound->name : profile.name;
    }

    qint64 bitsPerSecond(const QString &speed) {
        const double bps = Configs::bitrateToBps(speed);
        return bps < 0 ? -1 : qRound64(bps);
    }

    // A result this request's own test produced counts even under max_age 0.
    bool resultCounts(const Configs::Profile &profile, qint64 requestedAt, qint64 maxAge) {
        if (profile.latency_at <= 0) return false;
        return profile.latency_at >= requestedAt || requestedAt - profile.latency_at < maxAge;
    }

    QString connectionStateName(Engine::ConnectionState state) {
        switch (state) {
            case Engine::ConnectionState::Connecting: return QStringLiteral("connecting");
            case Engine::ConnectionState::Running: return QStringLiteral("running");
            case Engine::ConnectionState::Stopping: return QStringLiteral("stopping");
            case Engine::ConnectionState::Idle: break;
        }
        return QStringLiteral("idle");
    }

    QString killSwitchName() {
        if (!Configs::dataManager->settingsRepo->kill_switch) return QStringLiteral("off");
        switch (Sys::KillSwitch::instance()->state()) {
            case Sys::KillSwitch::State::Arming: return QStringLiteral("arming");
            case Sys::KillSwitch::State::Armed: return QStringLiteral("armed");
            case Sys::KillSwitch::State::Failed: return QStringLiteral("failed");
            case Sys::KillSwitch::State::Disabled: break;
        }
        return QStringLiteral("off");
    }

    // Optimistic until the guard has run once: only a failed arm tells that this system cannot support it.
    bool killSwitchSupported() {
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
        return Sys::KillSwitch::instance()->failureCode() != QStringLiteral("unsupported");
#else
        return false;
#endif
    }

    // null until the core answers the privilege probe, rather than a false that is not known yet.
    QJsonValue tunCapability() {
        if (Configs::dataManager->settingsRepo->disable_privilege_req) return true;
#ifdef Q_OS_WIN
        const auto admin = Configs::KnownAdmin();
        return admin ? QJsonValue(*admin) : QJsonValue();
#else
        const auto caps = Configs::KnownCapabilities();
        if (!caps) return {};
        return caps->privileged && caps->tunDevice.value_or(true) && caps->netAdmin.value_or(true);
#endif
    }

    bool killSwitchCapability() {
        if (!killSwitchSupported()) return false;
#ifdef Q_OS_WIN
        return Configs::KnownAdmin().value_or(true);
#else
        const auto caps = Configs::KnownCapabilities();
        return !caps || caps->killSwitch.value_or(caps->privileged);
#endif
    }

    QString tunPrivilegeMessage() {
        const auto caps = Configs::KnownCapabilities();
        if (!caps || (caps->tunDevice != false && caps->netAdmin != false) || caps->detail.isEmpty())
            return QStringLiteral("Tun needs privileges that Throne does not have; grant them in Throne once");
        // A privileged core that still lacks the device or CAP_NET_ADMIN gains nothing from more privileges.
        if (caps->privileged) return QStringLiteral("Tun cannot work with Throne's core: %1").arg(caps->detail);
        return QStringLiteral("Tun needs privileges that Throne does not have (%1); grant them in Throne once").arg(caps->detail);
    }

    Response busyConnection() {
        return Response::Error(409, QStringLiteral("busy"), QStringLiteral("A profile is starting or stopping"));
    }

    Response startFailure(Engine::StartOutcome outcome, const QString &error) {
        const auto make = [&error](int status, const char *code, const char *fallback) {
            return Response::Error(status, QString::fromLatin1(code), error.isEmpty() ? QString::fromLatin1(fallback) : error);
        };
        switch (outcome) {
            case Engine::StartOutcome::Exiting: return make(503, "exiting", "Throne is shutting down");
            case Engine::StartOutcome::NotFound: return make(404, "not_found", "The profile does not exist");
            case Engine::StartOutcome::GroupUnavailable: return make(409, "group_archived", "The profile's group is archived");
            case Engine::StartOutcome::KillSwitchInactive:
                return make(409, "kill_switch_inactive", "The kill switch is on but not working, so Throne does not start profiles");
            case Engine::StartOutcome::Superseded: return make(409, "superseded", "A newer start replaced this one before it ran");
            case Engine::StartOutcome::CoreUnavailable: return make(503, "core_unavailable", "Throne's core process is not reachable");
            case Engine::StartOutcome::BuildFailed: return make(422, "build_failed", "Throne could not build the profile's configuration");
            case Engine::StartOutcome::Busy: return make(409, "busy", "Another profile is starting or stopping");
            case Engine::StartOutcome::ExtraCoreBlocked:
                return make(409, "kill_switch_blocked", "The kill switch could not allow the profile's extra core");
            case Engine::StartOutcome::GeoAssetsMissing:
                return make(422, "geo_assets_missing", "The profile needs geoip or geosite files that are missing");
            case Engine::StartOutcome::StrictRouteUnavailable:
                return make(422, "strict_route_unavailable", "Windows could not enable strict routing");
            case Engine::StartOutcome::TunFailed: return make(422, "tun_failed", "The Tun interface could not be configured");
            case Engine::StartOutcome::StartFailed:
            case Engine::StartOutcome::Started: break;
        }
        return make(422, "start_failed", "The core rejected the configuration");
    }

    QString trafficProfileName(int profileId, const QHash<int, QString> &metaNames) {
        if (profileId == Stats::DIRECT_STAT_PROFILE_ID) return QStringLiteral("Direct");
        if (const auto name = metaNames.value(profileId); !name.isEmpty()) return name;
        if (const auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId)) return profileDisplayName(*profile);
        if (profileId == Configs::warpProfileID) return QStringLiteral("built-in warp");
        return {};
    }
}

namespace RemoteApi {
    Router::Router(Engine::EngineControl *engine, QObject *parent) : QObject(parent), engine_(engine) {
        connect(engine, &Engine::EngineControl::startFinished, this,
                [this](quint64 serial, int, Engine::StartOutcome outcome, const QString &error) {
                    const auto callback = starts_.take(serial);
                    if (!callback) return;
                    if (outcome == Engine::StartOutcome::Started) callback(std::nullopt);
                    else callback(RouterImpl::startFailure(outcome, error));
                });
        connect(engine, &Engine::EngineControl::stopFinished, this, [this](int) {
            for (const auto &callback : std::exchange(stops_, {})) callback();
        });
        connect(engine, &Engine::EngineControl::connectionStateChanged, this, [this](Engine::ConnectionState state) {
            if (state == Engine::ConnectionState::Connecting) return;
            for (const auto &waiter : std::exchange(settling_, {})) answer(waiter, Response::Ok(statusJson()));
        });
    }

    void Router::dispatch(const Request &request, const Responder &respond) {
        struct Endpoint {
            const char *method;
            const char *path;
            Handler handler;
            // Management: refused unless Request::admin.
            bool admin = false;
        };
        static const Endpoint endpoints[] = {
            {"GET", "/v1/status", &Router::getStatus},
            {"GET", "/v1/groups", &Router::getGroups},
            {"GET", "/v1/profiles", &Router::getProfiles},
            {"GET", "/v1/profiles/{id}", &Router::getProfile},
            {"PUT", "/v1/connection", &Router::putConnection},
            {"DELETE", "/v1/connection", &Router::deleteConnection},
            {"PUT", "/v1/modes", &Router::putModes},
            {"GET", "/v1/routes", &Router::getRoutes},
            {"PUT", "/v1/routes/active", &Router::putActiveRoute},
            {"POST", "/v1/tests", &Router::postTest},
            {"GET", "/v1/tests", &Router::getTests},
            {"GET", "/v1/tests/{id}", &Router::getTest},
            {"DELETE", "/v1/tests/{id}", &Router::deleteTest},
            {"DELETE", "/v1/results", &Router::deleteResults},
            {"GET", "/v1/stats/live", &Router::getLiveStats},
            {"GET", "/v1/stats/traffic", &Router::getTrafficStats},
            {"POST", "/v1/import", &Router::postImport, true},
            {"POST", "/v1/groups", &Router::postGroup, true},
            {"POST", "/v1/groups/update", &Router::postGroupsUpdate},
            {"POST", "/v1/groups/{id}/update", &Router::postGroupUpdate},
            {"DELETE", "/v1/groups/{id}", &Router::deleteGroup, true},
            {"DELETE", "/v1/profiles", &Router::deleteProfiles, true},
            {"GET", "/v1/settings", &Router::getSettings},
            {"PUT", "/v1/settings", &Router::putSettings, true},
            {"POST", "/v1/backups", &Router::postBackup, true},
            {"GET", "/v1/backups/{id}", &Router::getBackup, true},
            {"DELETE", "/v1/backups/{id}", &Router::deleteBackup, true},
            {"POST", "/v1/restores", &Router::postRestore, true},
            {"PUT", "/v1/restores/{id}", &Router::putRestore, true},
            {"GET", "/v1/restores/{id}", &Router::getRestore, true},
            {"DELETE", "/v1/restores/{id}", &Router::deleteRestore, true},
            {"POST", "/v1/restores/{id}/apply", &Router::postRestoreApply, true},
            {"POST", "/v1/rule-sets/update", &Router::postRuleSetsUpdate},
            {"GET", "/v1/geo-assets", &Router::getGeoAssets},
            {"POST", "/v1/geo-assets/update", &Router::postGeoAssetsUpdate},
            {"POST", "/v1/restart", &Router::postRestart, true},
            {"POST", "/v1/shutdown", &Router::postShutdown, true},
        };

        const auto &settings = Configs::dataManager->settingsRepo;
        // A restore from the window sets noSave and restarts afterwards; until then the repos still cache what it replaced.
        if (restarting_ || (settings->noSave && !settings->prepare_exit)) {
            respond(Response::Error(503, QStringLiteral("restarting"), QStringLiteral("Throne is restarting")));
            return;
        }
        bool knownPath = false;
        for (const auto &endpoint : endpoints) {
            int id = -1;
            if (!RouterImpl::matchPath(QString::fromLatin1(endpoint.path), request.path, &id)) continue;
            knownPath = true;
            if (request.method != QLatin1String(endpoint.method)) continue;
            if (endpoint.admin && !request.admin) {
                respond(forbidden());
                return;
            }
            if (request.method != QLatin1String("GET")) {
                MW_show_log(tr("[API] %1 %2 from %3").arg(request.method, request.path, request.peer));
                if (settings->prepare_exit) {
                    respond(Response::Error(503, QStringLiteral("exiting"), QStringLiteral("Throne is shutting down")));
                    return;
                }
            }
            (this->*endpoint.handler)(request, respond, id);
            return;
        }
        if (knownPath) {
            respond(Response::Error(405, QStringLiteral("method_not_allowed"),
                                    QStringLiteral("%1 is not allowed on %2").arg(request.method, request.path)));
        } else {
            respond(notFound(QStringLiteral("Unknown path %1").arg(request.path)));
        }
    }

    Response Router::badRequest(const QString &message) {
        return Response::Error(400, QStringLiteral("bad_request"), message);
    }

    Response Router::notFound(const QString &message) {
        return Response::Error(404, QStringLiteral("not_found"), message);
    }

    Response Router::noProfiles() {
        return Response::Error(400, QStringLiteral("no_profiles"), QStringLiteral("The selection matched no profile"));
    }

    Response Router::forbidden() {
        if (CurrentOverlay().headless) {
            return Response::Error(403, QStringLiteral("forbidden"),
                                   QStringLiteral("Management requests are accepted only from this device. Start Throne with "
                                                  "--api-admin-lan (or THRONE_API_ADMIN_LAN=1) to accept them from the local network."));
        }
        return Response::Error(403, QStringLiteral("forbidden"),
                               QStringLiteral("Management requests are accepted only from this device. Allow management from the local "
                                              "network in Throne's Throne API settings to accept them from the LAN."));
    }

    Response Router::editing(const QString &what) {
        auto subject = what;
        if (!subject.isEmpty()) subject[0] = subject[0].toUpper();
        return Response::Error(409, QStringLiteral("editing"),
                               QStringLiteral("%1 is open in an editor in Throne's window; close it and try again").arg(subject));
    }

    Response Router::serviceError(const Engine::ServiceError &error) {
        if (error.code.isEmpty()) return Response::Error(500, QStringLiteral("internal"), error.message);
        return Response::Error(error.status, error.code, error.message);
    }

    bool Router::bodyObject(const Request &request, bool required, QJsonObject *body, Response *error) {
        if (request.body.isObject()) {
            *body = request.body.toObject();
            return true;
        }
        if (!required && (request.body.isNull() || request.body.isUndefined())) return true;
        *error = badRequest(required ? QStringLiteral("The request needs a JSON object body")
                                     : QStringLiteral("The body must be a JSON object"));
        return false;
    }

    bool Router::present(const QJsonObject &body, const QString &key) {
        const auto value = body.value(key);
        return !value.isUndefined() && !value.isNull();
    }

    bool Router::readBool(const QJsonObject &body, const QString &key, std::optional<bool> *out, Response *error) {
        if (!present(body, key)) return true;
        const auto value = body.value(key);
        if (!value.isBool()) {
            *error = badRequest(QStringLiteral("\"%1\" must be true or false").arg(key));
            return false;
        }
        *out = value.toBool();
        return true;
    }

    bool Router::readNumber(const QJsonObject &body, const QString &key, std::optional<double> *out, Response *error) {
        if (!present(body, key)) return true;
        const auto value = body.value(key);
        if (!value.isDouble() || !std::isfinite(value.toDouble())) {
            *error = badRequest(QStringLiteral("\"%1\" must be a number").arg(key));
            return false;
        }
        *out = value.toDouble();
        return true;
    }

    bool Router::readInteger(const QJsonObject &body, const QString &key, std::optional<qint64> *out, Response *error) {
        constexpr double kExactLimit = 9007199254740992.0;
        if (!present(body, key)) return true;
        const auto value = body.value(key);
        const double number = value.toDouble();
        if (!value.isDouble() || std::trunc(number) != number || std::abs(number) > kExactLimit) {
            *error = badRequest(QStringLiteral("\"%1\" must be an integer").arg(key));
            return false;
        }
        *out = static_cast<qint64>(number);
        return true;
    }

    bool Router::readId(const QJsonObject &body, const QString &key, std::optional<int> *out, Response *error) {
        std::optional<qint64> number;
        if (!readInteger(body, key, &number, error)) return false;
        if (!number) return true;
        if (*number < std::numeric_limits<int>::min() || *number > std::numeric_limits<int>::max()) {
            *error = badRequest(QStringLiteral("\"%1\" is out of range").arg(key));
            return false;
        }
        *out = static_cast<int>(*number);
        return true;
    }

    bool Router::readString(const QJsonObject &body, const QString &key, std::optional<QString> *out, Response *error) {
        if (!present(body, key)) return true;
        const auto value = body.value(key);
        if (!value.isString()) {
            *error = badRequest(QStringLiteral("\"%1\" must be a string").arg(key));
            return false;
        }
        *out = value.toString();
        return true;
    }

    bool Router::readWait(const QJsonObject &body, bool defaultWait, int defaultTimeoutSec, WaitOptions *out, Response *error) {
        std::optional<bool> wait;
        std::optional<double> timeout;
        if (!readBool(body, QStringLiteral("wait"), &wait, error) || !readNumber(body, QStringLiteral("timeout"), &timeout, error)) {
            return false;
        }
        if (timeout && (*timeout < 1 || *timeout > RouterImpl::kMaxWaitSec)) {
            *error = badRequest(QStringLiteral("\"timeout\" must be from 1 to %1 seconds").arg(RouterImpl::kMaxWaitSec));
            return false;
        }
        out->wait = wait.value_or(defaultWait);
        out->timeoutMs = static_cast<int>(std::lround(timeout.value_or(defaultTimeoutSec) * 1000));
        return true;
    }

    bool Router::readNameFilter(const QString &pattern, QRegularExpression *out, Response *error) {
        QRegularExpression filter(pattern, QRegularExpression::CaseInsensitiveOption);
        if (!filter.isValid()) {
            *error = badRequest(QStringLiteral("\"name\" is not a valid regular expression: %1").arg(filter.errorString()));
            return false;
        }
        *out = filter;
        return true;
    }

    bool Router::readSelection(const QJsonObject &body, QList<int> *ids, Response *error) const {
        const bool byList = present(body, QStringLiteral("profiles"));
        if (byList == present(body, QStringLiteral("group"))) {
            *error = badRequest(QStringLiteral("Select the profiles with either \"profiles\" or \"group\""));
            return false;
        }
        if (byList) {
            const auto list = body.value(QStringLiteral("profiles"));
            if (!list.isArray() || present(body, QStringLiteral("name"))) {
                *error = badRequest(QStringLiteral("\"profiles\" must be an array of profile ids, and \"name\" goes with \"group\""));
                return false;
            }
            QSet<int> seen;
            for (const auto &item : list.toArray()) {
                const double number = item.toDouble();
                if (!item.isDouble() || std::trunc(number) != number || number < 0 || number > std::numeric_limits<int>::max()) {
                    *error = badRequest(QStringLiteral("\"profiles\" must be an array of profile ids"));
                    return false;
                }
                const int id = static_cast<int>(number);
                if (seen.contains(id)) continue;
                seen.insert(id);
                ids->append(id);
            }
            QSet<int> known;
            for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(*ids)) known.insert(profile->id);
            for (const int id : *ids) {
                if (known.contains(id)) continue;
                *error = notFound(QStringLiteral("Unknown profile %1").arg(id));
                return false;
            }
            return true;
        }

        std::optional<int> gid;
        std::optional<QString> pattern;
        QRegularExpression nameFilter;
        if (!readId(body, QStringLiteral("group"), &gid, error) || !readString(body, QStringLiteral("name"), &pattern, error) ||
            !readNameFilter(pattern.value_or(QString()), &nameFilter, error)) {
            return false;
        }
        if (Configs::dataManager->groupsRepo->GetGroup(*gid) == nullptr) {
            *error = notFound(QStringLiteral("Unknown group %1").arg(*gid));
            return false;
        }
        for (const auto &profile : groupProfiles(*gid, nameFilter)) ids->append(profile->id);
        return true;
    }

    QList<std::shared_ptr<Configs::Profile>> Router::groupProfiles(int gid, const QRegularExpression &nameFilter) const {
        QList<std::shared_ptr<Configs::Profile>> profiles;
        const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (group == nullptr) return profiles;
        QSet<int> seen;
        for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(group->Profiles())) {
            if (seen.contains(profile->id)) continue;
            seen.insert(profile->id);
            if (nameFilter.match(RouterImpl::profileDisplayName(*profile)).hasMatch()) profiles.append(profile);
        }
        return profiles;
    }

    bool Router::connectionBusy() const {
        const auto state = engine_->connectionState();
        return state == Engine::ConnectionState::Connecting || state == Engine::ConnectionState::Stopping;
    }

    QJsonObject Router::statusJson() const {
        const auto &settings = Configs::dataManager->settingsRepo;
        const auto running = engine_->runningSnapshot();
        const auto route = Configs::dataManager->routesRepo->GetRouteProfile(settings->current_route_id);
        const auto degraded = engine_->degradedReason();
        // Empty while this instance's Tun is on.
        const auto tunConflict = engine_->tunBlockedReason();
        return {
            {"state", RouterImpl::connectionStateName(engine_->connectionState())},
            {"profile", running != nullptr ? QJsonValue(profileJson(running)) : QJsonValue()},
            {"last_profile_id", engine_->resolveLastProfile()},
            {"tun", settings->spmode_vpn},
            {"tun_conflict", tunConflict.isEmpty() ? QJsonValue() : QJsonValue(tunConflict)},
            {"system_proxy", settings->spmode_system_proxy},
            {"route", route != nullptr ? QJsonValue(routeJson(*route)) : QJsonValue()},
            {"kill_switch", RouterImpl::killSwitchName()},
            {"kill_switch_error", RouterImpl::killSwitchName() == QLatin1String("failed") && !Sys::KillSwitch::instance()->failureCode().isEmpty()
                                       ? QJsonValue(Sys::KillSwitch::instance()->failureCode())
                                       : QJsonValue()},
            {"core", settings->core_running},
            {"inbound", QJsonObject{
                {"enabled", !settings->disable_mixed_inbound},
                {"address", settings->inbound_address},
                {"port", settings->inbound_socks_port},
            }},
            {"capabilities", QJsonObject{
                {"tun", RouterImpl::tunCapability()},
                {"kill_switch", RouterImpl::killSwitchCapability()},
                {"headless", !IsGuiApp()},
            }},
            {"restart_needed", engine_->restartPending()},
            {"degraded", degraded.isEmpty() ? QJsonValue() : QJsonValue(degraded)},
            {"version", QStringLiteral(NKR_VERSION)},
        };
    }

    QJsonObject Router::profileJson(const std::shared_ptr<Configs::Profile> &profile) const {
        const auto running = engine_->runningSnapshot();
        int latency = profile->latency;
        if (latency < 0 && latency != Configs::kLatencyConnectOnly) latency = -1;
        return {
            {"id", profile->id},
            {"group", profile->gid},
            {"name", RouterImpl::profileDisplayName(*profile)},
            {"type", profile->type},
            {"address", Configs::DisplayEffectiveAddress(profile)},
            {"latency_ms", latency},
            {"latency_at", profile->latency_at},
            {"download_bps", RouterImpl::bitsPerSecond(profile->dl_speed)},
            {"upload_bps", RouterImpl::bitsPerSecond(profile->ul_speed)},
            {"country", profile->test_country},
            {"ip_out", profile->ip_out},
            {"traffic_up", profile->traffic_uplink},
            {"traffic_down", profile->traffic_downlink},
            {"running", running != nullptr && running->id == profile->id},
        };
    }

    QJsonObject Router::groupJson(const Configs::Group &group) {
        const bool subscription = !group.url.isEmpty();
        const auto ids = group.Profiles();
        return {
            {"id", group.id},
            {"name", group.name},
            {"subscription", subscription},
            {"archived", group.archive},
            {"profile_count", static_cast<qint64>(QSet<int>(ids.begin(), ids.end()).size())},
            {"updated_at", subscription ? group.sub_last_update : 0},
        };
    }

    QJsonObject Router::routeJson(const Configs::RouteProfile &route) {
        return {
            {"id", route.id},
            {"name", route.name},
            {"active", route.id == Configs::dataManager->settingsRepo->current_route_id},
        };
    }

    Response Router::accepted() const {
        return Response::Ok(statusJson(), 202);
    }

    Router::WaiterPtr Router::makeWaiter(const Responder &respond, int timeoutMs, std::function<Response()> onDeadline) {
        auto waiter = std::make_shared<Waiter>();
        waiter->respond = respond;
        waiter->onDeadline = std::move(onDeadline);
        waiter->deadline = new QTimer(this);
        waiter->deadline->setSingleShot(true);
        connect(waiter->deadline, &QTimer::timeout, this, [this, waiter] {
            if (!waiter->done) answer(waiter, waiter->onDeadline());
        });
        waiter->deadline->start(timeoutMs);
        return waiter;
    }

    void Router::answer(const WaiterPtr &waiter, const Response &response) {
        if (waiter->done) return;
        waiter->done = true;
        // Possibly from inside the timer's own slot; the waiter that slot holds is released with it.
        waiter->deadline->stop();
        waiter->deadline->deleteLater();
        waiter->deadline = nullptr;
        waiter->onDeadline = nullptr;
        waiter->respond(response);
    }

    void Router::fail(const WaiterPtr &waiter, const Response &error) {
        if (waiter != nullptr && !waiter->done) {
            answer(waiter, error);
        } else {
            MW_show_log(tr("[API] %1").arg(error.errorMessage));
        }
    }

    void Router::completeStart(const WaiterPtr &waiter, const std::optional<Response> &failure) {
        if (waiter->done) return;
        if (failure) {
            answer(waiter, *failure);
        } else if (engine_->connectionState() == Engine::ConnectionState::Connecting) {
            settling_.append(waiter);
        } else {
            answer(waiter, Response::Ok(statusJson()));
        }
    }

    void Router::applyWithRestart(const WaitOptions &options, const Responder &respond, const std::function<bool(quint64)> &apply) {
        const quint64 serial = engine_->nextStartSerial();
        WaiterPtr waiter;
        if (options.wait) {
            waiter = makeWaiter(respond, options.timeoutMs, [this] { return accepted(); });
            // Before apply(): a start that fails at once reports before apply() returns.
            starts_.insert(serial, [this, waiter](const std::optional<Response> &failure) { completeStart(waiter, failure); });
        }
        if (apply(serial)) {
            if (waiter == nullptr) respond(accepted());
            return;
        }
        starts_.remove(serial);
        if (waiter != nullptr) answer(waiter, Response::Ok(statusJson()));
        else respond(accepted());
    }

    std::optional<Response> Router::launch(int profileId, bool restart, const WaiterPtr &waiter) {
        if (connectionBusy()) return RouterImpl::busyConnection();
        const auto running = engine_->runningSnapshot();
        if (!restart && running != nullptr && running->id == profileId) return Response::Ok(statusJson());
        const quint64 serial = engine_->nextStartSerial();
        if (waiter != nullptr) {
            starts_.insert(serial, [this, waiter](const std::optional<Response> &failure) { completeStart(waiter, failure); });
        }
        engine_->start(Engine::StartRequest{profileId, false, serial});
        return std::nullopt;
    }

    void Router::getStatus(const Request &, const Responder &respond, int) {
        respond(Response::Ok(statusJson()));
    }

    void Router::getGroups(const Request &, const Responder &respond, int) {
        QJsonArray groups;
        for (const int gid : RouterImpl::groupOrder()) {
            if (const auto group = Configs::dataManager->groupsRepo->GetGroup(gid)) groups.append(groupJson(*group));
        }
        respond(Response::Ok(QJsonObject{{"groups", groups}}));
    }

    void Router::getProfiles(const Request &request, const Responder &respond, int) {
        Response error;
        QRegularExpression nameFilter;
        if (!readNameFilter(request.query.value(QStringLiteral("name")), &nameFilter, &error)) {
            respond(error);
            return;
        }
        std::optional<int> gid;
        if (request.query.contains(QStringLiteral("group"))) {
            bool ok = false;
            gid = request.query.value(QStringLiteral("group")).toInt(&ok);
            if (!ok) {
                respond(badRequest(QStringLiteral("\"group\" must be a group id")));
                return;
            }
            if (Configs::dataManager->groupsRepo->GetGroup(*gid) == nullptr) {
                respond(notFound(QStringLiteral("Unknown group %1").arg(*gid)));
                return;
            }
        }
        std::optional<QSet<int>> wanted;
        QList<int> wantedOrder;
        if (request.query.contains(QStringLiteral("ids"))) {
            wanted.emplace();
            for (const auto &part : request.query.value(QStringLiteral("ids")).split(QLatin1Char(','), Qt::SkipEmptyParts)) {
                bool ok = false;
                const int id = part.trimmed().toInt(&ok);
                if (!ok) {
                    respond(badRequest(QStringLiteral("\"ids\" must be comma-separated profile ids")));
                    return;
                }
                if (wanted->contains(id)) continue;
                wanted->insert(id);
                wantedOrder.append(id);
            }
        }

        QList<std::shared_ptr<Configs::Profile>> profiles;
        if (gid) {
            profiles = groupProfiles(*gid, nameFilter);
            if (wanted) profiles.removeIf([&wanted](const auto &profile) { return !wanted->contains(profile->id); });
        } else if (wanted) {
            for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(wantedOrder)) {
                if (nameFilter.match(RouterImpl::profileDisplayName(*profile)).hasMatch()) profiles.append(profile);
            }
        } else {
            QSet<int> seen;
            for (const int groupId : RouterImpl::groupOrder()) {
                for (const auto &profile : groupProfiles(groupId, nameFilter)) {
                    if (seen.contains(profile->id)) continue;
                    seen.insert(profile->id);
                    profiles.append(profile);
                }
            }
        }

        QJsonArray list;
        for (const auto &profile : profiles) list.append(profileJson(profile));
        respond(Response::Ok(QJsonObject{{"profiles", list}}));
    }

    void Router::getProfile(const Request &, const Responder &respond, int id) {
        const auto profile = Configs::dataManager->profilesRepo->GetProfile(id);
        if (profile == nullptr) {
            respond(notFound(QStringLiteral("Unknown profile %1").arg(id)));
            return;
        }
        respond(Response::Ok(profileJson(profile)));
    }

    void Router::putConnection(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<int> profileId;
        std::optional<QString> select;
        std::optional<bool> restart;
        WaitOptions options;
        if (!bodyObject(request, true, &body, &error) || !readId(body, QStringLiteral("profile"), &profileId, &error) ||
            !readString(body, QStringLiteral("select"), &select, &error) || !readBool(body, QStringLiteral("restart"), &restart, &error) ||
            !readWait(body, true, RouterImpl::kDefaultWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        if (profileId.has_value() == select.has_value()) {
            respond(badRequest(QStringLiteral("Name the profile with either \"profile\" or \"select\"")));
            return;
        }
        if (select == QStringLiteral("lowest_latency")) {
            startLowestLatency(body, restart.value_or(false), options, respond);
            return;
        }

        int id = -1;
        if (profileId) {
            if (Configs::dataManager->profilesRepo->GetProfile(*profileId) == nullptr) {
                respond(notFound(QStringLiteral("Unknown profile %1").arg(*profileId)));
                return;
            }
            id = *profileId;
        } else if (select == QStringLiteral("last")) {
            id = engine_->resolveLastProfile();
            if (id < 0) {
                respond(notFound(QStringLiteral("Throne has no profile to resume")));
                return;
            }
        } else {
            respond(badRequest(QStringLiteral("Unknown \"select\": %1").arg(*select)));
            return;
        }

        const WaiterPtr waiter = options.wait ? makeWaiter(respond, options.timeoutMs, [this] { return accepted(); }) : nullptr;
        if (const auto now = launch(id, restart.value_or(false), waiter)) {
            if (waiter != nullptr) answer(waiter, *now);
            else respond(*now);
            return;
        }
        if (waiter == nullptr) respond(accepted());
    }

    void Router::startLowestLatency(const QJsonObject &body, bool restart, const WaitOptions &options, const Responder &respond) {
        std::optional<int> gid;
        std::optional<QString> pattern;
        std::optional<qint64> maxAge;
        QRegularExpression nameFilter;
        Response error;
        if (!readId(body, QStringLiteral("group"), &gid, &error) || !readString(body, QStringLiteral("name"), &pattern, &error) ||
            !readNameFilter(pattern.value_or(QString()), &nameFilter, &error) ||
            !readInteger(body, QStringLiteral("max_age"), &maxAge, &error)) {
            respond(error);
            return;
        }
        if (maxAge.value_or(0) < 0) {
            respond(badRequest(QStringLiteral("\"max_age\" must not be negative")));
            return;
        }
        const int groupId = gid.value_or(Configs::dataManager->settingsRepo->current_group);
        const auto group = Configs::dataManager->groupsRepo->GetGroup(groupId);
        if (group == nullptr) {
            respond(notFound(QStringLiteral("Unknown group %1").arg(groupId)));
            return;
        }
        if (group->archive) {
            respond(Response::Error(409, QStringLiteral("group_archived"), QStringLiteral("The group is archived")));
            return;
        }

        const qint64 requestedAt = QDateTime::currentSecsSinceEpoch();
        const qint64 age = maxAge.value_or(RouterImpl::kDefaultMaxAgeSec);
        QList<int> candidates;
        QList<int> untested;
        for (const auto &profile : groupProfiles(groupId, nameFilter)) {
            if (profile->type == QStringLiteral("autoselector")) continue;
            candidates.append(profile->id);
            if (!RouterImpl::resultCounts(*profile, requestedAt, age)) untested.append(profile->id);
        }
        if (candidates.isEmpty()) {
            respond(noProfiles());
            return;
        }
        if (connectionBusy()) {
            respond(RouterImpl::busyConnection());
            return;
        }

        const WaiterPtr waiter = options.wait ? makeWaiter(respond, options.timeoutMs, [this] { return accepted(); }) : nullptr;
        const auto pick = [this, candidates, requestedAt, age, restart, waiter] {
            startBestCandidate(candidates, requestedAt, age, restart, waiter);
        };
        if (untested.isEmpty()) {
            pick();
        } else {
            MW_show_log(tr("[API] Testing %n profile(s) to find the lowest latency", nullptr, static_cast<int>(untested.size())));
            // The callback comes on the runner's worker thread.
            engine_->queueUrlTests(untested, [this, pick] { QMetaObject::invokeMethod(this, pick, Qt::QueuedConnection); });
        }
        if (waiter == nullptr) respond(accepted());
    }

    void Router::startBestCandidate(const QList<int> &candidates, qint64 requestedAt, qint64 maxAge, bool restart, const WaiterPtr &waiter) {
        std::shared_ptr<Configs::Profile> best;
        for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(candidates)) {
            if (profile->latency <= 0 || !RouterImpl::resultCounts(*profile, requestedAt, maxAge)) continue;
            if (best == nullptr || profile->latency < best->latency) best = profile;
        }
        if (best == nullptr) {
            fail(waiter, Response::Error(409, QStringLiteral("no_working_profile"), QStringLiteral("No candidate passed the latency test")));
            return;
        }
        MW_show_log(tr("[API] Lowest latency: %1 (%2 ms)").arg(RouterImpl::profileDisplayName(*best)).arg(best->latency));
        const auto now = launch(best->id, restart, waiter);
        if (!now) return;
        if (now->status >= 400) fail(waiter, *now);
        else if (waiter != nullptr && !waiter->done) answer(waiter, *now);
    }

    void Router::deleteConnection(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        WaitOptions options;
        if (!bodyObject(request, false, &body, &error) || !readWait(body, true, RouterImpl::kDefaultWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        switch (engine_->connectionState()) {
            case Engine::ConnectionState::Idle:
                // Still a stop: it cancels the resume of a crashed profile that waits for the core.
                engine_->stop(Engine::StopOptions{.manual = true, .interactive = false});
                respond(Response::Ok(statusJson()));
                return;
            case Engine::ConnectionState::Connecting:
            case Engine::ConnectionState::Stopping:
                respond(RouterImpl::busyConnection());
                return;
            case Engine::ConnectionState::Running:
                break;
        }
        if (options.wait) {
            const auto waiter = makeWaiter(respond, options.timeoutMs, [this] { return accepted(); });
            stops_.append([this, waiter] { answer(waiter, Response::Ok(statusJson())); });
        }
        engine_->stop(Engine::StopOptions{.manual = true, .interactive = false});
        if (!options.wait) respond(accepted());
    }

    void Router::putModes(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<bool> tun;
        std::optional<bool> systemProxy;
        WaitOptions options;
        if (!bodyObject(request, false, &body, &error) || !readBool(body, QStringLiteral("tun"), &tun, &error) ||
            !readBool(body, QStringLiteral("system_proxy"), &systemProxy, &error) ||
            !readWait(body, true, RouterImpl::kDefaultWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        if (systemProxy.has_value() && *systemProxy && !engine_->systemProxySupported()) {
            respond(Response::Error(409, QStringLiteral("system_proxy_unsupported"),
                                    QStringLiteral("Headless Throne does not set the system proxy; point applications at its inbound")));
            return;
        }
        const auto &settings = Configs::dataManager->settingsRepo;
        const bool proxyChanges = systemProxy.has_value() && *systemProxy != settings->spmode_system_proxy;
        const bool tunChanges = tun.has_value() && *tun != settings->spmode_vpn;
        // A Tun restore the boot held leaves spmode_vpn false, so tun:false would otherwise be a no-op that keeps the hold.
        const bool clearHeldTun = tun.has_value() && !*tun && !tunChanges && engine_->tunRestoreHeld();
        const auto releaseHeldTun = [this] { engine_->setTun(false, Engine::ModeChange{.interactive = false, .restart = false}); };
        if (!proxyChanges && !tunChanges) {
            if (clearHeldTun) releaseHeldTun();
            respond(Response::Ok(statusJson()));
            return;
        }
        if (proxyChanges && *systemProxy && settings->disable_mixed_inbound) {
            respond(Response::Error(409, QStringLiteral("mixed_inbound_disabled"),
                                    QStringLiteral("The system proxy needs Throne's mixed inbound, which is turned off")));
            return;
        }
        if (tunChanges && *tun) Configs::RefreshStaleTunDevice(RouterImpl::kTunCapsRefreshMs);
        if (tunChanges && *tun && !settings->disable_privilege_req &&
            (!engine_->tunPrivileged() || RouterImpl::tunCapability() == QJsonValue(false))) {
            respond(Response::Error(412, QStringLiteral("needs_privileges"), RouterImpl::tunPrivilegeMessage()));
            return;
        }
        if (tunChanges && *tun) {
            if (const auto conflict = engine_->tunBlockedReason(); !conflict.isEmpty()) {
                respond(Response::Error(409, QStringLiteral("tun_conflict"), conflict));
                return;
            }
        }
        if (connectionBusy()) {
            respond(RouterImpl::busyConnection());
            return;
        }
        // System proxy first so that a Tun restart picks it up; at most one of the two restarts.
        applyWithRestart(options, respond, [&](quint64 serial) {
            bool restarted = false;
            if (clearHeldTun) releaseHeldTun();
            if (proxyChanges) {
                restarted = engine_->setSystemProxy(
                    *systemProxy, Engine::ModeChange{.interactive = false, .restart = !tunChanges, .restartSerial = serial});
            }
            if (tunChanges) {
                restarted = engine_->setTun(*tun, Engine::ModeChange{.interactive = false, .restartSerial = serial}) || restarted;
            }
            return restarted;
        });
    }

    void Router::getRoutes(const Request &, const Responder &respond, int) {
        QJsonArray routes;
        for (const auto &route : Configs::dataManager->routesRepo->GetAllRouteProfiles()) {
            if (route != nullptr) routes.append(routeJson(*route));
        }
        respond(Response::Ok(QJsonObject{{"routes", routes}}));
    }

    void Router::putActiveRoute(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<int> routeId;
        WaitOptions options;
        if (!bodyObject(request, true, &body, &error) || !readId(body, QStringLiteral("id"), &routeId, &error) ||
            !readWait(body, true, RouterImpl::kDefaultWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        if (!routeId) {
            respond(badRequest(QStringLiteral("\"id\" is required")));
            return;
        }
        if (Configs::dataManager->routesRepo->GetRouteProfile(*routeId) == nullptr) {
            respond(notFound(QStringLiteral("Unknown routing profile %1").arg(*routeId)));
            return;
        }
        if (Configs::dataManager->settingsRepo->current_route_id == *routeId) {
            respond(Response::Ok(statusJson()));
            return;
        }
        // The editor writes current_route_id back on OK.
        if (Engine::IsBeingEdited(Engine::EditTarget::Routes)) {
            respond(editing(QStringLiteral("the routing profile list")));
            return;
        }
        if (connectionBusy()) {
            respond(RouterImpl::busyConnection());
            return;
        }
        applyWithRestart(options, respond, [&](quint64 serial) { return engine_->chooseRoute(*routeId, false, serial); });
    }

    void Router::getLiveStats(const Request &, const Responder &respond, int) {
        const auto rates = engine_->freshLiveRates(RouterImpl::kLiveRatesFreshMs).value_or(Engine::LiveRates{});
        respond(Response::Ok(QJsonObject{
            {"proxy", QJsonObject{{"up", rates.proxyUp}, {"down", rates.proxyDown}}},
            {"direct", QJsonObject{{"up", rates.directUp}, {"down", rates.directDown}}},
        }));
    }

    void Router::getTrafficStats(const Request &request, const Responder &respond, int) {
        const auto by = request.query.value(QStringLiteral("by"), QStringLiteral("profile"));
        if (by != QStringLiteral("profile") && by != QStringLiteral("app")) {
            respond(badRequest(QStringLiteral("\"by\" must be profile or app")));
            return;
        }
        bool toOk = true;
        bool fromOk = true;
        const qint64 to = request.query.contains(QStringLiteral("to")) ? request.query.value(QStringLiteral("to")).toLongLong(&toOk)
                                                                       : QDateTime::currentSecsSinceEpoch();
        const qint64 from = request.query.contains(QStringLiteral("from")) ? request.query.value(QStringLiteral("from")).toLongLong(&fromOk)
                                                                           : to - RouterImpl::kTrafficWindowSec;
        if (!toOk || !fromOk || from > to) {
            respond(badRequest(QStringLiteral("\"from\" and \"to\" must be Unix seconds, \"from\" not after \"to\"")));
            return;
        }
        auto *repo = Configs::dataManager->trafficStatsRepo.get();
        if (Configs::dataManager->settingsRepo->disable_traffic_aggregation || repo == nullptr || repo->Disabled()) {
            respond(Response::Error(409, QStringLiteral("stats_disabled"), QStringLiteral("Traffic statistics are turned off in Throne")));
            return;
        }
        // The current minute stays in memory until flushed.
        Stats::trafficStatsManager->Flush();

        QJsonArray items;
        if (by == QStringLiteral("app")) {
            auto usage = repo->QueryAppUsage(from, to);
            std::stable_sort(usage.begin(), usage.end(), [](const Configs::AppUsage &a, const Configs::AppUsage &b) {
                return a.up + a.down > b.up + b.down;
            });
            for (const auto &item : usage) {
                items.append(QJsonObject{{"name", item.process_name}, {"up", item.up}, {"down", item.down}});
            }
        } else {
            auto usage = repo->QueryConfigUsage(from, to);
            std::stable_sort(usage.begin(), usage.end(), [](const Configs::ConfigUsage &a, const Configs::ConfigUsage &b) {
                return a.up + a.down > b.up + b.down;
            });
            QHash<int, QString> names;
            for (const auto &meta : repo->GetAllConfigMeta()) names.insert(meta.profile_id, meta.name);
            for (const auto &item : usage) {
                items.append(QJsonObject{
                    {"id", item.profile_id},
                    {"name", RouterImpl::trafficProfileName(item.profile_id, names)},
                    {"up", item.up},
                    {"down", item.down},
                });
            }
        }
        respond(Response::Ok(QJsonObject{{"by", by}, {"from", from}, {"to", to}, {"items", items}}));
    }
}
