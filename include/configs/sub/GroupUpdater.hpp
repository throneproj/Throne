#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <functional>

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

    // Jobs run one at a time on a background worker, in FIFO order.
    class GroupUpdater : public QObject {
        Q_OBJECT

    public:
        using Finish = std::function<void()>;
        // Must return at once; done may fire on any thread.
        using UrlTester = std::function<void(const QList<int> &profileIDs, const Finish &done)>;

        // showDiff: a manual refresh pops up the diff; automatic paths leave it false and only log.
        void RefreshGroup(int gid, const Finish &finish = nullptr, bool showDiff = false);

        void RefreshAll(bool onlyAllowed = false);

        // Runs on every runner poll (UI thread); each group follows its own ResolveAutoUpdate interval.
        void CheckAutoUpdate();

        // Epoch seconds of the next automatic refresh (<= now: due), -1 when none is scheduled. UI thread only.
        [[nodiscard]] qint64 NextAutoUpdate() const;

        void SubscribeUrl(const QString &url, const Finish &finish = nullptr);

        void ImportUrl(const QString &url, const Finish &finish = nullptr);

        void ImportText(const QString &text, int gid = -1, const Finish &finish = nullptr);

        void ImportBatch(const QStringList &payloads, const Finish &finish = nullptr);

        void SetUrlTester(UrlTester tester);

    signals:
        void asyncUpdateCallback(int gid);

    private:
        struct Job {
            int gid = -1;
            bool batch = false;
            std::function<void()> run;
        };

        void enqueue(Job job);
        void enqueueLocked(Job job);
        void drain();
        void refresh(int gid, bool showDiff);
        void requestUrlTest(int gid, const QList<int> &profileIDs);
        void afterUrlTest(int gid);
        void importDocuments(int gid, QList<QByteArray> documents);
        bool fetch(const QString &url, const QString &name, const RequestIdentity &identity, QByteArray &body, Configs::SubUserInfo &subInfo);
        [[nodiscard]] qint64 autoUpdateDue(const Configs::Group &group, qint64 interval) const;

        // UI thread only: in-memory, so a restart retries a failing group once.
        QHash<int, qint64> autoAttempts;

        QMutex mutex;
        QList<Job> queue;
        QSet<int> pending;
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
