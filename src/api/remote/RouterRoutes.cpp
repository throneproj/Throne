#include "include/api/remote/Router.hpp"

#include <QJsonArray>

#include "include/database/RoutesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/engine/EngineControl.h"
#include "include/engine/services/RouteService.h"
#include "include/engine/services/ServiceTypes.h"
#include "include/global/Configs.hpp"
#include "include/global/RunningProfiles.hpp"

namespace RemoteApi::RouterRoutesImpl {
    Response busyConnection() {
        return Response::Error(409, QStringLiteral("busy"), QStringLiteral("A profile is starting or stopping"));
    }

    QJsonObject withDetail(QJsonObject route, Configs::RouteProfile &profile) {
        const auto detail = Engine::RouteService::DetailJson(profile);
        for (auto it = detail.constBegin(); it != detail.constEnd(); ++it) route.insert(it.key(), it.value());
        return route;
    }
} // namespace RemoteApi::RouterRoutesImpl

namespace RemoteApi {
    void Router::getRouteSchema(const Request &, const Responder &respond, int) {
        respond(Response::Ok(Engine::RouteService::SchemaJson()));
    }

    void Router::getRoute(const Request &, const Responder &respond, int id) {
        const auto route = Configs::dataManager->routesRepo->GetRouteProfile(id);
        if (route == nullptr) {
            respond(notFound(QStringLiteral("Unknown routing profile %1").arg(id)));
            return;
        }
        respond(Response::Ok(RouterRoutesImpl::withDetail(routeJson(*route), *route)));
    }

    void Router::postRoute(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        if (!bodyObject(request, true, &body, &error)) {
            respond(error);
            return;
        }
        // The routing dialog writes its whole list back on OK.
        if (Engine::IsBeingEdited(Engine::EditTarget::Routes)) {
            respond(editing(QStringLiteral("the routing profile list")));
            return;
        }
        if (connectionBusy()) {
            respond(RouterRoutesImpl::busyConnection());
            return;
        }
        int routeId = -1;
        QStringList warnings;
        if (const auto refused = Engine::RouteService::Create(body, &routeId, &warnings)) {
            respond(serviceError(*refused));
            return;
        }
        noteChanged(Engine::ChangeTopic::Status);
        const auto route = Configs::dataManager->routesRepo->GetRouteProfile(routeId);
        if (route == nullptr) {
            respond(notFound(QStringLiteral("Routing profile %1 was deleted meanwhile").arg(routeId)));
            return;
        }
        respond(Response::Ok(QJsonObject{
            {"route", RouterRoutesImpl::withDetail(routeJson(*route), *route)},
            {"warnings", QJsonArray::fromStringList(warnings)},
            {"restart_needed", engine_->restartPending()},
        }, 201));
    }

    void Router::putRoute(const Request &request, const Responder &respond, int id) {
        QJsonObject body;
        Response error;
        if (!bodyObject(request, true, &body, &error)) {
            respond(error);
            return;
        }
        if (const auto refused = Engine::RouteService::CheckEdit(id)) {
            respond(serviceError(*refused));
            return;
        }
        // The config build reads routing profiles off the main thread.
        if (connectionBusy()) {
            respond(RouterRoutesImpl::busyConnection());
            return;
        }
        const bool inUse = id == Configs::ActiveRouteId();
        QStringList warnings;
        if (const auto refused = Engine::RouteService::Update(id, body, &warnings)) {
            respond(serviceError(*refused));
            return;
        }
        if (inUse && engine_->runningSnapshot() != nullptr) engine_->noteRestartNeeded(Engine::RestartReason::Routing);
        noteChanged(Engine::ChangeTopic::Status);
        const auto route = Configs::dataManager->routesRepo->GetRouteProfile(id);
        if (route == nullptr) {
            respond(notFound(QStringLiteral("Routing profile %1 was deleted meanwhile").arg(id)));
            return;
        }
        respond(Response::Ok(QJsonObject{
            {"route", RouterRoutesImpl::withDetail(routeJson(*route), *route)},
            {"warnings", QJsonArray::fromStringList(warnings)},
            {"restart_needed", engine_->restartPending()},
        }));
    }

    void Router::deleteRoute(const Request &request, const Responder &respond, int id) {
        QJsonObject body;
        Response error;
        if (!bodyObject(request, false, &body, &error)) {
            respond(error);
            return;
        }
        if (const auto refused = Engine::RouteService::CheckEdit(id)) {
            respond(serviceError(*refused));
            return;
        }
        if (connectionBusy()) {
            respond(RouterRoutesImpl::busyConnection());
            return;
        }
        // Deleting it moves current_route_id and the group overrides elsewhere, so ask before it goes.
        const bool inUse = id == Configs::ActiveRouteId();
        if (const auto refused = Engine::RouteService::Delete(id)) {
            respond(serviceError(*refused));
            return;
        }
        if (inUse && engine_->runningSnapshot() != nullptr) engine_->noteRestartNeeded(Engine::RestartReason::Routing);
        noteChanged(Engine::ChangeTopic::Status | Engine::ChangeTopic::Groups);
        respond(Response::Ok(QJsonObject{
            {"deleted", id},
            {"restart_needed", engine_->restartPending()},
        }));
    }
} // namespace RemoteApi
