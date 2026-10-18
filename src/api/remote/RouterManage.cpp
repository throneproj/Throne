#include "include/api/remote/Router.hpp"

#include <QJsonArray>
#include <QUrl>

#include <algorithm>
#include <memory>

#include "include/configs/sub/GroupUpdater.hpp"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineControl.h"
#include "include/engine/services/AssetUpdates.h"
#include "include/engine/services/GroupService.h"
#include "include/engine/services/ImportService.h"
#include "include/engine/services/ProfileService.h"
#include "include/engine/services/ServiceTypes.h"
#include "include/engine/services/SettingsRegistry.h"
#include "include/global/Configs.hpp"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Validation.hpp"

namespace RemoteApi::RouterManageImpl {
    constexpr int kImportWaitSec = 60;
    constexpr int kGroupUpdateWaitSec = 60;
    constexpr int kGroupsUpdateWaitSec = 120;
    constexpr int kDeleteWaitSec = 30;
    constexpr int kRuleSetsWaitSec = 90;
    constexpr int kGeoWaitSec = 300;
    constexpr int kNothingImportedDetails = 5;

    using Failure = Subscription::UpdateOutcome::Failure;

    Response busyConnection() {
        return Response::Error(409, QStringLiteral("busy"), QStringLiteral("A profile is starting or stopping"));
    }

    Response exiting() {
        return Response::Error(503, QStringLiteral("exiting"), QStringLiteral("Throne is shutting down"));
    }

    // Only http and https: a file:// URL would make Throne read local files for a remote client.
    bool isHttpUrl(const QString &text) {
        const QUrl url(text, QUrl::StrictMode);
        if (!url.isValid() || url.host().isEmpty()) return false;
        return url.scheme().compare(QLatin1String("http"), Qt::CaseInsensitive) == 0 ||
               url.scheme().compare(QLatin1String("https"), Qt::CaseInsensitive) == 0;
    }

    bool anyRunningUses(const QList<int> &profileIds) {
        return std::any_of(profileIds.cbegin(), profileIds.cend(), [](int id) { return Configs::RunningUsesProfile(id); });
    }

    QString failureText(const Subscription::UpdateOutcome &outcome) {
        if (!outcome.error.isEmpty()) return outcome.error;
        switch (outcome.failure) {
            case Failure::NoGroup: return QStringLiteral("The group no longer exists or is archived");
            case Failure::Fetch: return QStringLiteral("The download failed");
            case Failure::Empty: return QStringLiteral("No usable profile was found");
            case Failure::Database: return QStringLiteral("Throne could not save the result");
            case Failure::Internal:
            case Failure::None: break;
        }
        return QStringLiteral("The update failed");
    }

    Response updateFailure(const Subscription::UpdateOutcome &outcome, const QString &message) {
        switch (outcome.failure) {
            case Failure::Fetch: return Response::Error(502, QStringLiteral("fetch_failed"), message);
            case Failure::Empty: return Response::Error(422, QStringLiteral("nothing_imported"), message);
            case Failure::NoGroup: {
                const auto group = Configs::dataManager->groupsRepo->GetGroup(outcome.gid);
                if (group != nullptr && group->archive) return Response::Error(409, QStringLiteral("group_archived"), message);
                return Response::Error(404, QStringLiteral("not_found"), message);
            }
            case Failure::Database:
            case Failure::Internal:
            case Failure::None: break;
        }
        return Response::Error(500, QStringLiteral("internal"), message);
    }

    Response importFailure(const Subscription::UpdateOutcome &outcome) {
        if (outcome.failure != Failure::Empty) return updateFailure(outcome, failureText(outcome));
        auto message = outcome.report.summary().trimmed();
        if (message.isEmpty()) message = failureText(outcome);
        if (const auto details = outcome.report.details(kNothingImportedDetails).trimmed(); !details.isEmpty()) {
            message += QLatin1Char('\n') + details;
        }
        return updateFailure(outcome, message);
    }

