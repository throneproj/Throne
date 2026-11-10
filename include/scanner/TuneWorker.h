#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <atomic>
#include <deque>
#include <memory>

#include "include/database/entities/IpScan.h"
#include "include/database/entities/Profile.h"
#include "include/scanner/ScanManager.h"
#include "include/scanner/TunePlanner.h"

namespace Configs {
    class BuildTestConfigResult;
}

namespace Scanner {
    // Runs a Tune scan: one variant per attempt and one attempt in flight, paced, with the network checked at every stage
    // and after every failure so an outage pauses the search instead of failing variants.
    class TuneWorker : public ScanRun, public std::enable_shared_from_this<TuneWorker> {
    public:
        TuneWorker(const Configs::IpScan &scan, std::shared_ptr<Configs::Profile> base, QString dialHost, QString specHash,
                   bool resume, ScanRunHooks hooks);

        TuneWorker(const TuneWorker &) = delete;
        TuneWorker &operator=(const TuneWorker &) = delete;

        void Start() override;

        void RequestStop() override;

        void DisablePersist() override;

        [[nodiscard]] bool IsStopping() const override { return stop_.load(); }

        [[nodiscard]] bool IsFinalized() const override { return finalized_.load(); }

        [[nodiscard]] ScanLiveState LiveState() const override;

        [[nodiscard]] QString SessionId() const override { return sessionId_; }

    private:
        enum class Outcome { Passed, Failed, Stopped, NetworkLost };

        struct Variant {
            QString error;
            std::shared_ptr<Configs::BuildTestConfigResult> build;
            QStringList vpnEndpointTags;
        };

        void run();
        QString execute(bool &completed);
        Variant variant(int index);
        Outcome attempt(const Variant &variant, const QString &label, int *latencyMs, QString *error);
        // False when the search must pause; pauseReason_ says why.
        bool networkUp();
        bool pace();
        bool sleepUntil(qint64 deadlineMs);
        void persist(const Tune::Planner &planner);
        void saveRow();
        void finish(bool completed, const QString &error);
        [[nodiscard]] QJsonObject state() const;
        [[nodiscard]] qint64 elapsedMs() const;
        [[nodiscard]] static QString stageText(const Tune::Trial &trial);
        void setStage(const QString &stage);
        void pushLog(const QString &line);
        void notifyProgress() const;

        const int scanId_;
        const QString sessionId_;
        const std::shared_ptr<Configs::Profile> base_;
        const QString dialHost_;
        const QString specHash_;
        const bool resume_;
        const ScanRunHooks hooks_;

        // Run thread only.
        Configs::IpScan scan_;
        QList<Tune::Trial> trials_;
        QHash<int, Variant> variants_;
        QElapsedTimer clock_;
        qint64 elapsedBeforeMs_ = 0;
        qint64 lastAttemptEndMs_ = -1;
        // Start times within the last minute, for the per-minute cap.
        std::deque<qint64> attemptStarts_;
        QString testUrl_;
        qint64 bodyBytes_ = 0;
        QString stageText_;
        int best_ = -1;
        int mtuWinner_ = 0;
        QString note_;
        QString pauseReason_;

        std::atomic<bool> stop_{false};
        std::atomic<bool> persist_{true};
        std::atomic<bool> finalized_{false};

        mutable QMutex mutex_;
        ScanLiveState live_;
    };
} // namespace Scanner
