#include <include/global/Common.h>

#include "include/global/Configs.hpp"
#include "include/database/ProfilesRepo.h"
#include <QMap>

QList <std::pair <int, QString>> FixProfileDisplayName(QList <std::pair <int, QString>>& raw) {
    QList <int> needsFixingIDs;
    QMap <int, QString> fetchedDisplayNames;

    for (const auto&[id, name] : raw) {
        if (name.trimmed().isEmpty()) needsFixingIDs.append(id);
    }

    auto profiles = Configs::dataManager->profilesRepo->GetProfileBatch(needsFixingIDs);
    for (const auto& profile : profiles) {
        fetchedDisplayNames[profile->id] = profile->outbound->DisplayTypeAndName();
    }

    QList <std::pair <int, QString>> res;

    for (const auto& [id, rawName] : raw) {
        if (fetchedDisplayNames.contains(id)) {
            res.append({id, fetchedDisplayNames[id]});
        } else {
            res.append({id, rawName});
        }
    }

    return res;
}
