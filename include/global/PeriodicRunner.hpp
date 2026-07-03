#pragma once

#include <QString>
#include <functional>
#include <vector>

class QTimer;

namespace Throne {

// One periodically-run maintenance job (e.g. refreshing subscriptions or remote routing
// profiles). Instead of a fire-every-N-minutes timer, the runner remembers when each job
// last ran and re-runs it once the configured interval has elapsed. Because the last-run
// time is persisted (through setLastRun), the schedule survives restarts and catches up on
// windows that were missed while the app was closed or the machine was asleep.
struct PeriodicTask {
    // Stable identifier, only used for logging.
    QString id;
    // Configured period in minutes; return <= 0 to disable the job. Read live on every
    // tick, so changing the setting takes effect without re-registering the task.
    std::function<int()> intervalMinutes;
    // Epoch-seconds of the last run (0 = never). Getter/setter keep the runner agnostic
    // about where the timestamp is stored and how it is persisted.
    std::function<qint64()> lastRun;
    std::function<void(qint64)> setLastRun;
    // The work to perform. Expected to return quickly (the existing updaters spawn their
    // own worker threads) since it runs on the tick (UI) thread.
    std::function<void()> run;
};

// Polls its registered tasks on a fixed cadence and runs the ones that are due. A single
// app-wide instance lives on the UI thread; register jobs once during startup.
class PeriodicRunner {
public:
    static PeriodicRunner* instance();

    // Register a job. It becomes eligible on the next tick (and on the short post-startup
    // catch-up check), so an overdue job runs soon after launch rather than a full
    // interval later.
    void Add(PeriodicTask task);

    // Re-evaluate every task right now. Call after the user changes an interval so a newly
    // enabled or shortened job doesn't wait up to one poll to react.
    void CheckNow();

private:
    PeriodicRunner();
    void tick();

    QTimer* m_timer = nullptr;
    std::vector<PeriodicTask> m_tasks;
};

} // namespace Throne
