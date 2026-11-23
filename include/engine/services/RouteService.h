#pragma once

#include <memory>
#include <optional>

#include <QJsonObject>
#include <QStringList>

#include "include/engine/services/ServiceTypes.h"

namespace Configs {
    class RouteProfile;
}

namespace Engine::RouteService {
    // The rule editor's attributes, rule types and built-in outbounds (GET /v1/routes/schema).
    QJsonObject SchemaJson();

    // The routing profile's editable state: what the route detail adds to the Route object.
    QJsonObject DetailJson(Configs::RouteProfile &route);

    // Main thread. Why Update or Delete would refuse now: unknown, or the window's routing editor is open.
    std::optional<ServiceError> CheckEdit(int routeId);

    // Main thread. Creates a routing profile from a POST body; *routeId gets its id and *warnings the issues that do not block.
    std::optional<ServiceError> Create(const QJsonObject &body, int *routeId, QStringList *warnings);

    // Main thread. Applies a PUT body to a copy of the routing profile and saves it only when everything validates; notes a
    // Routing restart when it is the active route.
    std::optional<ServiceError> Update(int routeId, const QJsonObject &body, QStringList *warnings);

    // Main thread. Refuses the last routing profile; RoutesRepo moves what pointed at it elsewhere. Notes a Routing restart
    // when it was the active route.
    std::optional<ServiceError> Delete(int routeId);

    // Main thread. Adds a routing profile the window built (a share-link or deeplink import, remote profiles) and emits
    // RoutesChanged.
    std::optional<ServiceError> Add(const std::shared_ptr<Configs::RouteProfile> &route);

    // Main thread. Commits an edited copy of a routing profile (the window's route editors, the connections-menu rule toggle,
    // a remote update finished off the main thread); the caller holds or has checked the edit lock. Saves it, notes a Routing
    // restart when it is the active route, and emits RoutesChanged. not_found when it was deleted meanwhile.
    std::optional<ServiceError> Save(const std::shared_ptr<Configs::RouteProfile> &route);

    // Main thread, under the Routes edit lock the Routing Settings window holds. Adds the new ones (id < 0, ids assigned),
    // saves the edited ones and deletes removedIds; notes a Routing restart when the active route is among them, and emits
    // one RoutesChanged when anything changed.
    std::optional<ServiceError> Commit(const QList<std::shared_ptr<Configs::RouteProfile>> &changed, const QList<int> &removedIds);
} // namespace Engine::RouteService
