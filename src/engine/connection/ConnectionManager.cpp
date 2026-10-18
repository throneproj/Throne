#include "include/engine/connection/ConnectionManager.h"

#include "include/api/RPC.h"
#include "include/api/remote/Server.hpp"
#include "include/configs/common/xrayStreamSetting.h"
#include "include/configs/generate.h"
#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EnginePolicy.h"
#include "include/engine/EngineTr.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/autoselector/AutoSelectorController.h"
#include "include/engine/services/AssetUpdates.h"
#include "include/engine/services/StatsService.h"
#include "include/engine/supervisor/CoreSupervisor.h"
#include "include/engine/tests/TestRunner.h"
#include "include/engine/vpn/VpnAuthController.h"
#include "include/engine/vpn/VpnEndpoints.h"
#include "include/global/ExitLookup.hpp"
#include "include/global/Logger.hpp"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/sys/KillSwitch.hpp"

#include <QHostAddress>
#include <QJsonObject>
#include <QMutexLocker>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

namespace {
#ifdef Q_OS_LINUX
    constexpr bool kConnectionManagerLinux = true;
#else
    constexpr bool kConnectionManagerLinux = false;
#endif

    // Main thread, headless Linux, as a start brings the Tun up: once per boot each, the Tun settings that cut API clients off.
    void connectionManagerWarnTunApiReach() {
        static bool warnedRedirect = false;
        static bool warnedIpv6 = false;
        const auto api = RemoteApi::Server::instance()->lastResult();
        if (api.state != RemoteApi::ApplyResult::State::Listening || api.address.isLoopback()) return;
        const auto &settings = Configs::dataManager->settingsRepo;
        if (!settings->vpn_auto_redirect && !std::exchange(warnedRedirect, true)) {
            LOG_WARN(QStringLiteral("[Tun] replies to API clients outside the bypassed address ranges go into the tunnel; remote API "
                                    "clients may lose the connection. From this host: thronectl tun off (after eval \"$(Throne "
                                    "--headless --print-connection)\"), or restart with --no-resume"));
        }
        const auto protocol = api.address.protocol();
        const bool acceptsIpv6 = protocol == QAbstractSocket::IPv6Protocol || protocol == QAbstractSocket::AnyIPProtocol;
        if (settings->vpn_strict_route && !settings->vpn_ipv6 && acceptsIpv6 && !std::exchange(warnedIpv6, true)) {
            LOG_WARN(QStringLiteral("[Tun] strict route without Tun IPv6 drops IPv6 API clients while Tun runs; set tun_ipv6 or listen "
                                    "on IPv4"));
        }
    }
} // namespace

namespace Engine {
    ConnectionManager::ConnectionManager(EnginePolicy *policy, CoreSupervisor *coreSupervisor, StatsService *stats,
                                         const RuntimeOptions &options, QObject *parent)
        : QObject(parent), m_policy(policy), m_coreSupervisor(coreSupervisor), m_stats(stats), m_options(options) {
        connect(m_coreSupervisor, &CoreSupervisor::coreStarted, this, &ConnectionManager::onCoreStarted);
        connect(m_coreSupervisor, &CoreSupervisor::coreGaveUp, this, &ConnectionManager::onCoreGaveUp);
        m_lastProfileId = Configs::dataManager->settingsRepo->remember_id;
    }

    void ConnectionManager::attach(TestRunner *tests, AutoSelectorController *autoSelector, VpnAuthController *vpnAuth) {
        m_tests = tests;
        m_autoSelector = autoSelector;
        m_vpnAuth = vpnAuth;
    }

