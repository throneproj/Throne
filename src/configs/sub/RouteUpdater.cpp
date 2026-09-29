#include "include/configs/sub/RouteUpdater.hpp"

#include <QDateTime>
#include <QStringList>

#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/database/RoutesRepo.h"
#include "include/configs/generate.h"

namespace RouteUpdate {
    QString UpdateProfile(const std::shared_ptr<Configs::RouteProfile>& profile, QString* warnings) {
        if (!profile) return QObject::tr("internal error: null profile");
        if (!profile->isRemote) return QObject::tr("not a remote routing profile");
        const QString url = profile->remoteURL.trimmed();
        if (url.isEmpty()) return QObject::tr("remote URL is empty");

        auto resp = NetworkRequestHelper::HttpGet(Configs::get_jsdelivr_link(url));
        if (!resp.error.isEmpty()) return resp.error;

        QString fatal, warn;
        bool wasOldArray = false;
        // unmaterialized: a remote URL must never create server profiles here
        auto fetched = Configs::RouteProfile::FromShareInput(QString::fromUtf8(resp.data), &fatal, &warn, &wasOldArray, false);
        if (!fetched) return fatal.isEmpty() ? QObject::tr("could not parse a routing profile from the response") : fatal;
        if (fetched->isRaw) return QObject::tr("the remote content is a raw routing profile, which is not supported for remote profiles yet");

        // endpoint rules belong to the local profile, so they survive the swap by index
        QList<QPair<int, std::shared_ptr<Configs::RouteRule>>> endpointRules;
        for (int i = 0; i < profile->Rules.size(); i++) {
            if (profile->Rules[i]->type == Configs::endpointPreferredBy) endpointRules << qMakePair(i, profile->Rules[i]);
        }

        profile->isRaw = false;
        profile->rawRoute.clear();
        profile->Rules.clear();
        for (const auto& rule : fetched->Rules) {
            if (rule->type != Configs::endpointPreferredBy) profile->Rules << rule;
        }
        for (const auto& [index, rule] : endpointRules) {
            profile->Rules.insert(std::min<qsizetype>(index, profile->Rules.size()), rule);
        }
        profile->SyncEndpointRules();
        profile->defaultOutboundID = fetched->defaultOutboundID;
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