    QJsonObject importJson(const Subscription::UpdateOutcome &outcome) {
        const auto &report = outcome.report;
        QJsonArray ids;
        for (const int id : outcome.addedIds) ids.append(id);
        QJsonArray issues;
        for (const auto &line : report.lines()) {
            issues.append(QJsonObject{
                {"severity", line.severity == Validation::Severity::Error ? QStringLiteral("error") : QStringLiteral("warning")},
                {"field", line.field},
                {"message", line.message},
                {"count", line.count},
                {"examples", QJsonArray::fromStringList(line.examples)},
            });
        }
        return {
            {"group", outcome.gid},
            {"imported", report.imported()},
            {"profile_ids", ids},
            {"failed", report.failed()},
            {"warned", report.warned()},
            {"issues", issues},
            {"errors", QJsonArray::fromStringList(report.fatalReasons())},
        };
    }

    bool geoFileFromName(const QString &name, Engine::AssetUpdates::GeoFile *file) {
        if (name == QLatin1String("geoip")) {
            *file = Engine::AssetUpdates::GeoFile::GeoIp;
        } else if (name == QLatin1String("geosite")) {
            *file = Engine::AssetUpdates::GeoFile::GeoSite;
        } else {
            return false;
        }
        return true;
    }
}

namespace RemoteApi {
    QJsonObject Router::updateResultJson(const Subscription::UpdateOutcome &outcome) {
        if (!outcome.ok()) return {{"ok", false}, {"error", RouterManageImpl::failureText(outcome)}};
        return {
            {"ok", true},
            {"added", outcome.added},
            {"updated", outcome.updated},
            {"removed", outcome.removed},
            {"kept", outcome.kept},
        };
    }

    std::optional<Response> Router::afterStop(bool stopRunning, const std::function<void()> &then) {
        if (connectionBusy()) return RouterManageImpl::busyConnection();
        if (stopRunning && engine_->runningSnapshot() != nullptr) {
            stops_.append(then);
            engine_->stop(Engine::StopOptions{.block = false, .manual = true, .interactive = false});
            return std::nullopt;
        }
        then();
        return std::nullopt;
    }

