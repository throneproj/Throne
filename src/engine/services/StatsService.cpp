#include "include/engine/services/StatsService.h"

#include <QMetaObject>
#include <QThread>

#include "include/api/RPC.h"
#include "include/configs/generate.h"
#include "include/database/DatabaseManager.h"
#include "include/engine/EngineTr.h"
#include "include/engine/ThreadCheck.h"
#include "include/global/Utils.hpp"
#include "include/stats/traffic/TrafficLooper.hpp"
#include "include/stats/traffic/TrafficStatsManager.hpp"

namespace Engine {
    StatsService::StatsService(QObject *parent) : QObject(parent) {
        Stats::SetStatsSink(this);
    }

    StatsService::~StatsService() {
        // The pointer is main-only, so posted loop callbacks still pending see null.
        if (Stats::CurrentStatsSink() == this) Stats::SetStatsSink(nullptr);
    }

    void StatsService::startLoops() {
        ENGINE_CHECK_MAIN_THREAD();
        if (loopsStarted_) return;
        loopsStarted_ = true;
        runOnNewThread([] { Stats::trafficLooper->Loop(); });
        runOnNewThread([] { Stats::connection_lister->Loop(); });
    }

    void StatsService::beginRun(const QList<Configs::TrafficChainGroup> &chainGroups) {
        Stats::trafficLooper->SetChainGroups(chainGroups);
        Stats::trafficLooper->loop_enabled = true;
        Stats::connection_lister->suspend = false;
    }

    void StatsService::setOutboundNames(const QHash<QString, QString> &names) {
        Stats::connection_lister->SetOutboundNames(names);
    }

    void StatsService::endRun() {
        Stats::trafficLooper->loop_enabled = false;
        Stats::connection_lister->suspend = true;
        Stats::trafficLooper->loop_mutex.lock();
        Stats::trafficLooper->UpdateAll();
        Stats::trafficLooper->loop_mutex.unlock();
        Stats::trafficLooper->PersistTraffic();
        Stats::trafficStatsManager->Flush();
    }

    void StatsService::clearConnectionList() {
        if (QThread::currentThread() == thread()) {
            emit connectionsUpdated({});
        } else {
            QMetaObject::invokeMethod(this, [this] { emit connectionsUpdated({}); }, Qt::QueuedConnection);
        }
    }

    void StatsService::closeConnections(const QStringList &ids) {
        if (ids.isEmpty()) return;
        runOnNewThread([ids] {
            bool rpcOK = false;
            const auto err = API::defaultClient->CloseConnections(&rpcOK, ids);
            if (!rpcOK || !err.isEmpty()) {
                MW_show_log(MainWindowTr::tr("Failed to close connections: %1").arg(err.isEmpty() ? MainWindowTr::tr("IPC error") : err));
                return;
            }
            Stats::connection_lister->ForceUpdate();
        });
    }

    std::optional<LiveRates> StatsService::freshLiveRates(qint64 maxAgeMs) const {
        if (Configs::dataManager->settingsRepo->disable_traffic_stats || !liveRatesAt_.isValid() || liveRatesAt_.hasExpired(maxAgeMs)) {
            return std::nullopt;
        }
        return liveRates_;
    }

    QList<Stats::TrafficLine> StatsService::trafficLines() const {
        return lines_;
    }

    void StatsService::onTrafficLines(const QList<Stats::TrafficLine> &lines) {
        ENGINE_CHECK_MAIN_THREAD();
        lines_ = lines;
        emit trafficLinesChanged(lines);
    }

    void StatsService::onTrafficSample(const Stats::TrafficSample &sample) {
        ENGINE_CHECK_MAIN_THREAD();
        liveRates_ = LiveRates{
            .proxyUp = static_cast<qint64>(sample.proxyUp),
            .proxyDown = static_cast<qint64>(sample.proxyDown),
            .directUp = static_cast<qint64>(sample.directUp),
            .directDown = static_cast<qint64>(sample.directDown),
        };
        liveRatesAt_.start();
        emit trafficSampled(sample);
    }

    void StatsService::onTrafficStopped() {
        ENGINE_CHECK_MAIN_THREAD();
        emit trafficStopped();
    }

    void StatsService::onConnections(const QList<Stats::ConnectionMetadata> &rows) {
        ENGINE_CHECK_MAIN_THREAD();
        emit connectionsUpdated(rows);
    }
} // namespace Engine
