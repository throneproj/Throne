#include "include/configs/sub/RouteUpdater.hpp"

#include <QDateTime>
#include <QStringList>

#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/database/RoutesRepo.h"

namespace RouteUpdate {
    QString UpdateProfile(const std::shared_ptr<Configs::RouteProfile>& profile, QString* warnings) {
        if (!profile) return QObject::tr("internal error: null profile");
        if (!profile->isRemote) return QObject::tr("not a remote routing profile");
        const QString url = profile->remoteURL.trimmed();
        if (url.isEmpty()) return QObject::tr("remote URL is empty");

        auto resp = NetworkRequestHelper::HttpGet(url, false);
        if (!resp.error.isEmpty()) return resp.error;

        QString fatal, warn;
        bool wasOldArray = false;
        auto fetched = Configs::RouteProfile::FromShareInput(QString::fromUtf8(resp.data), &fatal, &warn, &wasOldArray);
        if (!fetched) return fatal.isEmpty() ? QObject::tr("could not parse a routing profile from the response") : fatal;
        if (fetched->isRaw) return QObject::tr("the remote content is a raw routing profile, which is not supported for remote profiles yet");

        // Overwrite the structured rules and default outbound, but keep the local identity
        // and remote settings so the user's URL / auto-update survive.
        profile->isRaw = false;
        profile->rawRoute.clear();
        profile->Rules = fetched->Rules;
        profile->defaultOutboundID = fetched->defaultOutboundID;
        // Adopt the remote name when the profile doesn't have one yet (e.g. a freshly created
        // remote profile). A name the user already chose is kept, so updates don't clobber it.
        if (profile->name.trimmed().isEmpty() && !fetched->name.trimmed().isEmpty()) {
            profile->name = fetched->name;
        }
        profile->remoteLastUpdate = QDateTime::currentSecsSinceEpoch();
        if (warnings) *warnings = warn;
        return {};
    }
}

void UI_update_all_remote_routes(bool onlyAllowed) {
    runOnNewThread([=] {
        auto profiles = Configs::dataManager->routesRepo->GetAllRouteProfiles();
        int updated = 0;
        QStringList failures;
        for (const auto& p : profiles) {
            if (!p->isRemote || p->remoteURL.trimmed().isEmpty()) continue;
            if (onlyAllowed && !p->autoUpdate) continue;

            MW_show_log(">>>>>>>> " + QObject::tr("Updating remote routing profile: %1").arg(p->name));
            QString warnings;
            auto err = RouteUpdate::UpdateProfile(p, &warnings);
            if (!err.isEmpty()) {
                failures << (p->name + ": " + err);
                MW_show_log("<<<<<<<< " + QObject::tr("Remote routing profile %1 failed: %2").arg(p->name, err));
                continue;
            }
            Configs::dataManager->routesRepo->Save(p);
            updated++;
            MW_show_log("<<<<<<<< " + QObject::tr("Remote routing profile updated: %1").arg(p->name)
                        + (warnings.isEmpty() ? QString() : "\n" + warnings));
        }
        MW_show_log(QObject::tr("Remote routing profiles: %1 updated, %2 failed").arg(updated).arg(failures.size()));
    });
}