    void Router::postImport(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<QString> as;
        std::optional<QString> text;
        std::optional<QString> url;
        std::optional<int> group;
        WaitOptions options;
        if (!bodyObject(request, true, &body, &error) || !readString(body, QStringLiteral("as"), &as, &error) ||
            !readString(body, QStringLiteral("text"), &text, &error) || !readString(body, QStringLiteral("url"), &url, &error) ||
            !readId(body, QStringLiteral("group"), &group, &error) ||
            !readWait(body, true, RouterManageImpl::kImportWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        Engine::ImportService::Request importRequest;
        importRequest.groupId = group.value_or(-1);
        if (as == QStringLiteral("profiles")) {
            if (text.value_or(QString()).trimmed().isEmpty()) {
                respond(badRequest(QStringLiteral("\"text\" is required and must not be empty")));
                return;
            }
            importRequest.source = Engine::ImportService::Source::Text;
            importRequest.payload = *text;
        } else if (as == QStringLiteral("url")) {
            const auto target = url.value_or(QString()).trimmed();
            if (!RouterManageImpl::isHttpUrl(target)) {
                respond(badRequest(QStringLiteral("\"url\" must be an http or https URL")));
                return;
            }
            importRequest.source = Engine::ImportService::Source::Url;
            importRequest.payload = target;
        } else {
            respond(badRequest(QStringLiteral("\"as\" must be profiles or url")));
            return;
        }

        const auto gid = std::make_shared<int>(importRequest.groupId);
        const auto waiter = makeWaiter(respond, options.wait ? options.timeoutMs : 0, [gid] {
            return Response::Ok(QJsonObject{{"group", *gid}, {"state", QStringLiteral("queued")}}, 202);
        });
        // The service logs every outcome, so a late one needs no log line here.
        const auto refused = Engine::ImportService::Import(importRequest, [this, waiter](const Subscription::UpdateOutcome &outcome) {
            answer(waiter, outcome.ok() ? Response::Ok(RouterManageImpl::importJson(outcome)) : RouterManageImpl::importFailure(outcome));
        }, gid.get());
        if (refused) answer(waiter, serviceError(*refused));
    }

    void Router::postGroup(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<QString> name;
        std::optional<QString> url;
        std::optional<bool> autoUpdate;
        std::optional<bool> update;
        WaitOptions options;
        if (!bodyObject(request, true, &body, &error) || !readString(body, QStringLiteral("name"), &name, &error) ||
            !readString(body, QStringLiteral("url"), &url, &error) || !readBool(body, QStringLiteral("auto_update"), &autoUpdate, &error) ||
            !readBool(body, QStringLiteral("update"), &update, &error) ||
            !readWait(body, true, RouterManageImpl::kGroupUpdateWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        Engine::GroupService::NewGroup spec;
        spec.name = name.value_or(QString()).trimmed();
        if (spec.name.isEmpty()) {
            respond(badRequest(QStringLiteral("\"name\" is required and must not be empty")));
            return;
        }
        if (url) {
            spec.url = url->trimmed();
            if (!RouterManageImpl::isHttpUrl(spec.url)) {
                respond(badRequest(QStringLiteral("\"url\" must be an http or https URL")));
                return;
            }
        }
        spec.autoUpdate = autoUpdate.value_or(true);
        int gid = -1;
        if (const auto refused = Engine::GroupService::Add(spec, &gid)) {
            respond(serviceError(*refused));
            return;
        }

        const auto groupValue = [gid] {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
            return group != nullptr ? QJsonValue(groupJson(*group)) : QJsonValue();
        };
        if (spec.url.isEmpty() || !update.value_or(true)) {
            respond(Response::Ok(QJsonObject{{"group", groupValue()}, {"update", QJsonValue()}}));
            return;
        }
        const auto waiter = makeWaiter(respond, options.wait ? options.timeoutMs : 0, [groupValue] {
            return Response::Ok(QJsonObject{{"group", groupValue()}}, 202);
        });
        // The group exists either way, so a failed first update is still a 200.
        const auto refused = Engine::GroupService::Update(gid, [this, waiter, groupValue](const Subscription::UpdateOutcome &outcome) {
            answer(waiter, Response::Ok(QJsonObject{{"group", groupValue()}, {"update", updateResultJson(outcome)}}));
        });
        if (refused) {
            answer(waiter, Response::Ok(QJsonObject{
                {"group", groupValue()},
                {"update", QJsonObject{{"ok", false}, {"error", refused->message}}},
            }));
        }
    }

    void Router::postGroupUpdate(const Request &request, const Responder &respond, int id) {
        QJsonObject body;
        Response error;
        WaitOptions options;
        if (!bodyObject(request, false, &body, &error) ||
            !readWait(body, true, RouterManageImpl::kGroupUpdateWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        const auto groupValue = [id] {
            const auto group = Configs::dataManager->groupsRepo->GetGroup(id);
            return group != nullptr ? QJsonValue(groupJson(*group)) : QJsonValue();
        };
        const auto waiter = makeWaiter(respond, options.wait ? options.timeoutMs : 0, [groupValue] {
            return Response::Ok(QJsonObject{{"group", groupValue()}}, 202);
        });
        const auto refused = Engine::GroupService::Update(id, [this, waiter, groupValue](const Subscription::UpdateOutcome &outcome) {
            if (outcome.ok()) {
                answer(waiter, Response::Ok(QJsonObject{{"group", groupValue()}, {"update", updateResultJson(outcome)}}));
            } else {
                answer(waiter, RouterManageImpl::updateFailure(outcome, RouterManageImpl::failureText(outcome)));
            }
        });
        if (refused) answer(waiter, serviceError(*refused));
    }

    void Router::postGroupsUpdate(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        WaitOptions options;
        if (!bodyObject(request, false, &body, &error) ||
            !readWait(body, true, RouterManageImpl::kGroupsUpdateWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        const auto queued = std::make_shared<QList<int>>();
        const auto waiter = makeWaiter(respond, options.wait ? options.timeoutMs : 0, [queued] {
            QJsonArray ids;
            for (const int id : *queued) ids.append(id);
            return Response::Ok(QJsonObject{{"queued", ids}}, 202);
        });
        *queued = Engine::GroupService::UpdateAll([this, waiter](const QList<Subscription::UpdateOutcome> &outcomes) {
            QJsonArray results;
            for (const auto &outcome : outcomes) {
                auto result = updateResultJson(outcome);
                result.insert(QStringLiteral("id"), outcome.gid);
                results.append(result);
            }
            answer(waiter, Response::Ok(QJsonObject{{"results", results}}));
        });
        if (queued->isEmpty()) answer(waiter, Response::Ok(QJsonObject{{"results", QJsonArray()}}));
    }

    void Router::deleteGroup(const Request &request, const Responder &respond, int id) {
        QJsonObject body;
        Response error;
        WaitOptions options;
        if (!bodyObject(request, false, &body, &error) || !readWait(body, true, RouterManageImpl::kDeleteWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        if (const auto refused = Engine::GroupService::CheckDelete(id)) {
            respond(serviceError(*refused));
            return;
        }
        const auto running = engine_->runningSnapshot();
        const bool stopRunning = running != nullptr && running->gid == id;
        const auto waiter = makeWaiter(respond, options.wait ? options.timeoutMs : 0, [id] {
            return Response::Ok(QJsonObject{{"deleted", id}, {"state", QStringLiteral("stopping")}}, 202);
        });
        const auto busy = afterStop(stopRunning, [this, id, waiter] {
            // A start may have slipped in while the stop ran.
            if (const auto now = engine_->runningSnapshot(); now != nullptr && now->gid == id) {
                fail(waiter, Response::Error(409, QStringLiteral("busy"), QStringLiteral("A profile of the group is running")));
                return;
            }
            QList<int> members;
            if (const auto group = Configs::dataManager->groupsRepo->GetGroup(id)) members = group->Profiles();
            const bool usedByRunning = RouterManageImpl::anyRunningUses(members);
            if (const auto refused = Engine::GroupService::Delete(id)) {
                fail(waiter, serviceError(*refused));
                return;
            }
            if (usedByRunning) engine_->noteRestartNeeded(Engine::RestartReason::Group);
            answer(waiter, Response::Ok(QJsonObject{{"deleted", id}}));
        });
        if (busy) answer(waiter, *busy);
    }

    void Router::deleteProfiles(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        QList<int> ids;
        WaitOptions options;
        if (!bodyObject(request, true, &body, &error) || !readSelection(body, &ids, &error) ||
            !readWait(body, true, RouterManageImpl::kDeleteWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        if (ids.isEmpty()) {
            respond(noProfiles());
            return;
        }
        if (const auto refused = Engine::ProfileService::CheckDelete(ids)) {
            respond(serviceError(*refused));
            return;
        }
        const auto running = engine_->runningSnapshot();
        const bool stopRunning = running != nullptr && ids.contains(running->id);
        const auto waiter = makeWaiter(respond, options.wait ? options.timeoutMs : 0, [count = static_cast<int>(ids.size())] {
            return Response::Ok(QJsonObject{{"deleted", count}, {"state", QStringLiteral("stopping")}}, 202);
        });
        const auto busy = afterStop(stopRunning, [this, ids, waiter] {
            if (const auto now = engine_->runningSnapshot(); now != nullptr && ids.contains(now->id)) {
                fail(waiter, Response::Error(409, QStringLiteral("busy"), QStringLiteral("One of the profiles is running")));
                return;
            }
            const bool usedByRunning = RouterManageImpl::anyRunningUses(ids);
            int deleted = 0;
            const auto refused = Engine::ProfileService::Delete(ids, &deleted);
            if (usedByRunning && deleted > 0) engine_->noteRestartNeeded(Engine::RestartReason::Profile);
            if (refused) {
                fail(waiter, serviceError(*refused));
                return;
            }
            answer(waiter, Response::Ok(QJsonObject{{"deleted", deleted}}));
        });
        if (busy) answer(waiter, *busy);
    }

    void Router::getSettings(const Request &, const Responder &respond, int) {
        respond(Response::Ok(QJsonObject{{"settings", Engine::SettingsRegistry::Values()}}));
    }

    void Router::putSettings(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        if (!bodyObject(request, true, &body, &error)) {
            respond(error);
            return;
        }
        const auto patch = body.value(QStringLiteral("settings"));
        if (!patch.isObject()) {
            respond(badRequest(QStringLiteral("\"settings\" must be an object of setting values")));
            return;
        }
        // The start and test workers read the settings off the main thread.
        if (connectionBusy()) {
            respond(RouterManageImpl::busyConnection());
            return;
        }
        if (engine_->testsRunning()) {
            respond(Response::Error(409, QStringLiteral("busy"), QStringLiteral("A test is running")));
            return;
        }
        const auto &settings = Configs::dataManager->settingsRepo;
        Engine::SettingsRegistry::Context context;
        context.profileRunning = engine_->runningSnapshot() != nullptr;
        context.tunOn = settings->spmode_vpn;
        context.systemProxyOn = settings->spmode_system_proxy;
        Engine::SettingsRegistry::PutResult result;
        if (const auto refused = Engine::SettingsRegistry::Put(patch.toObject(), context, &result)) {
            respond(serviceError(*refused));
            return;
        }
        if (result.restartNeeded) engine_->noteRestartNeeded(Engine::RestartReason::Settings);
        respond(Response::Ok(QJsonObject{
            {"settings", Engine::SettingsRegistry::Values()},
            {"changed", QJsonArray::fromStringList(result.changed)},
            {"restart_needed", engine_->restartPending()},
        }));
    }

    void Router::postRuleSetsUpdate(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        WaitOptions options;
        if (!bodyObject(request, false, &body, &error) || !readWait(body, true, RouterManageImpl::kRuleSetsWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        const auto notRunning = Response::Error(409, QStringLiteral("not_running"), QStringLiteral("No profile runs, so no rule-set is loaded"));
        if (engine_->runningSnapshot() == nullptr) {
            respond(notRunning);
            return;
        }
        const auto waiter = makeWaiter(respond, options.wait ? options.timeoutMs : 0, [] {
            return Response::Ok(QJsonObject{{"state", QStringLiteral("updating")}}, 202);
        });
        const bool queued = Engine::AssetUpdates::UpdateRuleSets([this, waiter, notRunning](const Engine::AssetUpdates::RuleSetResult &result) {
            if (!result.coreReachable) {
                answer(waiter, Response::Error(503, QStringLiteral("core_unavailable"),
                                               result.error.isEmpty() ? QStringLiteral("Throne's core process is not reachable") : result.error));
                return;
            }
            if (result.notRunning) {
                answer(waiter, notRunning);
                return;
            }
            QJsonArray failures;
            for (const auto &line : result.failures) {
                const auto split = line.indexOf(QStringLiteral(": "));
                if (split < 0) failures.append(QJsonObject{{"tag", QString()}, {"error", line}});
                else failures.append(QJsonObject{{"tag", line.left(split)}, {"error", line.mid(split + 2)}});
            }
            answer(waiter, Response::Ok(QJsonObject{{"updated", result.updated}, {"failures", failures}}));
        });
        if (!queued) answer(waiter, Response::Error(409, QStringLiteral("busy"), QStringLiteral("A rule-set update is already running")));
    }

    void Router::getGeoAssets(const Request &, const Responder &respond, int) {
        QJsonArray files;
        for (const auto &status : Engine::AssetUpdates::GeoStatus()) {
            files.append(QJsonObject{
                {"name", status.fileName},
                {"url", status.url},
                {"present", status.present},
                {"size", status.size},
                {"updated_at", status.modifiedAt},
            });
        }
        respond(Response::Ok(QJsonObject{{"files", files}}));
    }

    void Router::postGeoAssetsUpdate(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<bool> onlyMissing;
        WaitOptions options;
        if (!bodyObject(request, false, &body, &error) || !readBool(body, QStringLiteral("only_missing"), &onlyMissing, &error) ||
            !readWait(body, true, RouterManageImpl::kGeoWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        Engine::AssetUpdates::GeoRequest geo;
        geo.onlyMissing = onlyMissing.value_or(false);
        if (present(body, QStringLiteral("files"))) {
            const auto list = body.value(QStringLiteral("files"));
            const auto unknown = badRequest(QStringLiteral("\"files\" must be a non-empty array of geoip and geosite"));
            if (!list.isArray() || list.toArray().isEmpty()) {
                respond(unknown);
                return;
            }
            geo.files.clear();
            for (const auto &item : list.toArray()) {
                auto file = Engine::AssetUpdates::GeoFile::GeoIp;
                if (!item.isString() || !RouterManageImpl::geoFileFromName(item.toString(), &file)) {
                    respond(unknown);
                    return;
                }
                if (!geo.files.contains(file)) geo.files.append(file);
            }
        }

        const auto waiter = makeWaiter(respond, options.wait ? options.timeoutMs : 0, [] {
            return Response::Ok(QJsonObject{{"state", QStringLiteral("downloading")}}, 202);
        });
        const bool queued = Engine::AssetUpdates::UpdateGeoAssets(geo, [this, waiter](const QList<Engine::AssetUpdates::GeoFileResult> &results) {
            QJsonArray files;
            for (const auto &result : results) {
                QJsonObject file{{"name", result.fileName}, {"ok", result.error.isEmpty()}};
                if (!result.error.isEmpty()) file.insert(QStringLiteral("error"), result.error);
                else if (result.skipped) file.insert(QStringLiteral("skipped"), true);
                files.append(file);
            }
            answer(waiter, Response::Ok(QJsonObject{{"files", files}}));
        });
        if (!queued) answer(waiter, Response::Error(409, QStringLiteral("busy"), QStringLiteral("One of the files is already downloading")));
    }

    void Router::postRestart(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        if (!bodyObject(request, false, &body, &error)) {
            respond(error);
            return;
        }
        // Also covers a restore that is stopping the profile before it applies.
        if (connectionBusy()) {
            respond(RouterManageImpl::busyConnection());
            return;
        }
        const bool resume = engine_->resumesOnBoot() && engine_->runningSnapshot() != nullptr;
        // Queued, so this reply leaves before the teardown starts.
        if (!engine_->requestExit(Engine::ExitReason::Restart)) {
            respond(RouterManageImpl::exiting());
            return;
        }
        respond(Response::Ok(QJsonObject{{"restarting", true}, {"resume", resume}}, 202));
    }

    void Router::postShutdown(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        if (!bodyObject(request, false, &body, &error)) {
            respond(error);
            return;
        }
        if (connectionBusy()) {
            respond(RouterManageImpl::busyConnection());
            return;
        }
        if (!engine_->requestExit(Engine::ExitReason::None)) {
            respond(RouterManageImpl::exiting());
            return;
        }
        respond(Response::Ok(QJsonObject{{"stopping", true}}, 202));
    }
}
