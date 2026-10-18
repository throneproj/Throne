#pragma once

#include <QString>
#include <QList>
#include <QMutex>
#include <QVector>

#include <deque>
#include <string>

#include "include/database/entities/Profile.h"
#include "include/configs/generate.h"
#include "include/stats/traffic/TrafficLine.hpp"

namespace Stats {
    struct TrafficLooperEntry {
        QString tag;
        double downlink_rate = 0;
        double uplink_rate = 0;
    };

    inline QString DisplaySpeed(const std::shared_ptr<TrafficLooperEntry> &entry) {
        return UNICODE_LRO + QString("%1↑ %2↓").arg(ReadableSize(entry->uplink_rate), ReadableSize(entry->downlink_rate));
    }

    struct TrafficLooperGroup {
        QString watchTag;
        // watchTag as the core's stats-map key
        std::string watchTagKey;
        QList<std::shared_ptr<Configs::Profile>> profiles;
        int line = 0;
        // Set when the group credited a non-zero delta since the last persist.
        bool dirty = false;
    };

    class TrafficLooper {
    public:
        bool loop_enabled = false;
        bool looping = false;
        QMutex loop_mutex;

        std::shared_ptr<TrafficLooperEntry> proxy;
        std::shared_ptr<TrafficLooperEntry> direct;

        // Caller holds loop_mutex.
        void UpdateAll();

        void Loop();

        // Runs synchronously on the caller's thread, in one batched transaction.
        void PersistTraffic();

        void SetChainGroups(const QList<Configs::TrafficChainGroup>& configGroups);

    private:
        struct RateProbe {
            qint64 durationMs = 0;
            // [down, up] per line, the layout of TrafficSample::values.
            QVector<qint64> bytes;
        };

        QVector<double> WindowRates() const;

        QList<TrafficLooperGroup> groups;
        QList<TrafficLine> lines;
        QList<int> creditedIds;
        std::deque<RateProbe> rateWindow;
        qint64 lastProbeMs = 0;
    };

    extern TrafficLooper *trafficLooper;
} // namespace Stats
