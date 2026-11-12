#include "include/scanner/DefaultIpLists.h"

#include "include/database/DatabaseManager.h"
#include "include/database/IpListsRepo.h"
#include "include/scanner/IpListParse.h"
#include "include/scanner/WarpPresets.h"

#include <QFile>

#include <algorithm>

namespace {
    struct DefaultIpListSpec {
        QString name;
        // Built-in rule-set key; empty = a manual list.
        QString ruleSet;
        QString seedResource;
        const QStringList *seedCidrs = nullptr;
    };

    QList<DefaultIpListSpec> defaultIpListSpecs() {
        using namespace Scanner::DefaultIpLists;
        return {
            {kCloudflareCdnName, QStringLiteral("geoip-cloudflare"), QStringLiteral(":/scanner/cloudflare-cdn.txt")},
            {kFastlyCdnName, QStringLiteral("geoip-fastly"), QStringLiteral(":/scanner/fastly-cdn.txt")},
            {kGcoreCdnName, QStringLiteral("geoip-gcore"), QStringLiteral(":/scanner/gcore-cdn.txt")},
            {kWarpWireGuardName, {}, {}, &Scanner::WarpPresets::kWireGuardRanges},
            {kWarpMasqueName, {}, {}, &Scanner::WarpPresets::kMasqueEndpoints},
            {kIranResolversName, {}, QStringLiteral(":/scanner/iran-resolvers.txt")},
            {kRussiaResolversName, {}, QStringLiteral(":/scanner/russia-resolvers.txt")},
        };
    }

    int defaultIpListFind(const QString &name) {
        for (const auto &list : Configs::dataManager->ipListsRepo->GetAllIpLists()) {
            if (list->role == Configs::IpList::Role::User && list->name == name) return list->id;
        }
        return -1;
    }

    std::shared_ptr<Configs::IpList> defaultIpListBuild(const DefaultIpListSpec &spec) {
        auto list = Configs::IpListsRepo::NewIpList();
        list->name = spec.name;
        if (!spec.ruleSet.isEmpty()) {
            list->source_kind = Configs::IpList::SourceKind::RuleSet;
            list->source = spec.ruleSet;
        }
        if (spec.seedCidrs != nullptr) {
            list->entries = Scanner::EntriesFromCidrs(*spec.seedCidrs, 0);
        } else {
            QFile file(spec.seedResource);
            if (!file.open(QIODevice::ReadOnly)) return nullptr;
            list->entries = Scanner::ParseIpListText(file.readAll()).entries;
        }
        if (list->entries.isEmpty()) return nullptr;
        list->entryCount = static_cast<int>(list->entries.size());
        list->entriesLoaded = true;
        return list;
    }
} // namespace

namespace Scanner::DefaultIpLists {
    int Ensure(const QString &name) {
        if (const int id = defaultIpListFind(name); id >= 0) return id;
        for (const auto &spec : defaultIpListSpecs()) {
            if (spec.name != name) continue;
            auto list = defaultIpListBuild(spec);
            if (list == nullptr || !Configs::dataManager->ipListsRepo->AddIpList(list)) return -1;
            return list->id;
        }
        return -1;
    }

    int EnsureAll() {
        int created = 0;
        for (const auto &spec : defaultIpListSpecs()) {
            if (defaultIpListFind(spec.name) >= 0) continue;
            if (Ensure(spec.name) >= 0) ++created;
        }
        return created;
    }

    bool MigrateWarpMasqueRanges() {
        const int id = defaultIpListFind(kWarpMasqueName);
        if (id < 0) return true;
        static const QStringList covered = {
            QStringLiteral("162.159.198.1"), QStringLiteral("162.159.198.2"),
            QStringLiteral("162.159.199.1"), QStringLiteral("162.159.199.2"),
        };
        const auto entries = Configs::dataManager->ipListsRepo->GetEntries(id);
        const auto contains = [&entries](const Configs::IpListEntry &wanted) {
            return std::any_of(entries.cbegin(), entries.cend(), [&wanted](const Configs::IpListEntry &entry) {
                return entry.cidr == wanted.cidr && entry.port == wanted.port;
            });
        };
        QList<Configs::IpListEntry> ranges;
        for (const auto &range : Scanner::EntriesFromCidrs({QStringLiteral("162.159.198.0/24"), QStringLiteral("162.159.199.0/24")}, 0)) {
            if (!contains(range)) ranges << range;
        }

        QList<Configs::IpListEntry> migrated;
        migrated.reserve(entries.size() + ranges.size());
        bool placed = false;
        bool changed = !ranges.isEmpty();
        for (const auto &entry : entries) {
            // Only the seeded portless hosts: an entry with its own port is not covered by a portless range.
            if (entry.port == 0 && covered.contains(entry.cidr)) {
                // The ranges take the first seed's place, so a migrated list reads like a fresh seed.
                if (!placed) migrated << ranges;
                placed = true;
                changed = true;
                continue;
            }
            migrated << entry;
        }
        if (!changed) return true;
        if (!placed) migrated << ranges;
        return Configs::dataManager->ipListsRepo->ReplaceEntries(id, migrated);
    }
} // namespace Scanner::DefaultIpLists
