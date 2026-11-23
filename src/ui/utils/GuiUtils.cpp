#include "include/ui/utils/GuiUtils.hpp"

#include <QCoreApplication>
#include <QSet>

#include <algorithm>

#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"

QString ImportTargetRefusalText(const Engine::ServiceError &error) {
    if (error.code == QLatin1String("subscription_group"))
        return QObject::tr("This group is a subscription: its next update removes profiles imported into it. Select another group, then import again.");
    if (error.code == QLatin1String("group_archived")) return QObject::tr("This group is archived. Select another group, then import again.");
    if (error.code == QLatin1String("not_found")) return QObject::tr("This group no longer exists.");
    return error.message;
}

QString SettingsApplyErrorText(const Engine::ServiceError &error) {
    if (error.code == QLatin1String("port_conflict"))
        return QObject::tr("Two of these settings would use the same port. Choose a different port, then try again.");
    if (error.code == QLatin1String("exiting")) return QObject::tr("Throne is shutting down, so the settings were not saved.");
    return error.message;
}

QString HopReferenceWarning(const QList<int> &profileIds, int deletedGroup) {
    const auto refs = Configs::dataManager->groupsRepo->HopReferences(QSet<int>(profileIds.cbegin(), profileIds.cend()), deletedGroup);
    if (refs.isEmpty()) return {};
    constexpr qsizetype shown = 5;
    QString text = QStringLiteral("\n\n") + QObject::tr("These groups use a profile being deleted as their front or landing proxy. It will be removed from them, so they will connect without it:");
    for (qsizetype i = 0; i < std::min(refs.size(), shown); ++i) {
        const auto &ref = refs[i];
        text += QLatin1Char('\n') + (ref.role == Configs::HopRole::Front ? QObject::tr("%1 (front proxy)") : QObject::tr("%1 (landing proxy)")).arg(ref.groupName);
    }
    if (refs.size() > shown) text += QLatin1Char('\n') + QCoreApplication::translate("Validation", "…and %n more", nullptr, static_cast<int>(refs.size() - shown));
    return text;
}
