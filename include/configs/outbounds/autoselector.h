#pragma once
#include "include/configs/common/Outbound.h"

#include <QJsonArray>

namespace Configs
{
    namespace Checks {
        QString AutoSelectorCountries(const QString &text);
        QString AutoSelectorUrl(const QString &text);
        QString AutoSelectorBuildLimit(int buildLimit, int poolCap);
        QString AutoSelectorBenchInterval(int benchInterval, int interval);
        QString AutoSelectorWatchInterval(int watchInterval, int interval);
        QString AutoSelectorReadyCount(int activeSize, int expected);
        QString AutoSelectorCloseCount(int activeSize, int buildLimit);
    }

    // Membership resolves at build time from gids; ranking is client-side, best first in pool.
    class autoSelector : public outbound
    {
    public:
        bool HasServerEndpoint() override { return false; }

        QList<int> gids; // tracked groups, in the user's order
        QString nameFilter;             // regex over the profile name, empty = all
        QString countryFilter;          // comma-separated ISO codes, empty = all
        bool excludeUnavailable = true;

        int poolCap = 1000;
        int buildLimit = 300; // how many of the ranked pool enter the config

        // Minutes an existing URL-test result stays trustworthy; 0 always re-tests.
        int resultValidityMins = 1440;

        QString testURL;         // empty = fall back to the global test URL
        QString connectivityURL; // direct probe used to spot a local outage
        int intervalSec = 300;
        int benchIntervalSec = 600;
        // Re-check interval for the in-use member; a break that still accepts connections raises no error elsewhere.
        int watchIntervalSec = 15;
        int activeSize = 8;
        int sampling = 10;
        int toleranceMs = 300;
        int maxRTTms = 0; // 0 = no ceiling
        // How many members are kept confirmed working: the instant-failover pool, not a balancing setting.
        int expected = 3;
        int dialRetries = 2;
        bool interruptOnSwitch = true;

        // Off by default: a single sticky pick keeps session affinity and exact traffic accounting.
        bool balance = false;
        QString balanceMode = "rotate"; // "rotate" | "connection"
        int balanceIntervalSec = 30;

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
            if (object.contains("exclude_unavailable")) excludeUnavailable = object["exclude_unavailable"].toBool(true);
            if (object.contains("pool_cap")) poolCap = object["pool_cap"].toInt(3000);
            if (object.contains("build_limit")) buildLimit = object["build_limit"].toInt(300);
            if (object.contains("result_validity_mins")) resultValidityMins = object["result_validity_mins"].toInt(60);
            if (object.contains("test_url")) testURL = object["test_url"].toString();
            if (object.contains("connectivity_url")) connectivityURL = object["connectivity_url"].toString();
            if (object.contains("interval_sec")) intervalSec = object["interval_sec"].toInt(60);
            if (object.contains("bench_interval_sec")) benchIntervalSec = object["bench_interval_sec"].toInt(600);
            if (object.contains("watch_interval_sec")) watchIntervalSec = object["watch_interval_sec"].toInt(15);
            if (object.contains("active_size")) activeSize = object["active_size"].toInt(10);
            if (object.contains("sampling")) sampling = object["sampling"].toInt(10);
            if (object.contains("tolerance_ms")) toleranceMs = object["tolerance_ms"].toInt(100);
            if (object.contains("max_rtt_ms")) maxRTTms = object["max_rtt_ms"].toInt(0);
            if (object.contains("expected")) expected = object["expected"].toInt(3);
            if (object.contains("dial_retries")) dialRetries = object["dial_retries"].toInt(2);
            if (object.contains("interrupt_on_switch")) interruptOnSwitch = object["interrupt_on_switch"].toBool(true);
            if (object.contains("balance")) balance = object["balance"].toBool(false);
            if (object.contains("balance_mode")) balanceMode = object["balance_mode"].toString("rotate");
            if (object.contains("balance_interval_sec")) balanceIntervalSec = object["balance_interval_sec"].toInt(30);
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
            object["exclude_unavailable"] = excludeUnavailable;
            object["pool_cap"] = poolCap;
            object["build_limit"] = buildLimit;
            object["result_validity_mins"] = resultValidityMins;
            object["test_url"] = testURL;
            object["connectivity_url"] = connectivityURL;
            object["interval_sec"] = intervalSec;
            object["bench_interval_sec"] = benchIntervalSec;
            object["watch_interval_sec"] = watchIntervalSec;
            object["active_size"] = activeSize;
            object["sampling"] = sampling;
            object["tolerance_ms"] = toleranceMs;
            object["max_rtt_ms"] = maxRTTms;
            object["expected"] = expected;
            object["dial_retries"] = dialRetries;
            object["interrupt_on_switch"] = interruptOnSwitch;
            object["balance"] = balance;
            object["balance_mode"] = balanceMode;
            object["balance_interval_sec"] = balanceIntervalSec;
            object["pool"] = QListInt2QJsonArray(pool);
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
            if (poolCap < 1) poolCap = 1;
            if (poolCap > kMaxPoolCap) poolCap = kMaxPoolCap;
            if (buildLimit < 1) buildLimit = 1;
            if (buildLimit > kMaxBuildLimit) buildLimit = kMaxBuildLimit;
            if (buildLimit > poolCap) buildLimit = poolCap;
            if (resultValidityMins < 0) resultValidityMins = 0;
            if (intervalSec < 10) intervalSec = 10;
            if (benchIntervalSec < intervalSec) benchIntervalSec = intervalSec;
            if (watchIntervalSec < 5) watchIntervalSec = 5;
            // Watching slower than the full round makes it dead weight.
            if (watchIntervalSec > intervalSec) watchIntervalSec = intervalSec;
            if (activeSize < 1) activeSize = 1;
            if (expected < 1) expected = 1;
            // The ready set must sit inside the closely-checked set, or readiness rides on stale bench data.
            if (activeSize < expected) activeSize = expected;
            if (activeSize > buildLimit) activeSize = buildLimit;
            if (sampling < 2) sampling = 2;
            if (sampling > 60) sampling = 60;
            if (toleranceMs < 0) toleranceMs = 0;
            if (maxRTTms < 0) maxRTTms = 0;
            if (expected > buildLimit) expected = buildLimit;
            if (dialRetries < 0) dialRetries = 0;
            if (dialRetries > 5) dialRetries = 5;
            if (balanceIntervalSec < 5) balanceIntervalSec = 5;
            if (balanceMode != "connection") balanceMode = "rotate";
        }

        static constexpr int kMaxPoolCap = 3000;
        static constexpr int kMaxBuildLimit = 500;

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
