#include "include/configs/outbounds/autoselector.h"

#include <QCoreApplication>

#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/entities/Group.h"

namespace Configs
{
    namespace Checks {
        // AutoSelectorPlan matches these against the two-letter country of each profile's last test.
        QString AutoSelectorCountries(const QString &text)
        {
            return Validation::EachItem(text, Validation::Split::Comma, [](const QString &item) {
                const QString code = item.startsWith('!') ? item.mid(1).trimmed() : item;
                if (code.size() == 2 && code[0].isLetter() && code[1].isLetter() && code[0].unicode() < 0x80 && code[1].unicode() < 0x80) return QString();
                return QCoreApplication::translate("Validation", "Use two-letter country codes separated by commas, for example DE,NL,FR; "
                                                                 "put ! before a code to exclude that country, for example !RU.");
            });
        }

        QString AutoSelectorUrl(const QString &text)
        {
            return Validation::Url(text, {"http", "https"});
        }

        QString AutoSelectorBuildLimit(int buildLimit, int poolCap)
        {
            if (buildLimit <= poolCap) return {};
            return QCoreApplication::translate("Validation", "Cannot run more profiles than are ranked (%1).").arg(poolCap);
        }

        QString AutoSelectorBenchInterval(int benchInterval, int interval)
        {
            if (benchInterval >= interval) return {};
            return QCoreApplication::translate("Validation", "The full sweep cannot run more often than the check interval (%1 s).").arg(interval);
        }

        QString AutoSelectorWatchInterval(int watchInterval, int interval)
        {
            if (watchInterval <= interval) return {};
            return QCoreApplication::translate("Validation", "Watching the profile in use cannot be slower than the check interval (%1 s).").arg(interval);
        }

        QString AutoSelectorReadyCount(int activeSize, int expected)
        {
            if (activeSize >= expected) return {};
            return QCoreApplication::translate("Validation", "Check closely at least as many profiles as are kept ready (%1).").arg(expected);
        }

        QString AutoSelectorCloseCount(int activeSize, int buildLimit)
        {
            if (activeSize <= buildLimit) return {};
            return QCoreApplication::translate("Validation", "Cannot check closely more profiles than are run (%1).").arg(buildLimit);
        }

        // A tier with neither would take every profile, leaving the tiers below it empty.
        QString AutoSelectorTierMatch(int gid, const QString &nameFilter)
        {
            if (gid >= 0 || !nameFilter.trimmed().isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Pick a group or enter a name pattern for this tier.");
        }

        QString AutoSelectorTierCount(int tiers)
        {
            if (tiers > 0) return {};
            return QCoreApplication::translate("Validation", "Add at least one tier to choose in priority order.");
        }

        QString AutoSelectorFailbackChecks(int failbackChecks, int sampling)
        {
            if (failbackChecks <= sampling) return {};
            return QCoreApplication::translate("Validation", "Cannot require more checks in a row than are kept (%1).").arg(sampling);
        }
    }

    QString autoSelector::DisplayAddress()
    {
        if (gids.isEmpty()) return QObject::tr("no group");
        QStringList names;
        for (int gid : std::as_const(gids)) {
            if (auto group = dataManager->groupsRepo->GetGroup(gid); group != nullptr) names << group->name;
        }
        if (names.isEmpty()) return QObject::tr("missing group");
        return names.join(QStringLiteral(", "));
    }

    QString autoSelector::TierLabel(int index) const
    {
        if (index < 0 || index >= tiers.size()) return QObject::tr("everything else");
        const auto &tier = tiers[index];
        QStringList parts;
        if (tier.gid >= 0) {
            const auto group = dataManager->groupsRepo->GetGroup(tier.gid);
            parts << (group != nullptr ? group->name : QObject::tr("missing group"));
        }
        if (const auto pattern = tier.nameFilter.trimmed(); !pattern.isEmpty()) parts << QObject::tr("names matching %1").arg(pattern);
        if (parts.isEmpty()) return QObject::tr("any profile");
        return parts.join(QStringLiteral(", "));
    }

    void autoSelector::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        const auto report = [&issues](const QString &field, const QString &message, const QString &value = {}) {
            if (!message.isEmpty()) issues << Validation::Issue{Validation::Severity::Error, field, message, value};
        };
        report("name_filter", Validation::Regex(nameFilter), nameFilter);
        report("country_filter", Checks::AutoSelectorCountries(countryFilter), countryFilter);
        report("test_url", Checks::AutoSelectorUrl(testURL), testURL);
        report("connectivity_url", Checks::AutoSelectorUrl(connectivityURL), connectivityURL);
        if (!Prioritized()) return;
        report("tiers", Checks::AutoSelectorTierCount(static_cast<int>(tiers.size())));
        for (qsizetype i = 0; i < tiers.size(); ++i) {
            const auto field = QStringLiteral("tiers[%1]").arg(i);
            report(field, Checks::AutoSelectorTierMatch(tiers[i].gid, tiers[i].nameFilter));
            report(field + ".name_filter", Validation::Regex(tiers[i].nameFilter), tiers[i].nameFilter);
        }
    }
}
