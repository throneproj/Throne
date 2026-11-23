#pragma once
#include "include/configs/common/Outbound.h"

#include <QJsonArray>

#include <algorithm>

namespace Configs
{
    namespace Checks {
        QString AutoSelectorCountries(const QString &text);
        QString AutoSelectorUrl(const QString &text);
        QString AutoSelectorTierMatch(int gid, const QString &nameFilter);
        QString AutoSelectorTierCount(int tiers);
    }

    // Membership resolves at build time from gids; ranking is client-side, best first in pool.
    class autoSelector : public outbound
    {
    public:
        // Defaults of the saved settings, and the fixed values of those no longer saved.
        static constexpr int kDefaultBuildLimit = 300;
        static constexpr int kDefaultIntervalSec = 300;
        static constexpr int kDefaultToleranceMs = 300;
        static constexpr int kDefaultExpected = 3;
        static constexpr int kMinToleranceMs = 10;
        static constexpr int kMaxBuildLimit = 500;
        static constexpr int kPoolCap = 3000;
        static constexpr int kSuccessValiditySecs = 86400;
        static constexpr int kFailureValiditySecs = 3600;
        static constexpr int kWatchIntervalSec = 15;
        static constexpr int kSampling = 10;
        static constexpr int kDialRetries = 2;
        static constexpr int kFailbackChecks = 3;
        static constexpr int kFailbackDelaySec = 60;
        static constexpr int kSiteMemoryMins = 60;
        static constexpr int kBalanceIntervalSec = 30;
        static constexpr int kCloseMargin = 5;

        bool HasServerEndpoint() override { return false; }

        QList<int> gids; // tracked groups, in the user's order
        QString nameFilter;             // regex over the profile name, empty = all
        QString countryFilter;          // comma-separated ISO codes, "!XX" excludes one, empty = all

        int buildLimit = kDefaultBuildLimit; // how many of the ranked pool enter the config

        QString testURL;         // empty = fall back to the global test URL
        QString connectivityURL; // direct probe used to spot a local outage
        int intervalSec = kDefaultIntervalSec;
        int toleranceMs = kDefaultToleranceMs;
        int maxRTTms = 0; // 0 = no ceiling
        // How many members are kept confirmed working: the instant-failover pool, not a balancing setting.
        int expected = kDefaultExpected;
        bool interruptOnSwitch = true;

        // Off by default: a single sticky pick keeps session affinity and exact traffic accounting.
        bool balance = false;
        QString balanceMode = "rotate"; // "rotate" | "connection"

        QString strategy = "latency"; // "latency" | "priority"

        struct Tier {
            int gid = -1; // -1 = any tracked group
            QString nameFilter;
        };

        // A member takes the first tier it matches; members matching none form an implicit last tier.
        QList<Tier> tiers;

        // Also judge members by how the real connections they carry end, not only by probes.
        bool passive = false;

        // Members the core re-checks every interval: the ready set plus a margin of challengers.
        [[nodiscard]] int ActiveSize() const { return std::min(expected + kCloseMargin, buildLimit); }

        [[nodiscard]] int BenchIntervalSec(int built) const { return std::max(intervalSec, std::clamp(2 * built, 600, 1800)); }

        // Re-checks the in-use member; a break that still accepts connections raises no error elsewhere.
        [[nodiscard]] int WatchIntervalSec() const { return std::min(kWatchIntervalSec, intervalSec); }

        [[nodiscard]] bool Prioritized() const { return strategy == "priority"; }

        // Index tiers.size() names the members that match no tier.
        [[nodiscard]] QString TierLabel(int index) const;

        // Ranked membership from the last client-side sweep, best first.
        QList<int> pool;
        qint64 poolRankedAt = 0;
        // Hand-picked member, -1 = automatic; the core drops it if that profile is not in the built pool.
        int pinnedID = -1;
        QList<int> lastBuilt;
        qint64 lastBuiltAt = 0;
        // Compact usage history, newest first: see HistoryEntry.
        QJsonArray history;

        QString DisplayType() override { return QObject::tr("Auto Selector"); }