    void ConnectionManager::start(const StartRequest &request) {
        ENGINE_CHECK_MAIN_THREAD();
        ++m_geoRetryToken;
        if (Configs::dataManager->settingsRepo->prepare_exit) {
            emit startFinished(request.serial, request.profileId, StartOutcome::Exiting, {});
            return;
        }

        std::shared_ptr<Configs::Profile> ent = nullptr;
        if (request.profileId >= 0) {
            ent = Configs::dataManager->profilesRepo->GetProfile(request.profileId);
        }
        if (ent == nullptr) {
            emit startFinished(request.serial, request.profileId, StartOutcome::NotFound, {});
            return;
        }

        m_lastProfileId = ent->id;

        emit startDispatching(request.serial, ent->id);

        const auto group = Configs::dataManager->groupsRepo->GetGroup(ent->gid);
        if (group == nullptr || group->archive) {
            emit startFinished(request.serial, ent->id, StartOutcome::GroupUnavailable, {});
            return;
        }

        StartRequest resolved = request;
        resolved.profileId = ent->id;

        auto *killSwitch = Sys::KillSwitch::instance();
        if (killSwitch->state() == Sys::KillSwitch::State::Arming) {
            // Only the latest request survives, so repeated clicks while arming start one profile.
            const bool queued = m_killSwitchDeferredStart.profileId >= 0;
            if (queued) {
                emit startFinished(m_killSwitchDeferredStart.serial, m_killSwitchDeferredStart.profileId, StartOutcome::Superseded, {});
            }
            m_killSwitchDeferredStart = resolved;
            if (!queued) {
                killSwitch->whenSettled(this, [this](bool) {
                    if (const auto next = std::exchange(m_killSwitchDeferredStart, StartRequest{}); next.profileId >= 0) start(next);
                });
            }
            return;
        }
        if (!killSwitch->allowsStart()) {
            if (killSwitch->failedForIdentity()) {
                MW_show_log(QStringLiteral("[Kill switch] Not starting %1: profiles are refused while the core cannot adopt the "
                                           "kill-switch group (the kill switch keeps blocking).")
                                .arg(ent->outbound->DisplayTypeAndName()));
            } else {
                MW_show_log(MainWindowTr::tr("[Kill switch] Not starting %1: the kill switch is not active.").arg(ent->outbound->DisplayTypeAndName()));
            }
            emit startFinished(request.serial, ent->id, StartOutcome::KillSwitchInactive, killSwitch->failureText());
            if (request.interactive) policy()->killSwitchProblem();
            return;
        }
        if (guardRestartPending() ||
            (killSwitch->state() == Sys::KillSwitch::State::Armed && m_coreSupervisor->runningWithoutGuardIdentity())) {
            restartCoreForGuard(resolved);
            return;
        }

        // Ranking must run before the config is built and it blocks, so hop off the UI thread.
        if (m_autoSelector->rankBeforeStart(ent, resolved.interactive, [=, this] { start(resolved); })) return;

        const auto result = Configs::BuildSingBoxConfig(ent);
        if (!result->error.isEmpty()) {
            failStart(resolved, StartOutcome::BuildFailed, MainWindowTr::tr("BuildConfig return error"), result->error);
            return;
        }
        const auto finish = [=, this](StartOutcome outcome, const QString &error) {
            if (!resolved.interactive && !error.isEmpty()) MW_show_log(error);
            runOnUiThread([=, this] { emit startFinished(resolved.serial, resolved.profileId, outcome, error); });
        };
        auto profile_start_stage2 = [=, this](const Engine::SlowOperation &slowStart) {
            libcore::LoadConfigReq req;
            req.core_config = QJsonObject2QString(result->coreConfig, true).toStdString();
            req.tun_ipv4_cidr = result->tunIPv4CIDR.toStdString();
            req.disable_stats = Configs::dataManager->settingsRepo->disable_traffic_stats;
            req.xray_config = QJsonObject2QString(result->xrayConfig, true).toStdString();
            req.need_xray = !result->xrayConfig.isEmpty();
            for (const auto &full : result->xrayFullConfigs) req.xray_full_configs.push_back(full.toStdString());
            if (req.need_xray || !req.xray_full_configs.empty()) {
                req.xray_outbound_dns_strategy = Configs::getXrayOutboundDomainStrategy().toStdString();
                if (auto selector = ent->AutoSelector(); selector != nullptr) {
                    // The idle window must outlast the probe interval or the sidecar restarts every round.
                    req.xray_lazy_start = true;
                    req.xray_idle_seconds = std::max(120, selector->intervalSec * 2);
                    // 0 = resident.
                    req.xray_full_idle_seconds = 0;
                }
            }
            if (!result->extraCoreData->path.isEmpty())
            {
                if (!Sys::KillSwitch::instance()->permitExtraCore(result->extraCoreData->path)) {
                    runOnUiThread([slowStart] { slowStart.dismiss(); });
                    MW_show_log(MainWindowTr::tr("[Kill switch] The extra core %1 could not be allowed through the kill switch.").arg(result->extraCoreData->path));
                    finish(StartOutcome::ExtraCoreBlocked, {});
                    if (!resolved.interactive) return false;
                    runOnUiThread([this] {
                        if (Sys::KillSwitch::instance()->state() == Sys::KillSwitch::State::Failed) {
                            policy()->killSwitchProblem();
                        } else {
                            policy()->extraCoreNotPermitted();
                        }
                    });
                    return false;
                }
                req.need_extra_process = true;
                req.extra_process_path = result->extraCoreData->path.toStdString();
                req.extra_process_args = result->extraCoreData->args.toStdString();
                req.extra_process_conf = result->extraCoreData->config.toStdString();
                req.extra_no_out = result->extraCoreData->noLog;
            }
            bool rpcOK;
            const QString error = API::defaultClient->Start(&rpcOK, req);
            // Queued ahead of every dialog below, so none of them can be shown over the prompt.
            runOnUiThread([slowStart] { slowStart.dismiss(); });
            if (!rpcOK) {
                finish(StartOutcome::CoreUnavailable, {});
                return false;
            }
            if (!error.isEmpty()) {
                std::function<void()> onGeoInstalled;
                if (m_options.geoAutoDownload) {
                    onGeoInstalled = [this, id = ent->id, token = m_geoRetryToken.load()] { retryAfterGeoFetch(id, token); };
                }
                if (Engine::AssetUpdates::HandleXrayGeoError(policy(), error, ent->outbound->DisplayTypeAndName(), resolved.interactive,
                                                             onGeoInstalled)) {
                    finish(StartOutcome::GeoAssetsMissing, error);
                    return false;
                }
                if (error.contains("Fwpm", Qt::CaseInsensitive)) {
                    finish(StartOutcome::StrictRouteUnavailable, error);
                    if (!resolved.interactive) return false;
                    runOnUiThread([=, this] { policy()->strictRouteUnavailable(error); });
                    return false;
                }
                if (error.contains("configure tun interface")) {
                    finish(StartOutcome::TunFailed, error);
                    if (!resolved.interactive) return false;
                    runOnUiThread([=, this] {
                        if (policy()->offerCoreReset(error)) m_coreSupervisor->resetCore();
                    });
                    return false;
                }
                finish(StartOutcome::StartFailed, error);
                if (resolved.interactive) {
                    runOnUiThread([=, this] { policy()->startFailed(resolved, StartOutcome::StartFailed, "LoadConfig return error", error); });
                }
                return false;
            }
            m_stats->beginRun(result->chainGroups);
            m_autoSelector->noteBuilt(ent, result->autoSelectors);

            Configs::dataManager->settingsRepo->UpdateStartedId(ent->id);
            // Must land after the stop this start may have run first: that stop clears the map.
            Stats::SetVpnEndpointProfiles(result->vpnEndpointProfiles);
            m_stats->setOutboundNames(result->outboundNames);
            Configs::SetRunningProfiles(result->involvedProfiles);
            setRunning(ent, !result->tunIPv4CIDR.isEmpty());
            if (Configs::dataManager->settingsRepo->spmode_system_proxy) applySystemProxy(true);

            const bool exitIsEndpoint = Stats::VpnExitEndpoint(ent) != nullptr;

            runOnUiThread([=, this] {
                m_geoRetriedProfile = -1;
                // The profile object outlives the run, so an earlier run's answer would linger, for good with the lookup off.
                ent->runningCountryInfo.clear();
                clearRestartNeeded();
                m_vpnAuth->noteRunStarted();
                emit statusChanged();
                emit profileRowsChanged({ent->id});
                emit startFinished(resolved.serial, resolved.profileId, StartOutcome::Started, {});

                // "Only route advertised network" rejects this probe.
                if (exitIsEndpoint) return;

                if (m_options.exitCountryLookup && Configs::dataManager->settingsRepo->exit_country_lookup) lookupExitCountry(ent);
            });

            return true;
        };

        if (!mu_starting.tryLock()) {
            failStart(resolved, StartOutcome::Busy, QStringLiteral("Throne"), MainWindowTr::tr("Another profile is starting..."));
            return;
        }
        if (!mu_stopping.tryLock()) {
            mu_starting.unlock();
            failStart(resolved, StartOutcome::Busy, QStringLiteral("Throne"), MainWindowTr::tr("Another profile is stopping..."));
            return;
        }
        mu_stopping.unlock();

        if (m_coreSupervisor->awaitingCoreStarted()) {
            // coreStarted for the accepted core is still to come, and the start it carries would switch away from this one.
            deferStartToCore(resolved);
            m_coreSupervisor->setStartWhenUp(resolved.profileId);
            mu_starting.unlock();
            return;
        }

        if (!Configs::dataManager->settingsRepo->core_running) {
            if (!m_coreSupervisor->canStartCore()) {
                mu_starting.unlock();
                failStart(resolved, StartOutcome::CoreUnavailable, QStringLiteral("Throne"), m_coreSupervisor->lastFailure());
                return;
            }
            deferStartToCore(resolved);
            MW_show_log(MainWindowTr::tr("Try to start the config, but the core has not listened to the RPC port, so restart it..."));
            m_coreSupervisor->restartNotListening(ent->id);
            mu_starting.unlock();
            return;
        }

        if (kConnectionManagerLinux && m_options.headless && Configs::dataManager->settingsRepo->spmode_vpn) connectionManagerWarnTunApiReach();

        const Engine::SlowOperation slowStart = resolved.interactive
            ? policy()->slowOperation(Engine::SlowOperationKind::Start, 10000)
            : Engine::SlowOperation{};

        runOnUiThread([this] {
            m_profileConnecting = true;
            syncState();
            emit busyChanged();
        });

        runOnNewThread([=, this] {
            const bool switching = runningSnapshot() != nullptr;
            if (switching) {
                m_switchingProfile = true;
                stop({false, false, true, resolved.interactive});
                mu_stopping.lock();
                mu_stopping.unlock();
                m_switchingProfile = false;
            }
            MW_show_log(">>>>>>>> " + MainWindowTr::tr("Starting profile %1").arg(ent->outbound->DisplayTypeAndName()));
            const bool started = profile_start_stage2(slowStart);
            const auto markerAt = MonotonicMs();
            if (!started) {
                MW_show_log("<<<<<<<< " + MainWindowTr::tr("Failed to start profile %1").arg(ent->outbound->DisplayTypeAndName()));
            }
            mu_starting.unlock();
            runOnUiThread([=, this] {
                slowStart.dismiss();
                m_profileConnecting = false;
                syncState();
                emit busyChanged();
                if (started) {
                    emit runMarker(markerAt, switching ? RunMarker::Switched : RunMarker::Started,
                                   ent->outbound ? ent->outbound->DisplayName() : ent->name);
                } else if (switching) {
                    emit runMarker(markerAt, RunMarker::Stopped, {});
                }
            });
        });
    }

