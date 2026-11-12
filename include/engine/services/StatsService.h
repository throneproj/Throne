#pragma once

#include <functional>
#include <optional>

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "include/engine/EngineTypes.h"
#include "include/stats/StatsSink.hpp"

class QTimer;

namespace Configs {
    struct TrafficChainGroup;
}

namespace Engine {
    // The connection list as the Throne API serves it, in Stats::ConnectionLister::ApiSink's order.
    struct ConnectionSnapshot {
        QList<Stats::ConnectionMetadata> rows;
        // Epoch ms of the poll; 0 = none yet.
        qint64 updatedAtMs = 0;
    };

    struct ConnectionCloseResult {
        // False when the core did not answer.
        bool rpcOK = false;
        int closed = 0;
        // The core's error, or the IPC error.
        QString error;
    };

    // Live traffic/connection feed of the running profile, and its live rates. Lives on the main thread; installs itself as
    // the Stats sink for its lifetime. Must outlive nothing: the loops reach it only through the main-thread sink pointer.
    class StatsService final : public QObject, private Stats::StatsSink {
        Q_OBJECT

    public:
        explicit StatsService(QObject *parent = nullptr);

        ~StatsService() override;

        // Main thread, after the core IPC client reconnected. Spawns the traffic and connection loops once; later calls no-op.
        void startLoops();

        // Any thread (the start worker), after a successful Start RPC. The new lines reach the main thread before any sample.
        void beginRun(const QList<Configs::TrafficChainGroup> &chainGroups);

        // Any thread. Tag -> profile name of the running config; {} on stop.
        void setOutboundNames(const QHash<QString, QString> &names);

        // Any thread (the stop worker); blocks on DB writes. Disables both loops, credits the last probe, persists, flushes.
        void endRun();

        // Any thread. Empties the connection view (connectionsUpdated({})) and the API snapshot, inline on main, queued elsewhere.
        void clearConnectionList();

        // Any thread; returns at once. The RPC runs on a new thread; a failure is logged, success re-polls the lister.
        static void closeConnections(const QStringList &ids);

        // Any thread; returns at once. As above, but done runs on the main thread in place of the failure log.
        static void closeConnections(const QStringList &ids, const std::function<void(const ConnectionCloseResult &)> &done);

        // Main thread. Renews the Throne API's 10 s watch, which keeps the lister polling at 1 Hz and the snapshot current.
        void watchConnections();

        // Main thread. Empty before the first poll of a watch, once the watch lapsed, and once the run ended.
        ConnectionSnapshot connectionSnapshot() const;

        // Main thread. Empty when stats are off or the last sample is older than maxAgeMs; the caller checks that a profile runs.
        std::optional<LiveRates> freshLiveRates(qint64 maxAgeMs) const;

        // Main thread. Lines of the current run, empty before the first one; for observers that attach late.
        QList<Stats::TrafficLine> trafficLines() const;

    signals:
        // All emitted on the main thread.
        void trafficLinesChanged(const QList<Stats::TrafficLine> &lines);

        void trafficSampled(const Stats::TrafficSample &sample);

        void trafficStopped();

        void connectionsUpdated(const QList<Stats::ConnectionMetadata> &rows);

    private:
        void onTrafficLines(const QList<Stats::TrafficLine> &lines) override;

        void onTrafficSample(const Stats::TrafficSample &sample) override;

        void onTrafficStopped() override;

        void onConnections(const QList<Stats::ConnectionMetadata> &rows) override;

        void onApiConnections(const QList<Stats::ConnectionMetadata> &rows, qint64 polledAtMs);

        LiveRates liveRates_;
        QElapsedTimer liveRatesAt_;
        QList<Stats::TrafficLine> lines_;
        bool loopsStarted_ = false;
        ConnectionSnapshot connections_;
        QTimer *connectionsWatch_ = nullptr;
    };
} // namespace Engine
