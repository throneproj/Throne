#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <functional>
#include <memory>

#include "include/configs/sub/ImportReport.hpp"
#include "include/database/entities/Group.h"
#include "include/global/DeviceDetailsHelper.hpp"

namespace Subscription {
    struct RequestIdentity {
        QString userAgent;
        bool sendHwid = false;
        DeviceDetails device;
        Configs::subTlsVersion tlsVersion = Configs::subTlsVersion::automatic;
        Configs::subHttpVersion httpVersion = Configs::subHttpVersion::automatic;
    };

    // The global subscription settings under the group's overrides; nullptr resolves the globals alone.
    RequestIdentity ResolveIdentity(const Configs::Group *group);

    // A refresh's or a quiet import's result. Nothing was applied unless ok().
    struct UpdateOutcome {
        enum class Failure {
            None,
            // The group is gone or archived.
            NoGroup,
            // The download failed; error is free of URLs.
            Fetch,
            // Nothing usable was found; a refresh leaves the group untouched.
            Empty,
            Database,
            Internal,
        };

        int gid = -1;
        Failure failure = Failure::None;
        QString error;
        // An import's stored profiles, in document order.
        QList<int> addedIds;
        // A refresh's reconcile counts.
        int added = 0;
        int updated = 0;
        int removed = 0;
        int kept = 0;
        ImportReport report;

        [[nodiscard]] bool ok() const { return failure == Failure::None; }
    };

    // Jobs run one at a time on a background worker, in FIFO order.
    class GroupUpdater : public QObject {
        Q_OBJECT

    public:
        using Finish = std::function<void()>;
        // Runs on the worker thread, or at once on the caller's thread for a request refused before queuing.
        using OutcomeFinish = std::function<void(const UpdateOutcome &)>;
        // Must return at once; done may fire on any thread.
        // interactive: the update was started by the user in the window (Surface Diff or Report).
        using UrlTester = std::function<void(const QList<int> &profileIDs, bool interactive, const Finish &done)>;

        // Where an update's problems go: the log only (automatic), the diff popup (manual refresh), or a report box (a new subscription's first update).
        enum class Surface { Log, Diff, Report };

        // showDiff: a manual refresh pops up the diff; automatic paths leave it false and only log.
        void RefreshGroup(int gid, const Finish &finish = nullptr, bool showDiff = false);
        void RefreshGroup(int gid, const Finish &finish, Surface surface);

        void RefreshAll(bool onlyAllowed = false);

        // Runs on every runner poll (UI thread); each group follows its own ResolveAutoUpdate interval.
        void CheckAutoUpdate();

        // Epoch seconds of the next automatic refresh (<= now: due), -1 when none is scheduled. UI thread only.
        [[nodiscard]] qint64 NextAutoUpdate() const;

        void SubscribeUrl(const QString &url, const Finish &finish = nullptr);

        void ImportUrl(const QString &url, const Finish &finish = nullptr);

        void ImportText(const QString &text, int gid = -1, const Finish &finish = nullptr);

        void ImportBatch(const QStringList &payloads, const Finish &finish = nullptr);

        // Copies land in the current group and keep their source's endpoint, which no link carries.
        void CloneProfiles(const QList<int> &ids, const Finish &finish = nullptr);

        void SetUrlTester(UrlTester tester);

        // A refresh of gid that reports its outcome; while gid is already queued or running, done shares that job's outcome.
        void RefreshWithOutcome(int gid, Surface surface, const OutcomeFinish &done);

        // Quiet imports into an existing gid: problems go to the log and the outcome, never a report box.
        void ImportTextTo(int gid, const QString &text, const OutcomeFinish &done);

        void ImportUrlTo(int gid, const QString &url, const OutcomeFinish &done);

        // A refresh of gid is queued or running.
        [[nodiscard]] bool IsBusy(int gid);

        // No job is queued or running.
        [[nodiscard]] bool IsIdle();

    signals:
        void asyncUpdateCallback(int gid);

    private:
        struct Job {
            int gid = -1;
            bool batch = false;
            std::function<void()> run;
            // Runs after run() even when it throws.
            Finish finish;
            // A refresh's result, handed to the gid's waiters once the job ends; Internal until run() completes.
            std::shared_ptr<UpdateOutcome> outcome;
        };

        Job refreshJob(int gid, bool batch, Surface surface, const Finish &finish);
        void enqueue(Job job);
        void enqueueLocked(Job job);
        void drain();
        UpdateOutcome refresh(int gid, Surface surface);
        void applyRefresh(const std::shared_ptr<Configs::Group> &group, const QString &url, QByteArray body,
                          const Configs::SubUserInfo &subInfo, Surface surface, UpdateOutcome &result);
        void afterUrlTest(int gid);
        // emptyReason replaces the generic "nothing found" when no profile is imported; quiet never shows a report box.
        UpdateOutcome importDocuments(int gid, QList<QByteArray> documents, const QString &emptyReason = {}, bool quiet = false);
        bool fetch(const QString &url, const QString &name, const RequestIdentity &identity, QByteArray &body,
                   Configs::SubUserInfo &subInfo, QString *error = nullptr);
        [[nodiscard]] qint64 autoUpdateDue(const Configs::Group &group, qint64 interval) const;

        // UI thread only: in-memory, so a restart retries a failing group once.
        QHash<int, qint64> autoAttempts;

        QMutex mutex;
        QList<Job> queue;
        QSet<int> pending;
        // RefreshWithOutcome callers per gid, called once when that gid's refresh job ends.
        QHash<int, QList<OutcomeFinish>> waiters;
        int pendingBatch = 0;
        bool running = false;
        UrlTester urlTester;
    };

    struct AutoUpdatePlan {
        enum class Source { off, global, group, server };
        qint64 interval = 0; // seconds; 0 = not auto-updated
        Source source = Source::off;
    };

    // The server's interval when respected and sent, else the group's override, else the global setting.
    AutoUpdatePlan ResolveAutoUpdate(const Configs::Group &group);

    GroupUpdater *updater();
} // namespace Subscription
