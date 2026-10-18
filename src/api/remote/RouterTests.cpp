#include "include/api/remote/Router.hpp"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>

#include <utility>

#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineControl.h"
#include "include/global/Configs.hpp"

namespace RemoteApi::RouterJobs {
    constexpr int kDefaultWaitSec = 300;
    constexpr int kCancelWaitMs = 10000;
    constexpr qsizetype kKeptJobs = 32;
}

namespace RemoteApi {
    void Router::postTest(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<QString> kind;
        if (!bodyObject(request, true, &body, &error) || !readString(body, QStringLiteral("kind"), &kind, &error)) {
            respond(error);
            return;
        }
        if (kind != QStringLiteral("latency") && kind != QStringLiteral("ip") && kind != QStringLiteral("speed")) {
            respond(badRequest(QStringLiteral("\"kind\" must be latency, ip or speed")));
            return;
        }
        QList<int> ids;
        WaitOptions options;
        if (!readSelection(body, &ids, &error) || !readWait(body, false, RouterJobs::kDefaultWaitSec, &options, &error)) {
            respond(error);
            return;
        }
        const auto selectors = Configs::dataManager->profilesRepo->GetProfileIdsByType(QStringLiteral("autoselector"));
        const QSet<int> skipped(selectors.begin(), selectors.end());
        ids.removeIf([&skipped](int id) { return skipped.contains(id); });
        if (ids.isEmpty()) {
            respond(noProfiles());
            return;
        }

        const auto busy = Response::Error(409, QStringLiteral("busy"), QStringLiteral("A test is already running"));
        if (engine_->testsRunning()) {
            respond(busy);
            return;
        }
        auto job = std::make_shared<Job>();
        job->id = ++lastJobId_;
        job->kind = *kind;
        job->state = QStringLiteral("running");
        job->profileIds = ids;
        job->startedAt = QDateTime::currentSecsSinceEpoch();
        // Runs on the runner's worker thread, or right away when the runner refuses.
        const auto onFinished = [this, engine = engine_, jobId = job->id] {
            const bool stopped = engine->testsStopRequested();
            QMetaObject::invokeMethod(this, [this, jobId, stopped] { finishJob(jobId, stopped); }, Qt::QueuedConnection);
        };
        Engine::TestKind testKind = Engine::TestKind::Speed;
        if (kind == QStringLiteral("latency")) testKind = Engine::TestKind::Latency;
        else if (kind == QStringLiteral("ip")) testKind = Engine::TestKind::Ip;
        if (!engine_->runTests(testKind, ids, onFinished)) {
            respond(busy);
            return;
        }

        jobs_.prepend(job);
        while (jobs_.size() > RouterJobs::kKeptJobs) jobs_.removeLast();
        if (!options.wait) {
            respond(Response::Ok(jobJson(*job, false), 202));
            return;
        }
        job->waiters.append(makeWaiter(respond, options.timeoutMs, [this, job] { return jobResponse(*job); }));
    }

    void Router::getTests(const Request &, const Responder &respond, int) {
        QJsonArray jobs;
        for (const auto &job : jobs_) jobs.append(jobJson(*job, false));
        respond(Response::Ok(QJsonObject{{"jobs", jobs}}));
    }

    void Router::getTest(const Request &, const Responder &respond, int id) {
        const auto job = findJob(id);
        if (job == nullptr) {
            respond(notFound(QStringLiteral("Unknown test %1").arg(id)));
            return;
        }
        respond(Response::Ok(jobJson(*job, true)));
    }

    void Router::deleteTest(const Request &, const Responder &respond, int id) {
        const auto job = findJob(id);
        if (job == nullptr) {
            respond(notFound(QStringLiteral("Unknown test %1").arg(id)));
            return;
        }
        if (job->state != QStringLiteral("running")) {
            respond(jobResponse(*job));
            return;
        }
        job->cancelRequested = true;
        job->waiters.append(makeWaiter(respond, RouterJobs::kCancelWaitMs, [this, job] { return jobResponse(*job); }));
        // stop() asks the core over a blocking RPC.
        runOnNewThread([engine = engine_] { engine->stopTests(); });
    }

    void Router::deleteResults(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        QList<int> ids;
        if (!bodyObject(request, true, &body, &error) || !readSelection(body, &ids, &error)) {
            respond(error);
            return;
        }
        if (ids.isEmpty()) {
            respond(noProfiles());
            return;
        }
        respond(Response::Ok(QJsonObject{{"cleared", engine_->clearTestResults(ids)}}));
    }

    void Router::finishJob(int jobId, bool stopped) {
        const auto job = findJob(jobId);
        if (job == nullptr || job->state != QStringLiteral("running")) return;
        job->state = stopped || job->cancelRequested ? QStringLiteral("cancelled") : QStringLiteral("done");
        job->finishedAt = QDateTime::currentSecsSinceEpoch();
        const auto response = jobResponse(*job);
        for (const auto &waiter : std::exchange(job->waiters, {})) answer(waiter, response);
    }

    std::shared_ptr<Router::Job> Router::findJob(int jobId) const {
        for (const auto &job : jobs_) {
            if (job->id == jobId) return job;
        }
        return nullptr;
    }

    QJsonObject Router::jobJson(const Job &job, bool withResults) const {
        QJsonArray ids;
        for (const int id : job.profileIds) ids.append(id);
        QJsonObject json{
            {"id", job.id},
            {"kind", job.kind},
            {"state", job.state},
            {"profile_ids", ids},
            {"started_at", job.startedAt},
            {"finished_at", job.finishedAt},
        };
        if (withResults && job.state != QStringLiteral("running")) {
            QJsonArray results;
            for (const auto &profile : Configs::dataManager->profilesRepo->GetProfileBatch(job.profileIds)) results.append(profileJson(profile));
            json.insert(QStringLiteral("results"), results);
        }
        return json;
    }

    Response Router::jobResponse(const Job &job) const {
        return Response::Ok(jobJson(job, true), job.state == QStringLiteral("running") ? 202 : 200);
    }
}
