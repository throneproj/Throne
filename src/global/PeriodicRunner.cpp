#include "include/global/PeriodicRunner.hpp"

#include <QDateTime>
#include <QObject>
#include <QTimer>

#include "include/global/Utils.hpp"

namespace Throne {

// How often to re-check whether any job is due. The minimum job interval is measured in
// tens of minutes, so a 60s poll is plenty granular while staying cheap.
static constexpr int kPollSeconds = 60;
// Small delay before the first check so startup isn't competing with an update, while
// still letting an overdue job fire shortly after launch (not a full interval later).
static constexpr int kInitialDelaySeconds = 10;

PeriodicRunner* PeriodicRunner::instance() {
    static auto* runner = new PeriodicRunner();
    return runner;
}

PeriodicRunner::PeriodicRunner() {
    // No parent: this is an app-lifetime singleton created on the UI thread, so the timer
    // lives on the thread with the event loop and is never torn down early.
    m_timer = new QTimer();
    QObject::connect(m_timer, &QTimer::timeout, m_timer, [this] { tick(); });
    m_timer->start(kPollSeconds * 1000);
    // Catch up on anything already overdue shortly after startup.
    QTimer::singleShot(kInitialDelaySeconds * 1000, m_timer, [this] { tick(); });
}

void PeriodicRunner::Add(PeriodicTask task) {
    m_tasks.push_back(std::move(task));
}

void PeriodicRunner::CheckNow() {
    tick();
}

void PeriodicRunner::tick() {
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const auto& task : m_tasks) {
        const int minutes = task.intervalMinutes ? task.intervalMinutes() : 0;
        if (minutes <= 0) continue; // disabled
        const qint64 last = task.lastRun ? task.lastRun() : 0;
        // last == 0 means "never run", which is always due; otherwise wait out the interval.
        if (last > 0 && now - last < static_cast<qint64>(minutes) * 60) continue;
        // Record the attempt before running so a slow job can't double-fire and so the
        // interval counts from when we started (matches the persisted-schedule intent).
        if (task.setLastRun) task.setLastRun(now);
        if (!task.id.isEmpty()) MW_show_log(QObject::tr("Auto-update: running %1").arg(task.id));
        if (task.run) task.run();
    }
}

} // namespace Throne
