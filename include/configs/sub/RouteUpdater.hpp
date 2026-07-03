#pragma once

#include <QString>
#include <memory>

#include "include/database/entities/RouteProfile.h"

namespace RouteUpdate {
    // Fetch profile->remoteURL, parse the response (a throne://route deep link, its base64,
    // or the JSON share object) and overwrite the profile's structured rules + default
    // outbound in place, keeping id/name/URL/auto-update. Blocks on the network, so call it
    // on a worker thread. Returns an error string (empty on success); non-fatal notes (e.g.
    // outbound fallbacks) are written to *warnings when provided.
    QString UpdateProfile(const std::shared_ptr<Configs::RouteProfile>& profile, QString* warnings = nullptr);
}

// Fetch + persist every remote route profile: only those with auto-update enabled when
// onlyAllowed is true, otherwise all of them. The whole batch runs on a worker thread and
// each successful result is saved to the DB (no live reload of the active route). Safe to
// call from the UI thread.
void UI_update_all_remote_routes(bool onlyAllowed = false);
