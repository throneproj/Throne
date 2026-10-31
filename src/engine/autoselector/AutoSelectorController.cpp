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
#include <QMutex>
#include <QSemaphore>
#include <QSet>

#include <utility>

namespace {
    // Shared with the result hook, which a late result poll can still call after rank() has returned.
    struct AutoSelectorSweep {
        QMutex mu;
        // Released once when the quorum is met and once when the sweep ends.
        QSemaphore wake;
        QSet<int> measured;
        QSet<int> working;
        int workingBefore = 0;
        // 0 = never start early.
        int quorum = 0;
        bool quorumMet = false;
        bool done = false;

        // Under mu.
        [[nodiscard]] bool reachedQuorum() const { return quorum > 0 && workingBefore + working.size() >= quorum; }
    };
} // namespace

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
        // A second sweep would only be refused while one runs, and an early-started selector restarts during it.
        if (ent->type != "autoselector" || rankingInFlight_ > 0 || justRanked) return false;
        const auto plan = Configs::PlanAutoSelector(ent);
        if (!plan.error.isEmpty() || !plan.needsRanking) return false;

        ++rankingInFlight_;
        const auto resumeRanked = [=, this] { startRanked(ent->id, resume); };
        runOnNewThread([=, this] {
            const bool startedEarly = rank(ent, interactive, {}, resumeRanked);
            runOnUiThread([=, this] {
                --rankingInFlight_;
                if (startedEarly) finishEarlySweep(ent);
                else resumeRanked();
            });
        });
        return true;
    }

    void AutoSelectorController::startRanked(int profileID, const std::function<void()> &start) {
        ENGINE_CHECK_MAIN_THREAD();
        rankedFor_ = profileID;
        start();
        rankedFor_ = -1;
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

    bool AutoSelectorController::rank(const std::shared_ptr<Configs::Profile> &ent, bool interactive, const QList<int> &stale,
                                      const std::function<void()> &startEarly) {
        if (ent == nullptr || ent->type != "autoselector") return false;

        auto sweep = std::make_shared<AutoSelectorSweep>();
        // Stale members are not counted as working until this sweep re-measures them.
        const auto needed = Configs::AutoSelectorUnmeasuredCandidates(ent, stale, &sweep->workingBefore);
        if (needed.isEmpty()) {
            const auto ranked = Configs::RerankAutoSelectorPool(ent);
            MW_show_log(MainWindowTr::tr("[Auto selector] Reusing existing test results; ranked %1 profiles.").arg(ranked.size()));
            return false;
        }
        if (startEarly) sweep->quorum = Configs::AutoSelectorStartQuorum(ent);

        MW_show_log(MainWindowTr::tr("[Auto selector] Measuring %1 not-yet-tested profiles...").arg(needed.size()));
        const auto onResult = [sweep, ent](const std::shared_ptr<Configs::Profile> &member) {
            const bool works = Configs::AutoSelectorMemberWorks(ent, member);
            QMutexLocker lock(&sweep->mu);
            sweep->measured.insert(member->id);
            if (works) sweep->working.insert(member->id);
            else sweep->working.remove(member->id);
            if (sweep->quorumMet || !sweep->reachedQuorum()) return;
            sweep->quorumMet = true;
            sweep->wake.release();
        };
        const auto onFinished = [sweep] {
            QMutexLocker lock(&sweep->mu);
            sweep->done = true;
            sweep->wake.release();
        };
        // Wait on the sweep's own signals, not the session lock: this thread already holds it.
        if (tests_->runUrlTests(needed, onFinished, interactive, onResult)) {
            QMutexLocker lock(&sweep->mu);
            if (!sweep->quorumMet && sweep->reachedQuorum()) {
                sweep->quorumMet = true;
                sweep->wake.release();
            }
        }

        bool startedEarly = false;
        for (;;) {
            sweep->wake.acquire();
            qsizetype found = 0;
            qsizetype remaining = 0;
            QList<int> untrusted;
            {
                QMutexLocker lock(&sweep->mu);
                if (sweep->done) break;
                if (startedEarly || !sweep->quorumMet) continue;
                found = sweep->workingBefore + sweep->working.size();
                remaining = std::max<qsizetype>(0, needed.size() - sweep->measured.size());
                for (int id : stale) {
                    if (!sweep->measured.contains(id)) untrusted << id;
                }
                startedEarly = true;
            }
            // The early build takes the working members first; the core probes the unmeasured rest itself.
            Configs::RerankAutoSelectorPool(ent, untrusted);
            MW_show_log(MainWindowTr::tr("[Auto selector] %1 working profiles found; starting now while the remaining %2 are "
                                         "still being measured.")
                            .arg(found)
                            .arg(remaining));
            runOnUiThread(startEarly);
        }
        if (startedEarly) return true;

        const auto ranked = Configs::RerankAutoSelectorPool(ent);
        MW_show_log(MainWindowTr::tr("[Auto selector] Ranked %1 profiles.").arg(ranked.size()));
        return false;
    }

    void AutoSelectorController::finishEarlySweep(const std::shared_ptr<Configs::Profile> &ent) {
        ENGINE_CHECK_MAIN_THREAD();
        // The early start may still be building from the pool a rerank rewrites, and lastBuilt is current only once it settles.
        if (control_->connectionState() == ConnectionState::Connecting) {
            auto settled = std::make_shared<QMetaObject::Connection>();
            *settled = connect(control_, &EngineControl::connectionStateChanged, this, [=, this](ConnectionState state) {
                if (state == ConnectionState::Connecting) return;
                disconnect(*settled);
                finishEarlySweep(ent);
            });
            return;
        }
        runOnNewThread([=, this] {
            const auto ranked = Configs::RerankAutoSelectorPool(ent);
            MW_show_log(MainWindowTr::tr("[Auto selector] Ranked %1 profiles.").arg(ranked.size()));
            const bool changed = Configs::AutoSelectorBuildChanged(ent);
            runOnUiThread([=, this] {
                if (!changed || control_->connectionState() != ConnectionState::Running) return;
                if (const auto running = control_->runningSnapshot(); running == nullptr || running->id != ent->id) return;
                MW_show_log(MainWindowTr::tr("[Auto selector] Every profile is measured and the best ones changed — rebuilding "
                                             "with the full ranking."));
                startRanked(ent->id, [=, this] { control_->start(StartRequest{ent->id}); });
            });
        });
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
        ++rankingInFlight_;
        const auto restart = [=, this] {
            if (const auto current = control_->runningSnapshot(); current == nullptr || current->id != profileID) return;
            startRanked(profileID, [=, this] { control_->start(StartRequest{profileID}); });
        };
        runOnNewThread([=, this] {
            QList<int> stale;
            if (auto selector = ent->AutoSelector(); selector != nullptr) stale = selector->lastBuilt;
            const bool startedEarly = rank(ent, true, stale, restart);
            runOnUiThread([=, this] {
                --rankingInFlight_;
                if (startedEarly) finishEarlySweep(ent);
                else restart();
            });
        });
    }
} // namespace Engine
