#include "include/engine/autoselector/AutoSelectorController.h"

#include "include/configs/AutoSelectorPlan.h"
#include "include/database/DatabaseManager.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineControl.h"
#include "include/engine/EngineEvents.h"
#include "include/engine/EngineTr.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/tests/TestRunner.h"
#include "include/global/Utils.hpp"
#include "include/stats/autoselector/AutoSelectorMonitor.hpp"

#include <QDateTime>
#include <QHash>
#include <QSemaphore>
#include <QSet>

#include <utility>

namespace Engine {
    AutoSelectorController::AutoSelectorController(EngineControl *control, TestRunner *tests, QObject *parent)
        : QObject(parent), control_(control), tests_(tests) {
        // The selector monitor emits from its own poll thread.
        connect(Stats::autoSelectorMonitor, &Stats::AutoSelectorMonitor::poolExhausted, this,
                [this](int profileID) { onPoolExhausted(profileID); }, Qt::QueuedConnection);
        connect(Stats::autoSelectorMonitor, &Stats::AutoSelectorMonitor::updated, this,
                &AutoSelectorController::statusChanged, Qt::QueuedConnection);
        connect(EngineEvents::instance(), &EngineEvents::subscriptionGroupChanged, this,
                &AutoSelectorController::onSubscriptionGroupChanged);
    }

    void AutoSelectorController::startMonitorLoop() {
        ENGINE_CHECK_MAIN_THREAD();
        if (monitorLoopStarted_) return;
        monitorLoopStarted_ = true;
        runOnNewThread([] { Stats::autoSelectorMonitor->Loop(); });
    }

    bool AutoSelectorController::rankBeforeStart(const std::shared_ptr<Configs::Profile> &ent, bool interactive,
                                                 const std::function<void()> &resume) {
        ENGINE_CHECK_MAIN_THREAD();
        // A pool can still need ranking right after a sweep (validity 0, refused sweep); re-ranking on resume would loop.
        const bool justRanked = std::exchange(rankedFor_, -1) == ent->id;
        if (ent->type == "autoselector" && !rankingInFlight_ && !justRanked) {
            const auto plan = Configs::PlanAutoSelector(ent);
            if (plan.error.isEmpty() && plan.needsRanking) {
                rankingInFlight_ = true;
                runOnNewThread([=, this] {
                    rank(ent, interactive);
                    runOnUiThread([=, this] {
                        rankingInFlight_ = false;
                        rankedFor_ = ent->id;
                        resume();
                        rankedFor_ = -1;
                    });
                });
                return true;
            }
        }
        rankingInFlight_ = false;
        return false;
    }

    void AutoSelectorController::noteBuilt(const std::shared_ptr<Configs::Profile> &ent, const QList<Configs::AutoSelectorBuildInfo> &builds) {
        Stats::autoSelectorMonitor->SetBuild(builds);
        if (!builds.isEmpty()) {
            const auto& info = builds.first();
            if (auto selector = ent->AutoSelector(); selector != nullptr) {
                QList<int> builtIDs;
                QHash<int, QString> names;
                for (const auto& [tag, member] : info.members) {
                    if (member == nullptr) continue;
                    builtIDs << member->id;
                    names.insert(member->id, member->outbound ? member->outbound->DisplayName() : member->name);
                }
                const auto now = QDateTime::currentSecsSinceEpoch();
                selector->lastBuilt = builtIDs;
                selector->lastBuiltAt = now;
                selector->RecordHistory(builtIDs, names, now);
                Configs::dataManager->profilesRepo->Save(ent);
                MW_show_log(MainWindowTr::tr("[Auto selector] Running the best %1 of %2 ranked profiles.")
                                .arg(builtIDs.size())
                                .arg(selector->pool.size()));
            }
        }
        // Unconditional: an ordinary profile must hide the selector status too.
        emit statusChanged();
    }

    void AutoSelectorController::noteStopped() {
        Stats::autoSelectorMonitor->Clear();
        emit statusChanged();
    }

    void AutoSelectorController::rank(const std::shared_ptr<Configs::Profile> &ent, bool interactive, const QList<int> &stale) {
        if (ent == nullptr || ent->type != "autoselector") return;

        const auto needed = Configs::AutoSelectorUnmeasuredCandidates(ent, stale);
        if (needed.isEmpty()) {
            const auto ranked = Configs::RerankAutoSelectorPool(ent);
            MW_show_log(MainWindowTr::tr("[Auto selector] Reusing existing test results; ranked %1 profiles.").arg(ranked.size()));
            return;
        }

        MW_show_log(MainWindowTr::tr("[Auto selector] Measuring %1 not-yet-tested profiles...").arg(needed.size()));
        // Wait on the sweep's completion signal, not the session lock: this thread already holds it.
        QSemaphore sweepDone;
        tests_->runUrlTests(needed, [&sweepDone] { sweepDone.release(); }, interactive);
        sweepDone.acquire();

        const auto ranked = Configs::RerankAutoSelectorPool(ent);
        MW_show_log(MainWindowTr::tr("[Auto selector] Ranked %1 profiles.").arg(ranked.size()));
    }

    void AutoSelectorController::onSubscriptionGroupChanged(int gid, const QList<int> &disturbed) {
        ENGINE_CHECK_MAIN_THREAD();
        if (gid < 0) return;
        const QSet<int> disturbedSet(disturbed.begin(), disturbed.end());
        const auto running = control_->runningSnapshot();
        int restartID = -1;

        for (int id : Configs::dataManager->profilesRepo->GetProfileIdsByType("autoselector")) {
            auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
            if (ent == nullptr) continue;
            auto selector = ent->AutoSelector();
            if (selector == nullptr || !selector->Tracks(gid)) continue;

            const auto gone = [](int memberID) {
                return Configs::dataManager->profilesRepo->GetProfile(memberID) == nullptr;
            };
            const auto prunedPool = selector->pool.removeIf(gone);
            const auto prunedBuilt = selector->lastBuilt.removeIf(gone);
            if (prunedPool > 0 || prunedBuilt > 0) Configs::dataManager->profilesRepo->Save(ent);

            if (running == nullptr || running->id != ent->id) continue;
            // A replaced member keeps its id, so only the disturbed set spots it.
            bool rebuild = prunedBuilt > 0;
            for (int memberID : selector->lastBuilt) {
                if (!disturbedSet.contains(memberID)) continue;
                rebuild = true;
                break;
            }
            if (rebuild) restartID = ent->id;
        }

        if (restartID < 0) return;
        MW_show_log(MainWindowTr::tr("[Auto selector] The subscription replaced profiles it was running on — rebuilding."));
        control_->start(StartRequest{restartID});
    }

    void AutoSelectorController::onPoolExhausted(int profileID) {
        auto ent = Configs::dataManager->profilesRepo->GetProfile(profileID);
        const auto running = control_->runningSnapshot();
        if (ent == nullptr || running == nullptr || running->id != profileID) return;

        MW_show_log(MainWindowTr::tr("[Auto selector] Every running profile stopped working — rebuilding from the "
                                     "next best candidates."));
        runOnNewThread([=, this] {
            QList<int> stale;
            if (auto selector = ent->AutoSelector(); selector != nullptr) stale = selector->lastBuilt;
            rank(ent, true, stale);
            runOnUiThread([=, this] {
                if (const auto current = control_->runningSnapshot(); current == nullptr || current->id != profileID) return;
                control_->start(StartRequest{profileID});
            });
        });
    }
} // namespace Engine