    void ConnectionManager::stop(const StopOptions &options) {
        const bool crash = options.crash;
        const bool block = options.block;
        const bool manual = options.manual;
        const bool interactive = options.interactive;
        const int rpcTimeoutMs = options.rpcTimeoutMs;
        const bool skipTests = options.skipTests;

        if (manual) ++m_geoRetryToken;
        const auto current = runningSnapshot();
        if (current == nullptr) {
            // A crashed profile waits for the restarted core with nothing running; a user stop cancels that.
            if (manual && !crash) runOnUiThread([this] { m_coreSupervisor->cancelResume(); });
            return;
        }
        const auto id = current->id;

        auto profile_stop_stage2 = [=,this](const Engine::SlowOperation &slowStop) {
            if (!skipTests && m_tests->isTestingCurrent()) {
                bool ok;
                API::defaultClient->StopTests(&ok);
                if (!ok) MW_show_log("Failed to stop profile tests!");
            }
            if (!crash) {
                bool rpcOK;
                const QString error = API::defaultClient->Stop(&rpcOK, rpcTimeoutMs);
                runOnUiThread([slowStop] { slowStop.dismiss(); });
                if (rpcOK && !error.isEmpty()) {
                    if (interactive) {
                        runOnUiThread([=,this] { policy()->stopFailed(error); });
                    } else {
                        MW_show_log(MainWindowTr::tr("Stop return error") + ": " + error);
                    }
                    return false;
                } else if (!rpcOK) {
                    return false;
                }
            }
            if (Configs::dataManager->settingsRepo->spmode_system_proxy) applySystemProxy(false);
            return true;
        };

        if (!mu_stopping.tryLock()) {
            return;
        }

        // stop() is reached from a worker thread as well as the UI one.
        m_stats->clearConnectionList();

        runOnUiThread([this] {
            m_profileDisconnecting = true;
            syncState();
            emit busyChanged();
        });

        m_autoSelector->noteStopped();

        runOnNewThread([=, this] {
            m_stats->endRun();

            // runOnUiThread is a no-op before qApp exists, so the teardown must not chase this.
            Engine::SlowOperation slowStop;
            if (interactive) {
                runOnUiThread([this, &slowStop] {
                    slowStop = policy()->slowOperation(Engine::SlowOperationKind::Stop, 5000);
                }, true);
            }

            // Snapshot: `running` is cleared below and a racing start can reassign it.
            const auto stopping = runningSnapshot();
            if (stopping != nullptr) {
                MW_show_log(">>>>>>>> " + MainWindowTr::tr("Stopping profile %1").arg(stopping->outbound->DisplayTypeAndName()));
            }
            if (!profile_stop_stage2(slowStop)) {
                MW_show_log("<<<<<<<< " + MainWindowTr::tr("Failed to stop, please restart the program."));
            }

            if (manual) Configs::dataManager->settingsRepo->UpdateStartedId(Configs::NoProfileId);
            // Cleared here too: a restart prompt armed before the UI-thread teardown would have no id to start.
            Configs::ClearRunningProfiles();
            m_stats->setOutboundNames({});
            setRunning(nullptr);
            const auto stoppedAt = MonotonicMs();

            runOnUiThread([=, this, &slowStop] {
                slowStop.dismiss();
                if (!m_switchingProfile) emit runMarker(stoppedAt, RunMarker::Stopped, {});

                m_profileDisconnecting = false;
                clearRestartNeeded();
                m_vpnAuth->noteRunStopped(id, manual);
                syncState();
                emit statusChanged();
                emit profileRowsChanged({id});

                mu_stopping.unlock();
                emit stopFinished(id);
            }, true);
        }, block);
    }

