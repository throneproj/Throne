#pragma once

#include <QList>
#include <QMutex>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>

#ifndef Q_MOC_RUN
#include <core/gen/libcore.pb.h>
#endif

#include "include/database/entities/IpList.h"
#include "include/database/entities/IpScan.h"
#include "include/database/entities/Profile.h"
#include "include/scanner/ScanManager.h"

namespace Scanner {
    class ScanWorker : public std::enable_shared_from_this<ScanWorker> {
    public:
        struct Hooks {
            std::function<void()> progress;
            std::function<void()> results;
            // Run thread, once, after the last DB write.
            std::function<void()> finished;
        };

        ScanWorker(const Configs::IpScan &scan, StartMode mode, std::shared_ptr<Configs::Profile> base, Hooks hooks);

        ScanWorker(const ScanWorker &) = delete;
        ScanWorker &operator=(const ScanWorker &) = delete;

        void Start();

        // The caller also sends StopScan, which ends the RPC in flight.
        void RequestStop();

        void DisablePersist();

        [[nodiscard]] bool IsStopping() const { return stop_.load(); }

        // The final status is stored; a later status write would overwrite it.
        [[nodiscard]] bool IsFinalized() const { return finalized_.load(); }

        [[nodiscard]] ScanLiveState LiveState() const;

        [[nodiscard]] int ScanId() const { return scanId_; }

        [[nodiscard]] QString SessionId() const { return sessionId_; }

    private:
        enum class Stage { Preparing, Probing, TestingConfigs, CheckingNetwork };

        struct ProbeOutcome {
            QString address;
            int port = 0;
            int probePort = 0;
            int latencyMs = 0;
            int maxQName = 0;
            int uploadMtu = 0;
            int downloadMtu = 0;
        };

        struct ChunkResult {
            QList<Configs::IpListEntry> hits;
            QList<Configs::IpListEntry> failures;
            QList<Configs::IpListEntry> localFailures;
            QList<Configs::IpListEntry> replaced;
            // A resolver that answered at all, even with an error, proves its address family reachable.
            bool answeredV4 = false;
            bool answeredV6 = false;
            bool aborted = false;
            QString error;
        };

        void run();
        QString execute(bool &completed);
        QString preparePass();
        QString prepareInitial();
        QString prepareRescan();
        QString loadSpec();
        QString prepareResolverProbe();
        QString runChunks(bool &completed);
        ChunkResult runChunk(const libcore::ScanTargetSpec &spec, quint64 chunkStart, int maxTargets, quint64 *nextCursor);
        void confirmFailures(ChunkResult &result, quint64 chunkStart, quint64 &next);
        QString testConfigs(const QList<ProbeOutcome> &candidates, ChunkResult &result);
        void addHit(ChunkResult &result, const ProbeOutcome &candidate, int port, int latencyMs) const;
        bool networkUp(const QStringList &targets, QString *detail);
        void finishPass(bool completed, const QString &error);
        void persistPassState();
        void saveProgress();
        void deleteList(int listId);
        bool countTargets(const libcore::ScanTargetSpec &spec, quint64 *total, QString *error);
        void fillProbeRequest(libcore::ScanProbeRequest &request) const;
        [[nodiscard]] int probeTimeoutMs(int chunkSize) const;
        [[nodiscard]] bool probePhaseEnabled() const;
        [[nodiscard]] bool rescanPass() const { return scan_.mode == Configs::IpScan::Mode::RescanResult; }

        void pollLoop();
        void pollOnce();
        void beginChunk(quint64 chunkStart, int chunkSize);
        void setStage(Stage stage);
        void setTested(quint64 tested, quint64 total);
        void pushLog(const QString &line);
        void notifyProgress() const;

        const int scanId_;
        const QString sessionId_;
        const StartMode startMode_;
        const std::shared_ptr<Configs::Profile> base_;
        const Hooks hooks_;
        const bool configPhase_;
        const bool resolverPhase_;
        const Configs::IpScan::Status initialStatus_;

        // Run thread only.
        Configs::IpScan scan_;
        libcore::ScanTargetSpec spec_;
        libcore::ScanDnsOptions dns_;
        int basePort_ = 0;
        bool listsTouched_ = false;

        std::atomic<bool> stop_{false};
        std::atomic<bool> persist_{true};
        std::atomic<bool> pollStop_{false};
        std::atomic<bool> finalized_{false};

        mutable QMutex mutex_;
        ScanLiveState live_;
        bool chunkActive_ = false;
        quint64 chunkStart_ = 0;
        quint64 chunkSize_ = 0;
        qint64 baseProbed_ = 0;
        qint64 baseProbeFailed_ = 0;
        qint64 baseUrlTested_ = 0;
        qint64 lastProbed_ = 0;
        qint64 lastProbePassed_ = 0;
        qint64 lastUrlTested_ = 0;

        // Poller thread only.
        qint64 lastSeq_ = 0;
    };

    // Thread-safe; the caller persists scan.result_list_id.
    int EnsureScanResultList(Configs::IpScan &scan);
} // namespace Scanner
