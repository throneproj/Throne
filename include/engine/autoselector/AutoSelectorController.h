#pragma once

#include <functional>
#include <memory>

#include <QList>
#include <QObject>

namespace Configs {
    class Profile;
    struct AutoSelectorBuildInfo;
}

namespace Engine {
    class EngineControl;
    class TestRunner;

    // Ranks a selector's pool before its build, rebuilds an exhausted pool, prunes pools a group refresh invalidated.
    class AutoSelectorController : public QObject {
        Q_OBJECT

    public:
        // control and tests outlive the controller.
        AutoSelectorController(EngineControl *control, TestRunner *tests, QObject *parent = nullptr);

        // Main thread, once the core's RPC is up; later calls are no-ops. The poll thread never exits.
        void startMonitorLoop();

        // Main thread, from the start path. True when it went off-thread to rank; resume then runs on main, once enough members work or
        // the sweep ends, and must re-enter start, which skips ranking.
        bool rankBeforeStart(const std::shared_ptr<Configs::Profile> &ent, bool interactive, const std::function<void()> &resume);

        // Any thread (start worker), once the core accepted the config; an empty list puts the monitor back to sleep.
        void noteBuilt(const std::shared_ptr<Configs::Profile> &ent, const QList<Configs::AutoSelectorBuildInfo> &builds);

        // Any thread; persists health on the calling thread.
        void noteStopped();

    signals:
        void statusChanged();

    private:
        // Blocks until the sweep ends; call off the main thread. startEarly, when set, is posted to the main thread once
        // as soon as enough members work; true means it was, and the caller then owes finishEarlySweep instead of its start.
        bool rank(const std::shared_ptr<Configs::Profile> &ent, bool interactive, const QList<int> &stale = {},
                  const std::function<void()> &startEarly = {});

        // Main thread, after a sweep that started its selector early: reranks once that start has settled, and rebuilds
        // the selector if it still runs and the full ranking changes its build.
        void finishEarlySweep(const std::shared_ptr<Configs::Profile> &ent);

        // Main thread; runs start with ranking skipped for profileID.
        void startRanked(int profileID, const std::function<void()> &start);

        void onPoolExhausted(int profileID);

        void onSubscriptionGroupChanged(int gid, const QList<int> &disturbed);

        EngineControl *control_;
        TestRunner *tests_;
        // Main thread only: sweeps of ours still running. A start meanwhile builds from the pool as it stands.
        int rankingInFlight_ = 0;
        // Set only while a ranked start's resume runs, so that re-entry skips ranking.
        int rankedFor_ = -1;
        bool monitorLoopStarted_ = false;
    };
} // namespace Engine