    bool ConnectionManager::restartStartedProfile(bool interactive, quint64 restartSerial) {
        const int startedId = Configs::dataManager->settingsRepo->started_id;
        if (startedId < 0) return false;
        start(StartRequest{startedId, interactive, restartSerial});
        return true;
    }

    void ConnectionManager::applyPendingRestart() {
        const int startedId = Configs::dataManager->settingsRepo->started_id;
        clearRestartNeeded();
        if (startedId >= 0) start(StartRequest{startedId});
    }

    void ConnectionManager::failStart(const StartRequest &request, StartOutcome outcome, const QString &title, const QString &error) {
        emit startFinished(request.serial, request.profileId, outcome, error);
        if (request.interactive) {
            policy()->startFailed(request, outcome, title, error);
        } else {
            MW_show_log(title + ": " + error);
        }
    }

    void ConnectionManager::deferStartToCore(const StartRequest &request) {
        if (m_coreStartRequest.profileId >= 0) {
            emit startFinished(m_coreStartRequest.serial, m_coreStartRequest.profileId, StartOutcome::Superseded, {});
        }
        m_coreStartRequest = request;
    }

    void ConnectionManager::setBootStart(const StartRequest &request) {
        ENGINE_CHECK_MAIN_THREAD();
        m_bootSerial = request.serial;
        deferStartToCore(request);
    }

