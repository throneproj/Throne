#pragma once

#include <QSet>

namespace Configs
{
    // Every profile the running config was built from: the started one, its chain hops, the group's
    // front/landing proxies, route-profile outbounds and endpoint hops, auto-selector members.
    void SetRunningProfiles(const QSet<int> &profileIDs);

    // Clears the running route too.
    void ClearRunningProfiles();

    bool RunningUsesProfile(int profileID);

    // The routing profile the running config was built with; -1 when none (stopped, or a custom full config).
    void SetRunningRoute(int routeId);

    int RunningRouteId();

    // A group override naming a routing profile that still exists, else the global one (current_route_id).
    int ResolveRouteId(int groupRouteId);

    // The routing profile a profile of group gid is built with.
    int RouteIdFor(int gid);

    // The routing profile in effect: the one a config is being built with on this thread, else the running
    // config's, else the global one. For "which route applies"; the user's global choice stays current_route_id.
    int ActiveRouteId();

    // While alive, ActiveRouteId() on this thread answers routeId, so the build's readers (RouteDns::Current) follow it.
    class ScopedBuildRoute {
    public:
        explicit ScopedBuildRoute(int routeId);
        ~ScopedBuildRoute();
        ScopedBuildRoute(const ScopedBuildRoute &) = delete;
        ScopedBuildRoute &operator=(const ScopedBuildRoute &) = delete;

    private:
        int previous;
    };
}
