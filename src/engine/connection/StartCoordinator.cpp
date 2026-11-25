#include "include/engine/connection/StartCoordinator.h"

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
#include "include/engine/connection/EngineState.h"
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

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonObject>
#include <QPointer>
#include <QSemaphore>
#include <QThread>

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

namespace {
#ifdef Q_OS_LINUX
    constexpr bool kStartCoordinatorLinux = true;
#else
    constexpr bool kStartCoordinatorLinux = false;
#endif
#ifdef Q_OS_WIN
    constexpr bool kStartCoordinatorWindows = true;
#else
    constexpr bool kStartCoordinatorWindows = false;
#endif

    // Main thread, headless Linux, as a start brings the Tun up: once per boot each, the Tun settings that cut API clients off.
    void startCoordinatorWarnTunApiReach() {
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

    // Every start and stop body, one at a time in queue order, on a thread that is never deleted (like DS_cores).
    void startCoordinatorQueue(const std::function<void()> &job) {
        static QObject *const context = [] {
            auto *thread = new QThread;
            thread->start();
            auto *object = new QObject;
            object->moveToThread(thread);
            return object;
        }();
        QMetaObject::invokeMethod(context, job, Qt::QueuedConnection);
    }

    // Ops thread: runs teardown on the main thread and waits for it without an event loop; returns at once without an app.
    void startCoordinatorRunOnMain(const std::function<void()> &teardown) {
        auto *app = QCoreApplication::instance();
        if (app == nullptr) return;
        const auto returned = std::make_shared<QSemaphore>();
        QMetaObject::invokeMethod(app, [teardown, returned] {
            teardown();
            returned->release();
        }, Qt::QueuedConnection);
        returned->acquire();
    }
} // namespace

namespace Engine {
    StartCoordinator::StartCoordinator(std::shared_ptr<EngineState> state, EnginePolicy *policy, CoreSupervisor *core, StatsService *stats,
                                       const RuntimeOptions &options, ModeHooks hooks, QObject *parent)
        : QObject(parent), m_state(std::move(state)), m_policy(policy), m_core(core), m_stats(stats), m_options(options),
          m_hooks(std::move(hooks)) {}

    void StartCoordinator::attach(TestRunner *tests, AutoSelectorController *autoSelector, VpnAuthController *vpnAuth) {
        m_tests = tests;
        m_autoSelector = autoSelector;
        m_vpnAuth = vpnAuth;
    }

