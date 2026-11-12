#include "include/api/remote/Router.hpp"

#include <QJsonArray>

#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineControl.h"
#include "include/engine/services/ProfileService.h"
#include "include/engine/services/ServiceTypes.h"
#include "include/global/Configs.hpp"

namespace RemoteApi::RouterProfileConfigImpl {
    Response busyConnection() {
        return Response::Error(409, QStringLiteral("busy"), QStringLiteral("A profile is starting or stopping"));
    }
} // namespace RemoteApi::RouterProfileConfigImpl

namespace RemoteApi {
    void Router::getProfileConfig(const Request &, const Responder &respond, int id) {
        const auto config = Engine::ProfileService::ReadConfig(id);
        if (!config) {
            respond(notFound(QStringLiteral("Unknown profile %1").arg(id)));
            return;
        }
        respond(Response::Ok(QJsonObject{
            {"id", id},
            {"type", config->type},
            {"name", config->name},
            {"outbound", config->outbound},
            {"link", config->link},
            {"editable", config->notEditable.isEmpty()},
            {"reason", config->notEditable},
        }));
    }

    void Router::putProfileConfig(const Request &request, const Responder &respond, int id) {
        QJsonObject body;
        Response error;
        std::optional<QString> link;
        if (!bodyObject(request, true, &body, &error) || !readString(body, QStringLiteral("link"), &link, &error)) {
            respond(error);
            return;
        }
        const bool outboundGiven = present(body, QStringLiteral("outbound"));
        if (outboundGiven == link.has_value()) {
            respond(badRequest(QStringLiteral("Send exactly one of \"outbound\" and \"link\"")));
            return;
        }
        Engine::ProfileService::ConfigInput input;
        if (outboundGiven) {
            const auto outbound = body.value(QStringLiteral("outbound"));
            if (!outbound.isObject()) {
                respond(badRequest(QStringLiteral("\"outbound\" must be an object")));
                return;
            }
            input.outbound = outbound.toObject();
        } else {
            input.link = *link;
        }
        if (const auto refused = Engine::ProfileService::CheckConfigEdit(id)) {
            respond(serviceError(*refused));
            return;
        }
        // The start and test workers read profiles off the main thread.
        if (connectionBusy()) {
            respond(RouterProfileConfigImpl::busyConnection());
            return;
        }
        if (engine_->testsRunning()) {
            respond(Response::Error(409, QStringLiteral("busy"), QStringLiteral("A test is running")));
            return;
        }
        Engine::ProfileService::ConfigApplied applied;
        if (const auto refused = Engine::ProfileService::ApplyConfig(id, input, &applied)) {
            respond(serviceError(*refused));
            return;
        }
        if (applied.usedByRunning) engine_->noteRestartNeeded(Engine::RestartReason::Profile);
        noteChanged(Engine::ChangeTopic::Profiles);
        const auto profile = Configs::dataManager->profilesRepo->GetProfile(id);
        if (profile == nullptr) {
            respond(notFound(QStringLiteral("Profile %1 was deleted meanwhile").arg(id)));
            return;
        }
        respond(Response::Ok(QJsonObject{
            {"profile", profileJson(profile)},
            {"warnings", QJsonArray::fromStringList(applied.warnings)},
            {"restart_needed", engine_->restartPending()},
        }));
    }
} // namespace RemoteApi
