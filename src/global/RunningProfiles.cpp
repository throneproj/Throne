#include "include/global/RunningProfiles.hpp"

#include <QMutex>
#include <QMutexLocker>

#include <atomic>

#include "include/database/GroupsRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/global/Configs.hpp"

namespace Configs {
    namespace {
        QMutex runningProfilesMu;
        QSet<int> runningProfiles;
        std::atomic<int> runningRoute{-1};
        thread_local int buildRoute = -1;
    }

    void SetRunningProfiles(const QSet<int> &profileIDs)
    {
        QMutexLocker lk(&runningProfilesMu);
        runningProfiles.clear();
        for (const auto id : profileIDs) {
            if (id < 0) continue;
            runningProfiles.insert(id);
        }
    }

    void ClearRunningProfiles()
    {
        QMutexLocker lk(&runningProfilesMu);
        runningProfiles.clear();
        runningRoute = -1;
    }

    bool RunningUsesProfile(int profileID)
    {
        if (profileID < 0) return false;
        QMutexLocker lk(&runningProfilesMu);
        return runningProfiles.contains(profileID);
    }

    void SetRunningRoute(int routeId)
    {
        runningRoute = routeId;
    }

    int RunningRouteId()
    {
        return runningRoute;
    }

    int ResolveRouteId(int groupRouteId)
    {
        if (groupRouteId >= 0 && dataManager->routesRepo->HasRouteProfile(groupRouteId)) return groupRouteId;
        return dataManager->settingsRepo->current_route_id;
    }

    int RouteIdFor(int gid)
    {
        const auto group = gid >= 0 ? dataManager->groupsRepo->GetGroup(gid) : nullptr;
        return ResolveRouteId(group != nullptr ? group->route_id : -1);
    }

    int ActiveRouteId()
    {
        if (buildRoute >= 0) return buildRoute;
        if (const int running = runningRoute; running >= 0) return running;
        return dataManager->settingsRepo->current_route_id;
    }

    ScopedBuildRoute::ScopedBuildRoute(int routeId) : previous(buildRoute)
    {
        buildRoute = routeId;
    }

    ScopedBuildRoute::~ScopedBuildRoute()
    {
        buildRoute = previous;
    }
}
