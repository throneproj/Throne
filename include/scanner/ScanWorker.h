#pragma once

#include <QHash>
#include <QList>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <limits>
#include <memory>

#ifndef Q_MOC_RUN
#include <core/gen/libcore.pb.h>
#endif

#include "include/database/entities/IpList.h"
#include "include/database/entities/IpScan.h"
#include "include/database/entities/Profile.h"
#include "include/scanner/ScanManager.h"

namespace Scanner {
    // A rescan removes a failed row only once a row of the same address family passed in this run:
    // a dead route, server or profile fails every row of a family at once.
    class RescanRemovals {
    public:
        // Whether adding these would make a row of that family removable.
        [[nodiscard]] bool WouldRemove(const QList<Configs::IpListEntry> &hits, const QList<Configs::IpListEntry> &failures,
                                       bool v6) const;

        void Add(const QList<Configs::IpListEntry> &hits, const QList<Configs::IpListEntry> &failures, quint64 chunkStart);

        // The failures of families that passed; the rest stay pending.
        QList<Configs::IpListEntry> TakeRemovable();

        // Where the rows still waiting for their family to pass begin; a pause resumes the rescan there.
        [[nodiscard]] quint64 UnresolvedFrom() const;

        [[nodiscard]] bool AnyPassed() const { return v4_.passed || v6_.passed; }

        [[nodiscard]] int PendingCount() const { return static_cast<int>(v4_.pending.size() + v6_.pending.size()); }

        // One line per family whose failures were kept; empty when no family passed.
        [[nodiscard]] QStringList KeptLines() const;

    private:
        struct Family {
            bool passed = false;
            QList<Configs::IpListEntry> pending;
            quint64 unresolvedFrom = std::numeric_limits<quint64>::max();
        };

        Family v4_;
        Family v6_;
    };

    // A DNS resolver scan: each target is tried as the DNS tunnel profile's only resolver.
    class ScanWorker : public ScanRun, public std::enable_shared_from_this<ScanWorker> {
    public:
        using Hooks = ScanRunHooks;

        ScanWorker(const Configs::IpScan &scan, StartMode mode, std::shared_ptr<Configs::Profile> base, Hooks hooks);

        ScanWorker(const ScanWorker &) = delete;
        ScanWorker &operator=(const ScanWorker &) = delete;

        void Start() override;

        void RequestStop() override;

        void DisablePersist() override;

        [[nodiscard]] bool IsStopping() const override { return stop_.load(); }

        [[nodiscard]] bool IsFinalized() const override { return finalized_.load(); }

        [[nodiscard]] ScanLiveState LiveState() const override;

        [[nodiscard]] int ScanId() const { return scanId_; }

        [[nodiscard]] QString SessionId() const override { return sessionId_; }

    private:
        enum class Stage { Preparing, Probing, CheckingNetwork };

        struct ProbeOutcome {
            QString address;
            int port = 0;
            int probePort = 0;
            int latencyMs = 0;
            int lossPct = -1;
        };

        struct ChunkResult {
            QList<Configs::IpListEntry> hits;
            QList<Configs::IpListEntry> failures;
            QList<Configs::IpListEntry> localFailures;
            QList<Configs::IpListEntry> replaced;
            // Failure key -> the core's dns_reason; shortNamesOnly holds the keys that passed only a short name.
            QHash<QString, QString> reasons;
            QSet<QString> shortNamesOnly;
            // A reply of any kind, even an error, proves the network up.
            bool replied = false;
            bool aborted = false;
            QString error;
        };

        struct FailureTally {
            int noAnswer = 0;
            int errorReply = 0;
            int wrongAnswer = 0;
            int nameTooLong = 0;
            int shortNamesOnly = 0;
            int other = 0;
            int heavyLoss = 0;
        };

        void run();
        QString execute(bool &completed);
        QString preparePass();
        QString prepareInitial();
        QString prepareRescan();
        QString loadSpec();
        QString prepareResolverProbe();
        QString preflight(bool &completed);
        QString runChunks(bool &completed);
        ChunkResult runChunk(const libcore::ScanTargetSpec &spec, quint64 chunkStart, int maxTargets, quint64 *nextCursor);
        ChunkResult runTargets(const QList<Configs::IpListEntry> &targets);
        void confirmFailures(ChunkResult &result, quint64 chunkStart, quint64 &next);
        void addHit(ChunkResult &result, const ProbeOutcome &candidate) const;
        bool storeHits(const ChunkResult &result);
        void tally(const ChunkResult &result);
        void logSummary();
        bool networkUp(QString *detail, QStringList *systemResolvers);
        void finishPass(bool completed, const QString &error);
        void persistPassState();
        void saveProgress();
        void deleteList(int listId);
        bool countTargets(const libcore::ScanTargetSpec &spec, quint64 *total, QString *error);
        void fillProbeRequest(libcore::ScanProbeRequest &request) const;
        [[nodiscard]] int probeTimeoutMs(int chunkSize) const;
        [[nodiscard]] bool rescanPass() const { return scan_.mode == Configs::IpScan::Mode::RescanResult; }
        [[nodiscard]] bool stopAfterReached() const;

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
        const Configs::IpScan::Status initialStatus_;

        // Run thread only.
        Configs::IpScan scan_;
        libcore::ScanTargetSpec spec_;
        libcore::ScanDnsOptions dns_;
        QList<Configs::IpListEntry> systemResolvers_;
        QList<Configs::IpListEntry> profileResolvers_;
        FailureTally tally_;
        // "Scan from the beginning": the pass's first hit replaces the old results.
        bool replacePending_ = false;
        // Rows this run added; the stop-after limit counts these, so Resume finds that many more.
        int runFound_ = 0;
        bool stoppedByLimit_ = false;
        RescanRemovals removals_;
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
        qint64 lastProbed_ = 0;

        // Poller thread only.
        qint64 lastSeq_ = 0;
    };

    // Thread-safe; the caller persists scan.result_list_id.
    int EnsureScanResultList(Configs::IpScan &scan);

    // Whether the old results still wait for the initial pass's first hit to replace them; kept on the pass's snapshot.
    [[nodiscard]] bool ReplacePending(const Configs::IpScan &scan);
    void ClearReplacePending(const Configs::IpScan &scan);
} // namespace Scanner
