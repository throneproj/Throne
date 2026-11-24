#pragma once

#include <QHostAddress>
#include <QList>
#include <QMutex>
#include <QSet>
#include <QString>

#include <atomic>
#include <memory>

#include "include/database/entities/IpList.h"
#include "include/database/entities/IpScan.h"
#include "include/database/entities/Profile.h"
#include "include/scanner/ScanManager.h"
#include "include/scanner/ScanWorker.h"

namespace Scanner {
    // A WARP scan: known-good endpoints, port discovery, sampled rounds, then a data check that confirms handshake winners.
    // A search counts seconds of its time budget as cursor/total; a rescan counts the result rows it checked.
    class WarpWorker : public ScanRun, public std::enable_shared_from_this<WarpWorker> {
    public:
        WarpWorker(const Configs::IpScan &scan, StartMode mode, std::shared_ptr<Configs::Profile> base, ScanRunHooks hooks);

        // A finder run: the same search over seeds for base alone, with no database access; hits stay in LiveState().hits.
        WarpWorker(int finderId, std::shared_ptr<Configs::Profile> base, QList<Configs::IpListEntry> seeds, ScanRunHooks hooks);

        WarpWorker(const WarpWorker &) = delete;
        WarpWorker &operator=(const WarpWorker &) = delete;

        void Start() override;

        void RequestStop() override;

        void DisablePersist() override;

        [[nodiscard]] bool IsStopping() const override { return stop_.load(); }

        [[nodiscard]] bool IsFinalized() const override { return finalized_.load(); }

        [[nodiscard]] ScanLiveState LiveState() const override;

        [[nodiscard]] QString SessionId() const override { return sessionId_; }

    private:
        struct Target {
            QString address;
            // 0 = the core's default port for the carrier.
            int port = 0;
        };

        struct Candidate {
            QString address;
            // The port probed; entryPort is the one asked for.
            int port = 0;
            int entryPort = 0;
            int sent = 0;
            int answered = 0;
            int handshakeMs = 0;
        };

        // One /24, a smaller IPv4 range, or an IPv6 prefix sampled at random.
        struct Pool {
            bool v6 = false;
            quint32 v4First = 0;
            quint32 v4Count = 0;
            Q_IPV6ADDR v6Net{};
            int v6Prefix = 128;
            // The entry's own port; 0 = the ports discovery found.
            int port = 0;
            QSet<QString> used;
            bool exhausted = false;
        };

        struct ProbeBatch {
            QList<Candidate> winners;
            QList<Target> failures;
            int answered = 0;
            bool v4LocalFailure = false;
            bool v6LocalFailure = false;
            bool aborted = false;
            QString error;
        };

