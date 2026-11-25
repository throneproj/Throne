#pragma once

#include <QHostAddress>
#include <QList>
#include <QMutex>
#include <QSet>
#include <QString>

#include <atomic>
#include <memory>

#include "include/database/entities/IpList.h"
#include "include/database/entities/Profile.h"
#include "include/scanner/ScanManager.h"

namespace Scanner {
    // A WARP finder run for one profile, with no database access: port discovery over the seeds, sampled rounds, then a data
    // check that confirms handshake winners. It counts seconds of its time budget as tested/total; hits stay in LiveState().hits.
    class WarpWorker : public ScanRun, public std::enable_shared_from_this<WarpWorker> {
    public:
        WarpWorker(int finderId, std::shared_ptr<Configs::Profile> base, QList<Configs::IpListEntry> seeds, ScanRunHooks hooks);

        WarpWorker(const WarpWorker &) = delete;
        WarpWorker &operator=(const WarpWorker &) = delete;

        void Start() override;

        void RequestStop() override;

        void DisablePersist() override {}

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
        [[nodiscard]] QList<Pool> buildPools(const QList<Configs::IpListEntry> &entries) const;
        QString loadPools();
        QString search();
        QString discoverPorts(QList<Target> &seeds);
        QString tryJunk(const QList<Target> &targets, QList<Candidate> &found);
        void tryHttp2(const QList<Target> &targets, QList<Candidate> &found);
        void useHttp2(const std::shared_ptr<Configs::Profile> &copy, const QString &json);
        QString sampleRounds(const QList<Target> &seeds);
        QString probeTargets(const QList<Target> &targets, int attempts, bool confirm, QList<Candidate> *winners);
        ProbeBatch probe(const QList<Target> &targets, int attempts);
        QString guard(const ProbeBatch &batch);
        void addPending(const QList<Candidate> &winners);
        QString drainPending();
        QString dataCheck(const QList<Candidate> &batch, QList<Candidate> &confirmed);
        void storeConfirmed(const QList<Candidate> &confirmed);
        void addHits(const QList<Candidate> &confirmed);
        [[nodiscard]] bool stopRuleMet() const;
        [[nodiscard]] qint64 elapsedMs() const;
        [[nodiscard]] quint64 budgetCursor() const;
        bool networkUp(bool ipv6);
        void dropIpv6(const QString &why);
        QStringList discoveryAddresses(int count, bool anchors);
        QList<Target> roundTargets();
        static QString nextAddress(Pool &pool);
        void logOutcome();
        void finishRun(bool completed, const QString &error);
        void setStage(const QString &stage);
        void setTested(quint64 tested, quint64 total);
        void pushLog(const QString &line);
        void notifyProgress() const;
        [[nodiscard]] static QString networkDownError();
        [[nodiscard]] bool isMasque() const { return carrier_ == QLatin1String("masque"); }
        [[nodiscard]] int concurrencyFor(int attempts) const;
        [[nodiscard]] int chunkFor(int attempts) const;

        static bool better(const Candidate &a, const Candidate &b);
        static int lossPct(const Candidate &candidate);
        static Configs::IpListEntry entryOf(const Candidate &candidate);

        const QString sessionId_;
        const ScanRunHooks hooks_;
        const QList<Configs::IpListEntry> seeds_;

        // Run thread only.
        // A deep copy; junk and HTTP/2 mode swap in a copy with junk or the HTTP/2 fallback on.
        std::shared_ptr<Configs::Profile> base_;
        QString stage_;
        QString carrier_;
        QString probeJson_;
        QString testUrl_;
        QList<Pool> pools_;
        QList<int> ports_;
        int portCursor_ = 0;
        QList<Candidate> pending_;
        // The best confirmation of each address, best first.
        QList<Candidate> hits_;
        QSet<QString> confirmedAddresses_;
        qint64 lastConfirmMs_ = -1;
        qint64 networkUpAtMs_ = -1;
        // Budget spent when the search ended.
        qint64 searchMs_ = 0;
        bool answeredAny_ = false;
        bool junkMode_ = false;
        bool ipv6Allowed_ = true;

        // Also read by the ticker and LiveState().
        std::atomic<qint64> clockStartMs_{0};
        std::atomic<bool> searching_{false};

        std::atomic<bool> stop_{false};
        std::atomic<bool> finalized_{false};
        std::atomic<bool> tickStop_{false};

        mutable QMutex mutex_;
        ScanLiveState live_;
    };
} // namespace Scanner
