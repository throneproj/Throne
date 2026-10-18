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

        // Main thread, from the start path. True when it went off-thread to rank; resume then runs on main and must re-enter start, which skips ranking.
        bool rankBeforeStart(const std::shared_ptr<Configs::Profile> &ent, bool interactive, const std::function<void()> &resume);

        // Any thread (start worker), once the core accepted the config; an empty list puts the monitor back to sleep.
        void noteBuilt(const std::shared_ptr<Configs::Profile> &ent, const QList<Configs::AutoSelectorBuildInfo> &builds);

        // Any thread; persists health on the calling thread.
        void noteStopped();

    signals:
        void statusChanged();

    private:
        // Blocks; call off the main thread.
        void rank(const std::shared_ptr<Configs::Profile> &ent, bool interactive, const QList<int> &stale = {});

        void onPoolExhausted(int profileID);

        void onSubscriptionGroupChanged(int gid, const QList<int> &disturbed);

        EngineControl *control_;
        TestRunner *tests_;
        // Main thread only.
        bool rankingInFlight_ = false;
        // Set only while a ranked start's resume runs, so that re-entry skips ranking.
        int rankedFor_ = -1;
        bool monitorLoopStarted_ = false;
    };
} // namespace Engine