        // The tracked groups' names; defined in the .cpp because it reaches the group repository.
        QString DisplayAddress() override;

        [[nodiscard]] bool Tracks(int gid) const { return gids.contains(gid); }

        // No security of its own; it inherits whatever its members use.
        SecurityInfo GetSecurity() override { return {}; }

        // ParseFromJson() already ran Normalize(), so only the free-text fields can be wrong here.
        void Validate(Validation::Issues &issues) override;

        bool ParseFromJson(const QJsonObject &object) override
        {
            if (object.isEmpty()) return false;
            if (object.contains("name")) name = object["name"].toString();
            // "gid" is the single-group form older builds wrote, and still read.
            if (object.contains("gids")) {
                gids = QJsonArray2QListInt(object["gids"].toArray());
            } else if (const int gid = object["gid"].toInt(-1); gid >= 0) {
                gids = {gid};
            }
            if (object.contains("name_filter")) nameFilter = object["name_filter"].toString();
            if (object.contains("country_filter")) countryFilter = object["country_filter"].toString();
            if (object.contains("build_limit")) buildLimit = object["build_limit"].toInt(kDefaultBuildLimit);
            if (object.contains("test_url")) testURL = object["test_url"].toString();
            if (object.contains("connectivity_url")) connectivityURL = object["connectivity_url"].toString();
            if (object.contains("interval_sec")) intervalSec = object["interval_sec"].toInt(kDefaultIntervalSec);
            if (object.contains("tolerance_ms")) toleranceMs = object["tolerance_ms"].toInt(kDefaultToleranceMs);
            if (object.contains("max_rtt_ms")) maxRTTms = object["max_rtt_ms"].toInt(0);
            if (object.contains("expected")) expected = object["expected"].toInt(kDefaultExpected);
            if (object.contains("interrupt_on_switch")) interruptOnSwitch = object["interrupt_on_switch"].toBool(true);
            if (object.contains("balance")) balance = object["balance"].toBool(false);
            if (object.contains("balance_mode")) balanceMode = object["balance_mode"].toString("rotate");
            if (object.contains("strategy")) strategy = object["strategy"].toString("latency");
            if (object.contains("tiers")) {
                QList<Tier> parsed;
                for (const auto &value : object["tiers"].toArray()) {
                    const auto entry = value.toObject();
                    parsed << Tier{entry["gid"].toInt(-1), entry["name_filter"].toString()};
                }
                tiers = parsed;
            }
            if (object.contains("passive")) passive = object["passive"].toBool(false);
            if (object.contains("pool")) pool = QJsonArray2QListInt(object["pool"].toArray());
            if (object.contains("pool_ranked_at")) poolRankedAt = static_cast<qint64>(object["pool_ranked_at"].toDouble());
            if (object.contains("pinned_id")) pinnedID = object["pinned_id"].toInt(-1);
            if (object.contains("last_built")) lastBuilt = QJsonArray2QListInt(object["last_built"].toArray());
            if (object.contains("last_built_at")) lastBuiltAt = static_cast<qint64>(object["last_built_at"].toDouble());
            if (object.contains("history")) history = object["history"].toArray();
            Normalize();
            return true;
        }

        QJsonObject ExportToJson() override
        {
            QJsonObject object;
            object["name"] = name;
            object["type"] = "autoselector";
            object["gids"] = QListInt2QJsonArray(gids);
            object["gid"] = gids.value(0, -1);
            object["name_filter"] = nameFilter;
            object["country_filter"] = countryFilter;
            object["build_limit"] = buildLimit;
            object["test_url"] = testURL;
            object["connectivity_url"] = connectivityURL;
            object["interval_sec"] = intervalSec;
            object["tolerance_ms"] = toleranceMs;
            object["max_rtt_ms"] = maxRTTms;
            object["expected"] = expected;
            object["interrupt_on_switch"] = interruptOnSwitch;
            object["balance"] = balance;
            object["balance_mode"] = balanceMode;
            object["strategy"] = strategy;
            QJsonArray tierArray;
            for (const auto &tier : tiers) tierArray.append(QJsonObject{{"gid", tier.gid}, {"name_filter", tier.nameFilter}});
            object["tiers"] = tierArray;
            object["passive"] = passive;
            object["pool"] =QListInt2QJsonArray(pool);
            object["pool_ranked_at"] = static_cast<double>(poolRankedAt);
            object["pinned_id"] = pinnedID;
            object["last_built"] = QListInt2QJsonArray(lastBuilt);
            object["last_built_at"] = static_cast<double>(lastBuiltAt);
            object["history"] = history;
            return object;
        }

