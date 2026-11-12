#pragma once

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

    // Main thread. Applies a PUT body to a copy of the routing profile and saves it only when everything validates.
    std::optional<ServiceError> Update(int routeId, const QJsonObject &body, QStringList *warnings);

    // Main thread. Refuses the last routing profile; RoutesRepo moves what pointed at it elsewhere.
    std::optional<ServiceError> Delete(int routeId);
} // namespace Engine::RouteService