    void StartCoordinator::requestStart(const StartRequest &request) {
        ENGINE_CHECK_MAIN_THREAD();
        const quint64 intent = ++m_intentToken;
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

        m_state->lastTarget = ent->id;

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
            supersedePending(m_pending.killSwitch, resolved);
            // One callback per Arming wait: a user stop can empty the slot while it is registered.
            if (!std::exchange(m_killSwitchWaiting, true)) {
                killSwitch->whenSettled(this, [this](bool) {
                    m_killSwitchWaiting = false;
                    if (const auto next = std::exchange(m_pending.killSwitch, StartRequest{}); next.profileId >= 0) requestStart(next);
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
            (killSwitch->state() == Sys::KillSwitch::State::Armed && m_core->runningWithoutGuardIdentity())) {
            deferToCore(resolved, Wake::Guard);
            return;
        }

        // Before the rank and the build, so a refused start does neither.
        if (startInFlight()) {
            failStart(resolved, StartOutcome::Busy, QStringLiteral("Throne"), MainWindowTr::tr("Another profile is starting..."));
            return;
        }
        if (m_stopInFlight) {
            failStart(resolved, StartOutcome::Busy, QStringLiteral("Throne"), MainWindowTr::tr("Another profile is stopping..."));
            return;
        }

        emit startDispatching(resolved.serial, resolved.profileId);

        // Ranking must run before the config is built and it blocks, so hop off the UI thread.
        const auto resume = [=, this] {
            if (intent != m_intentToken.load()) {
                emit startFinished(resolved.serial, resolved.profileId, StartOutcome::Superseded, {});
                return;
            }
            requestStart(resolved);
        };
        const auto wanted = [this, intent] { return intent == m_intentToken.load(); };
        if (m_autoSelector->rankBeforeStart(ent, resolved.interactive, wanted, resume)) return;

        if (m_startInFlight.exchange(true)) {
            failStart(resolved, StartOutcome::Busy, QStringLiteral("Throne"), MainWindowTr::tr("Another profile is starting..."));
            return;
        }
        // The previous start's body has ended, but its end has still to run on this thread.
        if (m_state->connecting) {
            m_startInFlight = false;
            failStart(resolved, StartOutcome::Busy, QStringLiteral("Throne"), MainWindowTr::tr("Another profile is starting..."));
            return;
        }
        if (m_stopInFlight) {
            m_startInFlight = false;
            failStart(resolved, StartOutcome::Busy, QStringLiteral("Throne"), MainWindowTr::tr("Another profile is stopping..."));
            return;
        }

        // Restart reasons noted after this point are newer than the config this start runs.
        const quint64 hintEpoch = m_state->restartEpoch;
        // Claimed through the build as through the body: another start is refused and a stop is held.
        m_state->connecting = true;
        syncState();
        emit busyChanged();
        runOnNewThread([=, this] {
            const auto result = Configs::BuildSingBoxConfig(ent);
            QMetaObject::invokeMethod(this, [=, this] { startBuilt(resolved, ent, intent, hintEpoch, result); }, Qt::QueuedConnection);
        });
    }

    void StartCoordinator::startBuilt(const StartRequest &resolved, const std::shared_ptr<Configs::Profile> &ent, quint64 intent,
                                      quint64 hintEpoch, const std::shared_ptr<Configs::BuildConfigResult> &result) {
        ENGINE_CHECK_MAIN_THREAD();
        const bool exiting = Configs::dataManager->settingsRepo->prepare_exit;
        if (exiting || (m_heldStop.has_value() && m_heldStop->user)) {
            m_startInFlight = false;
            endStart(false);
            emit startFinished(resolved.serial, resolved.profileId, exiting ? StartOutcome::Exiting : StartOutcome::Superseded, {});
            return;
        }
        if (!result->error.isEmpty()) {
            m_startInFlight = false;
            endStart(false);
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
            req.xray_config = QJsonObject2QString(result->xrayConfig, true).toStdString();
            req.need_xray = !result->xrayConfig.isEmpty();
            for (const auto &full : result->xrayFullConfigs) req.xray_full_configs.push_back(full.toStdString());
            if (req.need_xray || !req.xray_full_configs.empty()) {
                req.xray_outbound_dns_strategy = result->xrayDnsStrategy.toStdString();
                if (auto selector = result->autoSelectors.isEmpty() || result->autoSelectors.first().profile == nullptr
                                        ? nullptr : result->autoSelectors.first().profile->AutoSelector();
                    selector != nullptr) {
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
                    // The request's token: a stop held during this start has moved it on already.
                    onGeoInstalled = [this, id = ent->id, intent] { retryAfterGeoFetch(id, intent); };
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
                    if (error.contains("open existing adapter")) {
                        runOnUiThread([=, this] {
                            policy()->offerLeftoverTunAdapterRetry(error, [=, this, self = QPointer<StartCoordinator>(this)](bool retry) {
                                if (!retry || !self) return;
                                if (Configs::dataManager->settingsRepo->prepare_exit || m_state->connectionState() != ConnectionState::Idle) return;
                                requestStart(StartRequest{ent->id});
                            });
                        });
                        return false;
                    }
                    runOnUiThread([=, this] {
                        policy()->offerCoreReset(error, [this, self = QPointer<StartCoordinator>(this)](bool reset) {
                            if (reset && self) m_core->resetCore();
                        });
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

            m_state->setResumeTarget(ent->id, true);
            // Must land after the stop this start may have run first: that stop clears the map.
            Stats::SetVpnEndpointProfiles(result->vpnEndpointProfiles);
            m_stats->setOutboundNames(result->outboundNames);
            Configs::SetRunningProfiles(result->involvedProfiles);
            Configs::SetRunningRoute(result->routeId);
            Configs::SetRunningRouteRules({result->routeRuleOrigins, result->outboundNames, result->vpnEndpointProfiles});
            m_state->setRunning(ent, !result->tunIPv4CIDR.isEmpty());
            if (Configs::dataManager->settingsRepo->spmode_system_proxy) m_hooks.applySystemProxy(true);

            const bool exitIsEndpoint = Stats::VpnExitEndpoint(ent) != nullptr;

            runOnUiThread([=, this] {
                m_geoRetriedProfile = -1;
                // The profile object outlives the run, so an earlier run's answer would linger, for good with the lookup off.
                ent->runningCountryInfo.clear();
                clearRestartNotedBy(hintEpoch);
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

        // Crash stops are never held, so one can be accepted while the config builds.
        if (m_stopInFlight) {
            m_startInFlight = false;
            endStart(false);
            failStart(resolved, StartOutcome::Busy, QStringLiteral("Throne"), MainWindowTr::tr("Another profile is stopping..."));
            return;
        }

        if (m_core->awaitingCoreStarted()) {
            m_startInFlight = false;
            endStart(false);
            // coreStarted for the accepted core is still to come, and the start it carries would switch away from this one.
            deferToCore(resolved, Wake::AlreadyComing);
            return;
        }

        if (!Configs::dataManager->settingsRepo->core_running) {
            m_startInFlight = false;
            endStart(false);
            if (!m_core->canStartCore()) {
                failStart(resolved, StartOutcome::CoreUnavailable, QStringLiteral("Throne"), m_core->lastFailure());
                return;
            }
            // A core that is still coming up takes the start instead of being restarted under it.
            deferToCore(resolved, m_core->state() == CoreState::Starting ? Wake::Starting : Wake::NotListening);
            return;
        }

        if (kStartCoordinatorLinux && m_options.headless && Configs::dataManager->settingsRepo->spmode_vpn) startCoordinatorWarnTunApiReach();

        // Wintun spends 15 s on a leftover adapter before failing; a restart from the prompt inside that window strands a new one.
        const int slowStartMs = kStartCoordinatorWindows && !result->tunIPv4CIDR.isEmpty() ? 20000 : 10000;
        const Engine::SlowOperation slowStart = resolved.interactive
            ? policy()->slowOperation(Engine::SlowOperationKind::Start, slowStartMs)
            : Engine::SlowOperation{};

        // Queued first, so the previous profile's teardown has run before the Start RPC.
        const bool switching = m_state->running() != nullptr;
        if (switching) acceptStop({.manual = true, .interactive = resolved.interactive, .keepIntent = true}, true, {});

        startCoordinatorQueue([=, this] {
            MW_show_log(">>>>>>>> " + MainWindowTr::tr("Starting profile %1").arg(ent->outbound->DisplayTypeAndName()));
            const bool started = profile_start_stage2(slowStart);
            if (!started) {
                MW_show_log("<<<<<<<< " + MainWindowTr::tr("Failed to start profile %1").arg(ent->outbound->DisplayTypeAndName()));
            }
            m_startInFlight = false;
            runOnUiThread([=, this] {
                slowStart.dismiss();
                endStart(!started);
            });
        });
    }

    void StartCoordinator::endStart(bool clearReasons) {
        ENGINE_CHECK_MAIN_THREAD();
        m_state->connecting = false;
        // Before syncState(), so the state goes from Connecting straight to Stopping.
        if (auto held = std::exchange(m_heldStop, std::nullopt)) {
            std::function<void()> done;
            if (!held->done.isEmpty()) {
                done = [callbacks = held->done] {
                    for (const auto &callback : callbacks) callback();
                };
            }
            decideStop(held->options, done);
        }
        if (clearReasons) clearRestartNeeded();
        syncState();
        emit busyChanged();
    }

    void StartCoordinator::requestStop(const StopOptions &options) {
        if (options.manual && !options.keepIntent) ++m_intentToken;
        auto *app = QCoreApplication::instance();
        if (app == nullptr) return;
        if (QThread::currentThread() == app->thread()) {
            if (!options.block) {
                decideStop(options, {});
                return;
            }
            const auto finished = std::make_shared<bool>(false);
            QEventLoop loop;
            const QPointer<QEventLoop> waiter(&loop);
            decideStop(options, [finished, waiter] {
                *finished = true;
                if (waiter) waiter->quit();
            });
            if (!*finished) loop.exec();
            return;
        }
        if (!options.block) {
            QMetaObject::invokeMethod(this, [this, options] { decideStop(options, {}); }, Qt::QueuedConnection);
            return;
        }
        QEventLoop loop;
        const QPointer<QEventLoop> waiter(&loop);
        QMetaObject::invokeMethod(this, [this, options, waiter] {
            decideStop(options, [waiter] {
                if (waiter) QMetaObject::invokeMethod(waiter.data(), &QEventLoop::quit, Qt::QueuedConnection);
            });
        }, Qt::QueuedConnection);
        loop.exec();
    }

    bool StartCoordinator::stopPending() const {
        return m_heldStop.has_value();
    }

    bool StartCoordinator::startInFlight() const {
        return m_startInFlight.load() || m_state->connecting;
    }

    void StartCoordinator::decideStop(const StopOptions &options, const std::function<void()> &done) {
        const bool exiting = Configs::dataManager->settingsRepo->prepare_exit;
        const bool userStop = options.manual && !options.keepIntent && !options.crash;
        if (!exiting && startInFlight() && (userStop || (options.block && !options.crash))) {
            holdStop(options, done, userStop);
            return;
        }
        // Also with nothing running: a crashed profile may wait for the restarted core.
        if (userStop) applyUserStop();
        if (m_state->running() == nullptr) {
            if (done) done();
            return;
        }
        if (m_stopInFlight) {
            if (!done) return;
            // Crash stops and the exit's stop never wait on another stop's RPC.
            if (options.crash || exiting) {
                done();
            } else {
                m_stopWaiters.append(done);
            }
            return;
        }
        acceptStop(options, false, done);
    }

    void StartCoordinator::holdStop(const StopOptions &options, const std::function<void()> &done, bool userStop) {
        const bool first = !m_heldStop.has_value();
        if (first) {
            m_heldStop = HeldStop{options, {}};
        } else {
            m_heldStop->options.manual = m_heldStop->options.manual || options.manual;
            m_heldStop->options.interactive = m_heldStop->options.interactive || options.interactive;
        }
        if (done) m_heldStop->done.append(done);
        if (userStop) {
            m_heldStop->user = true;
            applyUserStop();
        }
        if (!first) return;
        MW_show_log(MainWindowTr::tr("Stop requested; the profile stops once its start finishes."));
        emit busyChanged();
    }

    void StartCoordinator::applyUserStop() {
        m_state->setResumeTarget(Configs::NoProfileId, true);
        supersedePending(m_pending.killSwitch, {});
        supersedePending(m_pending.core, {});
        // The core's own resume as well: a parked guard deferral would start the profile once the core is back.
        m_core->cancelResume();
    }

    void StartCoordinator::acceptStop(const StopOptions &options, bool switching, const std::function<void()> &done) {
        ENGINE_CHECK_MAIN_THREAD();
        const bool crash = options.crash;
        const bool manual = options.manual;
        const bool interactive = options.interactive;
        const int rpcTimeoutMs = options.rpcTimeoutMs;
        const bool skipTests = options.skipTests;
        const auto id = m_state->running()->id;

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
            if (Configs::dataManager->settingsRepo->spmode_system_proxy) m_hooks.applySystemProxy(false);
            return true;
        };

        m_stopInFlight = true;
        // Here rather than after the Stop RPC: a core that dies during it would resume this profile.
        if (manual) m_state->setResumeTarget(Configs::NoProfileId, true);

        m_stats->clearConnectionList();

        runOnUiThread([this] {
            m_state->disconnecting = true;
            syncState();
            emit busyChanged();
        });

        m_autoSelector->noteStopped();

        const Engine::SlowOperation slowStop = interactive
            ? policy()->slowOperation(Engine::SlowOperationKind::Stop, 5000)
            : Engine::SlowOperation{};

        startCoordinatorQueue([=, this] {
            m_stats->endRun();

            // Snapshot: `running` is cleared below and a racing start can reassign it.
            const auto stopping = m_state->running();
            if (stopping != nullptr) {
                MW_show_log(">>>>>>>> " + MainWindowTr::tr("Stopping profile %1").arg(stopping->outbound->DisplayTypeAndName()));
            }
            if (!profile_stop_stage2(slowStop)) {
                MW_show_log("<<<<<<<< " + MainWindowTr::tr("Failed to stop, please restart the program."));
            }

            // Cleared here too: a restart prompt armed before the UI-thread teardown would have no id to start.
            Configs::ClearRunningProfiles();
            m_stats->setOutboundNames({});
            m_state->setRunning(nullptr, false);

            startCoordinatorRunOnMain([=, this] {
                slowStop.dismiss();
                m_state->disconnecting = false;
                // A switch leaves the reasons to its start.
                if (!switching) clearRestartNeeded();
                m_vpnAuth->noteRunStopped(id, manual, switching);
                syncState();
                emit statusChanged();
                emit profileRowsChanged({id});

                m_stopInFlight = false;
                emit stopFinished(id);
                if (done) done();
                for (const auto &joined : std::exchange(m_stopWaiters, {})) joined();
            });
        });
    }

    bool StartCoordinator::restart(int profileId, bool interactive, quint64 serial) {
        if (profileId < 0) return false;
        requestStart(StartRequest{profileId, interactive, serial});
        return true;
    }

    bool StartCoordinator::restartResumeTarget(bool interactive, quint64 serial) {
        return restart(m_state->resumeTarget(), interactive, serial);
    }

    void StartCoordinator::applyPendingRestart() {
        // A requested restart leaves the hint to the start: it stays unless that start covers it.
        if (!restart(m_state->resumeTarget(), true, 0)) clearRestartNeeded();
    }

    void StartCoordinator::failStart(const StartRequest &request, StartOutcome outcome, const QString &title, const QString &error) {
        emit startFinished(request.serial, request.profileId, outcome, error);
        if (request.interactive) {
            policy()->startFailed(request, outcome, title, error);
        } else {
            MW_show_log(title + ": " + error);
        }
    }

    void StartCoordinator::supersedePending(StartRequest &slot, const StartRequest &next) {
        if (slot.profileId >= 0) emit startFinished(slot.serial, slot.profileId, StartOutcome::Superseded, {});
        slot = next;
    }

    void StartCoordinator::deferToCore(const StartRequest &request, Wake wake) {
        bool woken = true;
        switch (wake) {
            case Wake::AlreadyComing:
            case Wake::Guard:
                woken = m_core->setStartWhenUp(request.profileId);
                break;
            case Wake::NotListening:
                woken = m_core->restartNotListening(request.profileId);
                if (woken) {
                    MW_show_log(MainWindowTr::tr("Try to start the config, but the core has not listened to the RPC port, so restart it..."));
                }
                break;
            case Wake::Starting:
                woken = m_core->deferToStartingCore(request.profileId);
                break;
            case Wake::Boot:
                // The caller hands the id to CoreSupervisor::start().
                break;
        }
        if (!woken) {
            emit startFinished(request.serial, request.profileId, StartOutcome::CoreUnavailable, m_core->lastFailure());
            return;
        }
        if (wake == Wake::Boot) m_pending.bootSerial = request.serial;
        supersedePending(m_pending.core, request);
        if (wake == Wake::Guard && !guardRestartPending()) {
            m_pending.guardWindow.start();
            MW_show_log(MainWindowTr::tr("[Kill switch] Restarting the core so that its own traffic passes the kill switch..."));
            m_core->restartForGuard();
        }
    }

    void StartCoordinator::setBootStart(const StartRequest &request) {
        ENGINE_CHECK_MAIN_THREAD();
        deferToCore(request, Wake::Boot);
    }

    void StartCoordinator::noteCoreUp() {
        m_pending.guardWindow.invalidate();
    }

    void StartCoordinator::takeCoreStart(int profileId, bool tunRestoreHeld, const QString &tunHoldWhy) {
        const int id = profileId;
        const auto request = std::exchange(m_pending.core, StartRequest{});
        const quint64 bootSerial = std::exchange(m_pending.bootSerial, 0);
        if (request.profileId >= 0 && request.profileId != id) {
            emit startFinished(request.serial, request.profileId, StartOutcome::Superseded, {});
        }
        if (id >= 0) {
            const StartRequest next = request.profileId == id ? request : StartRequest{id, false};
            // Any other start (crash resume, deferred API start) runs proxy-only, as it would against a live core.
            if (tunRestoreHeld && next.serial != 0 && next.serial == bootSerial) {
                const auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
                MW_show_log(QStringLiteral("[Tun] Not starting %1 without Tun; turn Tun off to run it proxy-only.")
                                .arg(ent != nullptr && ent->outbound ? ent->outbound->DisplayTypeAndName() : QString::number(id)));
                emit startFinished(next.serial, id, StartOutcome::TunFailed,
                                   QStringLiteral("Tun could not be restored (%1); the profile was not started without it.")
                                       .arg(tunHoldWhy));
            } else {
                requestStart(next);
            }
        }
    }

    void StartCoordinator::onCoreGaveUp(const QString &reason) {
        // Otherwise a start within the window would wait on a guard restart that is not coming.
        m_pending.guardWindow.invalidate();
        const auto request = std::exchange(m_pending.core, StartRequest{});
        if (request.profileId >= 0) emit startFinished(request.serial, request.profileId, StartOutcome::CoreUnavailable, reason);
    }

    void StartCoordinator::onGuardArmed() {
        if (guardRestartPending() || !m_core->runningWithoutGuardIdentity()) return;
        StartRequest request{m_state->resumeTarget(), false};
        if (m_core->awaitingCoreStarted() && m_core->startWhenUp() >= 0) {
            // The start waiting for the probing core has not run yet; the guard restart carries it over.
            request = m_pending.core.profileId >= 0 ? std::exchange(m_pending.core, StartRequest{})
                                                    : StartRequest{m_core->startWhenUp(), false};
        }
        deferToCore(request, Wake::Guard);
    }

    bool StartCoordinator::guardRestartPending() const {
        return m_pending.guardWindow.isValid() && !m_pending.guardWindow.hasExpired(kGuardRestartWindowMs);
    }

    void StartCoordinator::noteRestartNeeded(RestartReason reason) {
        ENGINE_CHECK_MAIN_THREAD();
        // A start in flight may have built its config before this change, and has no resume target yet on a first start.
        if (m_state->resumeTarget() < 0 && !startInFlight()) return;
        m_state->restartNotedAt[reason] = ++m_state->restartEpoch;
        if (m_state->restartReasons.contains(reason)) return;
        m_state->restartReasons << reason;
        emit restartReasonsChanged();
    }

    void StartCoordinator::clearRestartNeeded() {
        clearRestartNotedBy(m_state->restartEpoch);
    }

    void StartCoordinator::clearRestartNotedBy(quint64 epoch) {
        QList<RestartReason> kept;
        for (const auto reason : std::as_const(m_state->restartReasons)) {
            if (m_state->restartNotedAt.value(reason) > epoch) {
                kept << reason;
            } else {
                m_state->restartNotedAt.remove(reason);
            }
        }
        if (kept.size() == m_state->restartReasons.size()) return;
        m_state->restartReasons = kept;
        emit restartReasonsChanged();
    }

    QList<RestartReason> StartCoordinator::restartReasons() const {
        return m_state->restartReasons;
    }

    bool StartCoordinator::restartPending() const {
        return !m_state->restartReasons.isEmpty();
    }

    void StartCoordinator::retryAfterGeoFetch(int profileId, quint64 token) {
        ENGINE_CHECK_MAIN_THREAD();
        if (token != m_intentToken.load()) return;
        if (std::exchange(m_geoRetriedProfile, profileId) == profileId) return;
        if (Configs::dataManager->settingsRepo->prepare_exit || m_state->connectionState() != ConnectionState::Idle ||
            m_state->lastTarget != profileId) {
            return;
        }
        MW_show_log(QStringLiteral("Geo assets installed; starting profile %1 again.").arg(profileId));
        requestStart(StartRequest{profileId, false});
    }

    void StartCoordinator::lookupExitCountry(const std::shared_ptr<Configs::Profile> &ent) {
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
                if (m_state->running() != ent) return;
                ent->runningCountryInfo = text;
                emit statusChanged();
            });
        });
    }

    int StartCoordinator::resolveLastProfile() const {
        const auto &settings = Configs::dataManager->settingsRepo;
        for (const int id : {m_state->resumeTarget(), m_state->lastTarget, settings->remember_id}) {
            if (id >= 0 && Configs::dataManager->profilesRepo->GetProfile(id) != nullptr) return id;
        }
        return -1;
    }

    quint64 StartCoordinator::nextStartSerial() {
        return ++m_startSerial;
    }

    void StartCoordinator::syncState() {
        ENGINE_CHECK_MAIN_THREAD();
        if (const auto connState = m_state->connectionState(); connState != m_state->published) {
            m_state->published = connState;
            emit connectionStateChanged(connState);
        }
    }
} // namespace Engine