        BuildResult Build() override
        {
            // The group outbound is assembled in generate.cpp, where members are resolvable.
            return {{}, "Cannot call Build on an auto selector config"};
        }

        // Clamps what a hand-edited profile could put out of range; runs before every build.
        void Normalize()
        {
            QList<int> tracked;
            for (int gid : std::as_const(gids)) {
                if (gid >= 0 && !tracked.contains(gid)) tracked << gid;
            }
            gids = tracked;
            if (buildLimit < 1) buildLimit = 1;
            if (buildLimit > kMaxBuildLimit) buildLimit = kMaxBuildLimit;
            if (intervalSec < 10) intervalSec = 10;
            if (expected < 1) expected = 1;
            if (expected > buildLimit) expected = buildLimit;
            if (toleranceMs < kMinToleranceMs) toleranceMs = kMinToleranceMs;
            if (maxRTTms < 0) maxRTTms = 0;
            if (balanceMode != "connection") balanceMode = "rotate";
            if (strategy != "priority") strategy = "latency";
            for (auto &tier : tiers) {
                if (tier.gid < -1) tier.gid = -1;
            }
        }

        struct HistoryEntry {
            int id = -1;
            qint64 firstUsed = 0;
            qint64 lastUsed = 0;
            int builds = 0;
            int failures = 0;
            QString name;
        };

        static constexpr int kMaxHistoryEntries = 2000;

        [[nodiscard]] QList<HistoryEntry> HistoryEntries() const
        {
            QList<HistoryEntry> entries;
            entries.reserve(history.size());
            for (const auto &value : history) {
                const auto object = value.toObject();
                HistoryEntry entry;
                entry.id = object["id"].toInt(-1);
                entry.firstUsed = static_cast<qint64>(object["first"].toDouble());
                entry.lastUsed = static_cast<qint64>(object["last"].toDouble());
                entry.builds = object["builds"].toInt();
                entry.failures = object["fails"].toInt();
                entry.name = object["name"].toString();
                if (entry.id >= 0) entries << entry;
            }
            return entries;
        }

        // Newest-first, so the LRU trim at the tail drops the least recently used.
        void RecordHistory(const QList<int> &ids, const QHash<int, QString> &names, qint64 now)
        {
            QHash<int, HistoryEntry> byID;
            QList<int> order;
            for (const auto &entry : HistoryEntries()) {
                byID.insert(entry.id, entry);
                order << entry.id;
            }
            for (int id : ids) {
                auto entry = byID.value(id);
                if (entry.id < 0) {
                    entry.id = id;
                    entry.firstUsed = now;
                } else {
                    order.removeOne(id);
                }
                entry.lastUsed = now;
                entry.builds++;
                if (names.contains(id)) entry.name = names.value(id);
                byID.insert(id, entry);
                order.prepend(id);
            }
            while (order.size() > kMaxHistoryEntries) {
                byID.remove(order.takeLast());
            }
            QJsonArray out;
            for (int id : order) {
                const auto entry = byID.value(id);
                out.append(QJsonObject{
                    {"id", entry.id},
                    {"first", static_cast<double>(entry.firstUsed)},
                    {"last", static_cast<double>(entry.lastUsed)},
                    {"builds", entry.builds},
                    {"fails", entry.failures},
                    {"name", entry.name},
                });
            }
            history = out;
        }

        void RecordHistoryFailure(int id)
        {
            for (int i = 0; i < history.size(); i++) {
                auto object = history[i].toObject();
                if (object["id"].toInt(-1) != id) continue;
                object["fails"] = object["fails"].toInt() + 1;
                history[i] = object;
                return;
            }
        }
    };
}