    void ConnectionManager::onCoreStarted(int profileId) {
        auto &settings = Configs::dataManager->settingsRepo;
        m_guardCoreRestart.invalidate();
        // The core may just have been given root, which the guard needs too.
        if (Sys::KillSwitch::instance()->failedForPrivileges()) Sys::KillSwitch::instance()->apply();
        QString tunHoldWhy;
        if (m_options.headless) {
            const bool wantTun = settings->remember_tun || settings->flag_restart_tun_on;
            if (wantTun) {
                setTun(true, ModeChange{.save = false, .interactive = false, .restart = false});
                settings->flag_restart_tun_on = false;
            }
            // setTun's return value is not success, so the hold is judged by the resulting state.
            m_tunRestoreHeld = m_options.tunRequired && wantTun && !settings->spmode_vpn;
            if (m_tunRestoreHeld) {
                tunHoldWhy = tunRestoreHoldReason();
                MW_show_log(QStringLiteral("[Tun] The remembered Tun mode could not be restored: %1 (degraded: tun_unavailable).")
                                .arg(tunHoldWhy));
            }
        } else {
            if (settings->remember_enable && settings->remember_system_proxy) {
                setSystemProxy(true, ModeChange{.save = false});
            }
            if ((settings->remember_enable && settings->remember_tun) || settings->flag_restart_tun_on) {
                setTun(true, ModeChange{.save = settings->flag_restart_tun_on});
                settings->flag_restart_tun_on = false;
            }
        }
        {
            const int id = profileId;
            const auto request = std::exchange(m_coreStartRequest, StartRequest{});
            const quint64 bootSerial = std::exchange(m_bootSerial, 0);
            if (request.profileId >= 0 && request.profileId != id) {
                emit startFinished(request.serial, request.profileId, StartOutcome::Superseded, {});
            }
            if (id >= 0) {
                const StartRequest next = request.profileId == id ? request : StartRequest{id};
                // Any other start (crash resume, deferred API start) runs proxy-only, as it would against a live core.
                if (m_tunRestoreHeld && next.serial != 0 && next.serial == bootSerial) {
                    const auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
                    MW_show_log(QStringLiteral("[Tun] Not starting %1 without Tun; turn Tun off to run it proxy-only.")
                                    .arg(ent != nullptr && ent->outbound ? ent->outbound->DisplayTypeAndName() : QString::number(id)));
                    emit startFinished(next.serial, id, StartOutcome::TunFailed,
                                       QStringLiteral("Tun could not be restored (%1); the profile was not started without it.")
                                           .arg(tunHoldWhy));
                } else {
                    start(next);
                }
            }
        }
        emit statusChanged();
    }

