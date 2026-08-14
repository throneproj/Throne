#pragma once

#include <QString>
#include <QList>
#include <QMutex>

#include <string>

#include "include/database/entities/Profile.h"
#include "include/configs/generate.h"

namespace Stats {
    // Aggregate rate accumulator used for the status-bar / traffic-graph
    // numbers (one for all proxied traffic combined, one for direct).
    struct TrafficLooperEntry {
        QString tag;
        double downlink_rate = 0;
        double uplink_rate = 0;
    };

    inline QString DisplaySpeed(const std::shared_ptr<TrafficLooperEntry> &entry) {
        return UNICODE_LRO + QString("%1↑ %2↓").arg(ReadableSize(entry->uplink_rate), ReadableSize(entry->downlink_rate));
    }

    // Runtime view of a TrafficChainGroup: same watchTag + profile list, plus
    // bookkeeping for delta-based rate computation.
    struct TrafficLooperGroup {
        QString watchTag;
        // watchTag as the core's stats-map key; precomputed once
        std::string watchTagKey;
        QList<std::shared_ptr<Configs::Profile>> profiles;
        long long last_update = 0;
        double uplink_rate = 0;
        double downlink_rate = 0;
        // Set when the group credited a non-zero delta since the last persist.
        // Auto-selector pools contribute one idle group per unselected member,
        // so persisting only dirty groups keeps that cost proportional to
        // traffic rather than to pool size.
        bool dirty = false;
    };

    class TrafficLooper {
    public:
        bool loop_enabled = false;
        bool looping = false;
        QMutex loop_mutex;

        std::shared_ptr<TrafficLooperEntry> proxy;
        std::shared_ptr<TrafficLooperEntry> direct;

        void UpdateAll();

        void Loop();

        // Persist every active profile's legacy traffic total to disk in one
        // batched transaction. Called on a slow cadence from the loop and once on
        // stop/exit; runs synchronously on the caller's thread (no thread spawn).
        void PersistTraffic();

        void SetChainGroups(const QList<Configs::TrafficChainGroup>& configGroups);

    private:
        QList<TrafficLooperGroup> groups;
        long long direct_last_update = 0;
    };

    extern TrafficLooper *trafficLooper;
} // namespace Stats
