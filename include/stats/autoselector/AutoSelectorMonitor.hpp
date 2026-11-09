#pragma once

#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <memory>

#include "include/configs/generate.h"

namespace Stats
{
    struct AutoSelectorMemberView
    {
        QString tag;
        int profileID = -1;
        QString name;
        int rank = 0;
        QString state; // ok | degraded | untested | dead | cooldown
        bool selected = false;
        bool selectedUDP = false;
        bool pinned = false;
        bool qualified = false;
        bool active = false;
        int averageMs = 0;
        int deviationMs = 0;
        int minMs = 0;
        int maxMs = 0;
        int samples = 0;
        int failures = 0;
        int probes = 0;
        int dialTotal = 0;
        int dialFail = 0;
        qint64 lastOKms = 0;
        qint64 lastProbeMs = 0;
        qint64 cooldownUntilMs = 0;
        QString lastError;

        [[nodiscard]] bool isDead() const { return state == "dead"; }
        [[nodiscard]] bool isUsable() const { return state == "ok" || state == "degraded"; }

        [[nodiscard]] bool hasProblem() const
        {
            return state == "dead" || state == "cooldown" || state == "degraded";
        }
    };

    struct AutoSelectorView
    {
        bool valid = false;
        QString groupTag;
        int profileID = -1;
        QString phase; // starting | probing | ready | suspended
        QString selectedTag;
        QString selectedName;
        int selectedProfileID = -1;
        // May differ from selectedTag: the pin is a preference, ranking still overrides an unhealthy member.
        QString pinnedTag;
        QString pinnedName;
        bool balance = false;
        QString balanceMode;
        // Core froze its ranking (local network down); nothing may be judged broken while this is set.
        bool suspended = false;
        qint64 suspendedSinceMs = 0;
        // Members dial through chain hops in front of the selector.
        bool upstream = false;
        int membersTotal = 0;
        int membersProbed = 0;
        int membersAlive = 0;
        int membersQualified = 0;
        int membersCooldown = 0;
        int probesInFlight = 0;
        int roundsCompleted = 0;
        qint64 lastRoundMs = 0;
        qint64 nextRoundMs = 0;
        qint64 lastSwitchMs = 0;
        QString lastSwitchReason;
        qint64 updatedAtMs = 0;
        qint64 exhaustedSinceMs = 0;
        QList<AutoSelectorMemberView> members;

        [[nodiscard]] QString summary() const;

        [[nodiscard]] QString detail() const;
    };

    class AutoSelectorMonitor : public QObject
    {
        Q_OBJECT

    public:
        // An empty `infos` (an ordinary profile) puts the monitor back to sleep.
        void SetBuild(const QList<Configs::AutoSelectorBuildInfo> &infos);

        void Clear();

        // Runs on its own thread; polls only while a selector is running.
        void Loop();

        [[nodiscard]] AutoSelectorView Snapshot() const;

        [[nodiscard]] bool Active() const;

        void RequestRecheck() const;

        // An empty `tag` restores automatic selection; returns the core's error, empty on success.
        [[nodiscard]] QString RequestSelect(const QString &tag);

        void PersistHealth();

    signals:
        void updated();
        // Emitted only once every member has been unusable for the grace period while the local network is fine.
        void poolExhausted(int profileID);

    private:
        void poll();

        mutable QMutex mutex;
        AutoSelectorView view;
        QHash<QString, int> tagToProfile;
        QHash<QString, QString> tagToName;
        QString groupTag;
        int profileID = -1;
        bool upstream = false;
        bool active = false;

        qint64 exhaustedSince = 0;
        qint64 lastRebuildRequest = 0;
        int rebuildBackoffSecs = 0;
        qint64 lastHealthPersist = 0;
    };

    extern AutoSelectorMonitor *autoSelectorMonitor;

    constexpr int kPoolExhaustedGraceSecs = 20;
    // Doubled on each consecutive rebuild, up to the max.
    constexpr int kRebuildBackoffMinSecs = 60;
    constexpr int kRebuildBackoffMaxSecs = 600;
    constexpr int kHealthPersistSecs = 60;
} // namespace Stats