    void ConnectionManager::onCoreGaveUp(const QString &reason) {
        // Otherwise a start within the window would wait on a guard restart that is not coming.
        m_guardCoreRestart.invalidate();
        const auto request = std::exchange(m_coreStartRequest, StartRequest{});
        if (request.profileId >= 0) emit startFinished(request.serial, request.profileId, StartOutcome::CoreUnavailable, reason);
    }

    void ConnectionManager::retryAfterGeoFetch(int profileId, quint64 token) {
        ENGINE_CHECK_MAIN_THREAD();
        if (token != m_geoRetryToken.load()) return;
        if (std::exchange(m_geoRetriedProfile, profileId) == profileId) return;
        if (Configs::dataManager->settingsRepo->prepare_exit || connectionState() != ConnectionState::Idle ||
            m_lastProfileId != profileId) {
            return;
        }
        MW_show_log(QStringLiteral("Geo assets installed; starting profile %1 again.").arg(profileId));
        start(StartRequest{profileId, false});
    }

    void ConnectionManager::lookupExitCountry(const std::shared_ptr<Configs::Profile> &ent) {
        ENGINE_CHECK_MAIN_THREAD();
        const QString provider = Configs::dataManager->settingsRepo->exit_lookup_provider;
        // Off the UI thread: HttpGet spins a nested event loop that would re-enter API dispatch.
        runOnNewThread([=, this] {
            QString error;
            const auto info = Configs_network::LookupExit(provider, &error);
            if (!info.hasCountry()) {
                LOG_DEBUG(QStringLiteral("[ExitLookup] no exit country: %1").arg(error));
                return;
            }
            const QString text = Configs_network::ExitLocationText(info);
            runOnUiThread([=, this] {
                if (runningSnapshot() != ent) return;
                ent->runningCountryInfo = text;
                emit statusChanged();
            });
        });
    }

    int ConnectionManager::resolveLastProfile() const {
        const auto &settings = Configs::dataManager->settingsRepo;
        for (const int id : {settings->started_id, m_lastProfileId, settings->remember_id}) {
            if (id >= 0 && Configs::dataManager->profilesRepo->GetProfile(id) != nullptr) return id;
        }
        return -1;
    }

    int ConnectionManager::lastProfileId() const {
        return m_lastProfileId;
    }

    ConnectionState ConnectionManager::connectionState() const {
        if (m_profileConnecting) return ConnectionState::Connecting;
        if (m_profileDisconnecting) return ConnectionState::Stopping;
        return runningSnapshot() != nullptr ? ConnectionState::Running : ConnectionState::Idle;
    }

    std::shared_ptr<Configs::Profile> ConnectionManager::runningSnapshot() const {
        QMutexLocker locker(&m_runningMutex);
        return m_running;
    }

    void ConnectionManager::setRunning(std::shared_ptr<Configs::Profile> profile, bool tun) {
        QMutexLocker locker(&m_runningMutex);
        m_running.swap(profile);
        m_runningTun = m_running != nullptr && tun;
    }

    bool ConnectionManager::runningTun() const {
        QMutexLocker locker(&m_runningMutex);
        return m_runningTun;
    }

    quint64 ConnectionManager::nextStartSerial() {
        return ++m_startSerial;
    }

    void ConnectionManager::syncState() {
        ENGINE_CHECK_MAIN_THREAD();
        if (const auto connState = connectionState(); connState != m_lastConnectionState) {
            m_lastConnectionState = connState;
            // Whether a profile runs decides the dns-in port probe, and a Tun of ours may just have gone.
            m_tunBlockedAtMs = 0;
            emit connectionStateChanged(connState);
        }
    }
} // namespace Engine