        void run();
        void tickLoop();
        QString execute(bool &completed);
        QString prepare();
        QString preparePass();
        QString prepareSearch();
        QString prepareRescan();
        [[nodiscard]] QList<Pool> buildPools(const QList<Configs::IpListEntry> &entries) const;
        QString loadPools();
        QString search();
        QString knownGood();
        QString discoverPorts(QList<Target> &seeds);
        QString tryJunk(const QList<Target> &targets, QList<Candidate> &found);
        void checkHttp3(const QList<Candidate> &found);
        void tryHttp2(const QList<Target> &targets, QList<Candidate> &found);
        void useHttp2(const std::shared_ptr<Configs::Profile> &copy, const QString &json);
        QString sampleRounds(const QList<Target> &seeds);
        QString rescan(bool &completed);
        QString rescanRows(const QList<Configs::IpListEntry> &rows, quint64 start);
        QString probeTargets(const QList<Target> &targets, int attempts, bool confirm, QList<Candidate> *winners,
                             QList<Target> *failures);
        ProbeBatch probe(const QList<Target> &targets, int attempts);
        QString guard(ProbeBatch &batch);
        void addPending(const QList<Candidate> &winners);
        QString drainPending();
        QString dataCheckAll(const QList<Candidate> &candidates, QList<Candidate> &confirmed, QList<Candidate> &failed);
        QString dataCheck(const QList<Candidate> &batch, QList<Candidate> &confirmed, QList<Candidate> &failed);
        void storeConfirmed(const QList<Candidate> &confirmed);
        void addHits(const QList<Candidate> &confirmed);
        void writeResults(const QList<Configs::IpListEntry> &hits, const QList<Configs::IpListEntry> &dead);
        [[nodiscard]] bool stopRuleMet() const;
        [[nodiscard]] qint64 elapsedMs() const;
        [[nodiscard]] quint64 budgetCursor() const;
        bool networkUp(bool ipv6, QString *detail);
        void dropIpv6(const QString &why);
        QStringList discoveryAddresses(int count, bool anchors);
        QList<Target> roundTargets();
        static QString nextAddress(Pool &pool);
        void logOutcome();
        void finishRun(bool completed, const QString &error);
        void persistState();
        void saveProgress();
        void deleteList(int listId);
        void setStage(const QString &stage);
        void setTested(quint64 tested, quint64 total);
        void pushLog(const QString &line);
        // The main log; a finder run shows everything in its own dialog instead.
        void mainLog(const QString &line) const;
        void notifyProgress() const;
        [[nodiscard]] QString networkDownError() const;
        [[nodiscard]] bool rescanPass() const { return scan_.mode == Configs::IpScan::Mode::RescanResult; }
        [[nodiscard]] bool rescanResumable() const;
        [[nodiscard]] bool isMasque() const { return carrier_ == QLatin1String("masque"); }
        [[nodiscard]] int concurrencyFor(int attempts) const;
        [[nodiscard]] int chunkFor(int attempts) const;

        static bool better(const Candidate &a, const Candidate &b);
        static int lossPct(const Candidate &candidate);
        static Configs::IpListEntry entryOf(const Candidate &candidate);

        const int scanId_;
        const QString sessionId_;
        const StartMode startMode_;
        const ScanRunHooks hooks_;
        const Configs::IpScan::Status initialStatus_;

        // Set before Start(): a finder run's seed entries stand in for the scan's base list.
        bool finder_ = false;
        QList<Configs::IpListEntry> seeds_;

        // Run thread only.
        Configs::IpScan scan_;
        // A deep copy; junk and HTTP/2 mode swap in a copy with that setting.
        std::shared_ptr<Configs::Profile> base_;
        QString stage_;
        QString carrier_;
        QString probeJson_;
        QString testUrl_;
        QList<Pool> pools_;
        QList<int> ports_;
        int portCursor_ = 0;
        QList<Candidate> pending_;
        // Finder runs: the best confirmation of each address, best first.
        QList<Candidate> hits_;
        QSet<QString> confirmedAddresses_;
        qint64 lastConfirmMs_ = -1;
        qint64 networkUpAtMs_ = -1;
        // Budget spent when the search ended.
        qint64 searchMs_ = 0;
        int runConfirmed_ = 0;
        bool answeredAny_ = false;
        bool junkMode_ = false;
        bool http2Mode_ = false;
        bool ipv6Allowed_ = true;
        // "Scan from the beginning": the pass's first confirmed endpoint replaces the old results.
        bool replacePending_ = false;
        RescanRemovals removals_;
        bool listsTouched_ = false;

        // Also read by the ticker and LiveState().
        std::atomic<qint64> clockStartMs_{0};
        // The budget a paused search had already spent.
        std::atomic<qint64> budgetBaseMs_{0};
        std::atomic<bool> searching_{false};

        std::atomic<bool> stop_{false};
        std::atomic<bool> persist_{true};
        std::atomic<bool> finalized_{false};
        std::atomic<bool> tickStop_{false};

        mutable QMutex mutex_;
        ScanLiveState live_;
    };
} // namespace Scanner
