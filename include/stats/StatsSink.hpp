#pragma once

#include <QList>
#include <QString>
#include <QVector>

#include "include/stats/connections/connectionLister.hpp"
#include "include/stats/traffic/TrafficLine.hpp"

namespace Stats {
    // One looper emission. An idle tick (no profile runs) carries only monoMs.
    struct TrafficSample {
        qint64 monoMs = 0;
        bool coreRunning = false;
        // [down, up] per line of the last onTrafficLines, in its order; NaN = no rate yet.
        QVector<double> values;
        double proxyDown = 0;
        double proxyUp = 0;
        double directDown = 0;
        double directUp = 0;
        // Status-bar text; running ticks only.
        QString status;
        // Profiles whose traffic counters moved; running ticks only.
        QList<int> creditedIds;
    };

    // Main thread only: the loops hop to the main thread before calling it.
    class StatsSink {
    public:
        virtual ~StatsSink() = default;
        virtual void onTrafficLines(const QList<TrafficLine> &lines) = 0;
        virtual void onTrafficSample(const TrafficSample &sample) = 0;
        // The run ended; the next samples are idle ticks.
        virtual void onTrafficStopped() = 0;
        // Whole snapshot in the lister's order, never a delta.
        virtual void onConnections(const QList<ConnectionMetadata> &rows) = 0;
    };

    // Main thread only.
    inline StatsSink *statsSink = nullptr;

    inline void SetStatsSink(StatsSink *sink) { statsSink = sink; }

    inline StatsSink *CurrentStatsSink() { return statsSink; }
} // namespace Stats
