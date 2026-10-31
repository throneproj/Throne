#pragma once

#include <QString>
#include <memory>

#include "include/database/entities/RouteProfile.h"

namespace RouteUpdate {
    // Blocks on the network (worker thread only); returns an error string, empty on success.
    QString UpdateProfile(const std::shared_ptr<Configs::RouteProfile>& profile, QString* warnings = nullptr);

    // A subscription's routing profile (Happ "happ://routing/add/<base64>", its bare JSON, or throne://route/...),
    // cut down to proxy/direct/block rules with no DNS or endpoints. Null with *error empty when nothing is provided ("off").
    std::shared_ptr<Configs::RouteProfile> ParseSubscriptionRoute(const QString& value, int* imported, int* dropped, QString* error);

    // Parses on the calling thread, then creates or refreshes group gid's own routing profile on the UI thread.
    void ImportSubscriptionRoute(int gid, const QString& value);
}

// Runs the whole batch on a worker thread and saves each result; the active route is not reloaded.
void UI_update_all_remote_routes(bool onlyAllowed = false);
