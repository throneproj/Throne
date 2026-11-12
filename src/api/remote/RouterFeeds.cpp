#include "include/api/remote/Router.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QTimer>

#include <algorithm>
#include <limits>
#include <utility>

#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/engine/EngineControl.h"
#include "include/engine/services/SettingsRegistry.h"
#include "include/global/Configs.hpp"
#include "include/global/LogBuffer.hpp"

namespace RemoteApi::RouterFeedsImpl {
    constexpr qint64 kDefaultLogLimit = 200;
    constexpr qint64 kMaxLogLimit = 1000;
    constexpr qint64 kMaxHoldSec = 300;
    constexpr int kSettleMs = 200;
    constexpr int kStatusPollMs = 1000;
    constexpr int kLogPollMs = 200;

    struct TopicName {
        Engine::ChangeTopic topic;
        const char *name;
    };

    // Indexes Router::topicSeqs_.
    constexpr TopicName kTopics[] = {
        {Engine::ChangeTopic::Status, "status"},
        {Engine::ChangeTopic::Groups, "groups"},
        {Engine::ChangeTopic::Profiles, "profiles"},
        {Engine::ChangeTopic::Tests, "tests"},
        {Engine::ChangeTopic::Settings, "settings"},
    };

    // New with every process, so a client sees that Throne restarted.
    const QString &instanceId() {
        static const QString id = QString::number(QRandomGenerator::system()->generate64(), 16).rightJustified(16, QLatin1Char('0'));
        return id;
    }

    // False when the parameter is present but not an integer from min to max.
    bool queryInteger(const Request &request, const QString &key, qint64 fallback, qint64 min, qint64 max, qint64 *out) {
        if (!request.query.contains(key)) {
            *out = fallback;
            return true;
        }
        bool ok = false;
        *out = request.query.value(key).toLongLong(&ok);
        return ok && *out >= min && *out <= max;
    }

    // Empty when "after" and "timeout" are usable.
    QString readCursor(const Request &request, qint64 *after, qint64 *timeoutSec) {
        if (!queryInteger(request, QStringLiteral("after"), 0, 0, std::numeric_limits<qint64>::max(), after)) {
            return QStringLiteral("\"after\" must be an integer, 0 or more");
        }
        if (!queryInteger(request, QStringLiteral("timeout"), 0, 0, kMaxHoldSec, timeoutSec)) {
            return QStringLiteral("\"timeout\" must be an integer from 0 to %1 seconds").arg(kMaxHoldSec);
        }
        return {};
    }

    bool sameInstance(const Request &request) {
        return request.query.value(QStringLiteral("instance")) == instanceId();
    }
}

namespace RemoteApi {
    void Router::setupFeeds() {
        eventSettle_ = new QTimer(this);
        eventSettle_->setSingleShot(true);
        eventSettle_->setInterval(RouterFeedsImpl::kSettleMs);
        connect(eventSettle_, &QTimer::timeout, this, [this] { settleEvents(); });
        statusPoll_ = new QTimer(this);
        statusPoll_->setInterval(RouterFeedsImpl::kStatusPollMs);
        connect(statusPoll_, &QTimer::timeout, this, [this] { pollStatus(); });
        logPoll_ = new QTimer(this);
        logPoll_->setInterval(RouterFeedsImpl::kLogPollMs);
        connect(logPoll_, &QTimer::timeout, this, [this] { pollLogs(); });
        connect(engine_, &Engine::EngineControl::changed, this, &Router::noteChanged);
    }

    void Router::getLogs(const Request &request, const Responder &respond, int) {
        qint64 after = 0;
        qint64 timeout = 0;
        qint64 limit = 0;
        if (const auto problem = RouterFeedsImpl::readCursor(request, &after, &timeout); !problem.isEmpty()) {
            respond(badRequest(problem));
            return;
        }
        if (!RouterFeedsImpl::queryInteger(request, QStringLiteral("limit"), RouterFeedsImpl::kDefaultLogLimit, 1,
                                           RouterFeedsImpl::kMaxLogLimit, &limit)) {
            respond(badRequest(QStringLiteral("\"limit\" must be an integer from 1 to %1").arg(RouterFeedsImpl::kMaxLogLimit)));
            return;
        }
        const bool same = RouterFeedsImpl::sameInstance(request);
        const auto from = static_cast<quint64>(after);
        const int count = static_cast<int>(limit);
        if (!same || timeout == 0 || Logging::BufferedNewestSeq() > from) {
            respond(logsResponse(same, from, count));
            return;
        }
        const auto waiter = makeWaiter(respond, static_cast<int>(timeout) * 1000, [this, from, count] { return logsResponse(true, from, count); });
        logWaiters_.append(FeedWaiter{from, count, waiter});
        if (!logPoll_->isActive()) logPoll_->start();
    }

    void Router::getEvents(const Request &request, const Responder &respond, int) {
        qint64 after = 0;
        qint64 timeout = 0;
        if (const auto problem = RouterFeedsImpl::readCursor(request, &after, &timeout); !problem.isEmpty()) {
            respond(badRequest(problem));
            return;
        }
        const auto from = static_cast<quint64>(after);
        if (!RouterFeedsImpl::sameInstance(request)) {
            respond(eventsResponse(from, true));
            return;
        }
        if (timeout == 0 || changedAfter(from)) {
            respond(eventsResponse(from, false));
            return;
        }
        const auto waiter = makeWaiter(respond, static_cast<int>(timeout) * 1000, [this, from] { return eventsResponse(from, false); });
        eventWaiters_.append(FeedWaiter{from, 0, waiter});
        if (!statusPoll_->isActive()) {
            statusPoll_->start();
            pollStatus();
        }
    }

    void Router::noteChanged(Engine::ChangeTopics topics) {
        if (!topics) return;
        ++changeSeq_;
        for (size_t i = 0; i < topicSeqs_.size(); ++i) {
            if (topics.testFlag(RouterFeedsImpl::kTopics[i].topic)) topicSeqs_[i] = changeSeq_;
        }
        if (!eventWaiters_.isEmpty() && !eventSettle_->isActive()) eventSettle_->start();
    }

    bool Router::changedAfter(quint64 after) const {
        return std::any_of(topicSeqs_.cbegin(), topicSeqs_.cend(), [after](quint64 seq) { return seq > after; });
    }

    Response Router::eventsResponse(quint64 after, bool everything) const {
        QJsonArray changes;
        for (size_t i = 0; i < topicSeqs_.size(); ++i) {
            if (everything || topicSeqs_[i] > after) changes.append(QLatin1String(RouterFeedsImpl::kTopics[i].name));
        }
        return Response::Ok(QJsonObject{
            {"instance", RouterFeedsImpl::instanceId()},
            {"cursor", static_cast<qint64>(changeSeq_)},
            {"changes", changes},
        });
    }

    Response Router::logsResponse(bool sameInstance, quint64 after, int limit) const {
        const auto slice = sameInstance ? Logging::BufferedLinesAfter(after, limit) : Logging::BufferedTail(limit);
        QJsonArray lines;
        for (const auto &line : slice.lines) {
            lines.append(QJsonObject{{"seq", static_cast<qint64>(line.seq)}, {"time", line.time}, {"text", line.text}});
        }
        quint64 cursor = sameInstance ? after : slice.newestSeq;
        if (!slice.lines.isEmpty()) cursor = slice.lines.constLast().seq;
        quint64 dropped = 0;
        if (sameInstance) {
            const quint64 firstHeld = slice.oldestSeq > 0 ? slice.oldestSeq : slice.newestSeq + 1;
            if (firstHeld > after + 1) dropped = firstHeld - after - 1;
        }
        return Response::Ok(QJsonObject{
            {"instance", RouterFeedsImpl::instanceId()},
            {"cursor", static_cast<qint64>(cursor)},
            {"more", slice.more},
            {"dropped", static_cast<qint64>(dropped)},
            {"lines", lines},
        });
    }

    size_t Router::statusHashNow() const {
        auto status = statusJson();
        auto profile = status.value(QStringLiteral("profile")).toObject();
        if (!profile.isEmpty()) {
            // The running profile's traffic counters move with every transfer.
            profile.remove(QStringLiteral("traffic_up"));
            profile.remove(QStringLiteral("traffic_down"));
            status.insert(QStringLiteral("profile"), profile);
        }
        return qHash(QJsonDocument(status).toJson(QJsonDocument::Compact));
    }

    size_t Router::groupsHashNow() const {
        const auto &repo = Configs::dataManager->groupsRepo;
        const auto order = repo->GetGroupsTabOrder();
        size_t seed = qHashRange(order.cbegin(), order.cend());
        for (const int gid : repo->GetAllGroupIds()) {
            const auto group = repo->GetGroup(gid);
            if (group == nullptr) continue;
            seed = qHashMulti(seed, gid, group->name, group->url, group->archive, group->sub_last_update, group->route_id,
                              group->import_sub_route);
            const auto ids = group->Profiles();
            seed = qHashRange(ids.cbegin(), ids.cend(), seed);
        }
        return seed;
    }

    size_t Router::settingsHashNow() const {
        return qHash(QJsonDocument(Engine::SettingsRegistry::Values()).toJson(QJsonDocument::Compact));
    }

    void Router::settleEvents() {
        if (releaseFeedsOnExit()) return;
        for (const auto &pending : std::as_const(eventWaiters_)) {
            if (!pending.waiter->done && changedAfter(pending.after)) answer(pending.waiter, eventsResponse(pending.after, false));
        }
        eventWaiters_.removeIf([](const FeedWaiter &pending) { return pending.waiter->done; });
    }

    // Stops only on a tick that finds nothing held, so a client that asks again at once does not restart it.
    void Router::pollStatus() {
        if (releaseFeedsOnExit()) return;
        eventWaiters_.removeIf([](const FeedWaiter &pending) { return pending.waiter->done; });
        if (eventWaiters_.isEmpty()) {
            statusPoll_->stop();
            return;
        }
        const auto hash = statusHashNow();
        if (statusHash_ && *statusHash_ != hash) noteChanged(Engine::ChangeTopic::Status);
        statusHash_ = hash;
        pollRepos();
    }

    void Router::pollRepos() {
        const auto &data = Configs::dataManager;
        const RepoRevisions now{data->groupsRepo->Revision(), data->profilesRepo->Revision(), data->routesRepo->Revision(),
                                data->settingsRepo->Revision()};
        if (!repoRevisions_) {
            repoRevisions_ = now;
            groupsHash_ = groupsHashNow();
            settingsHash_ = settingsHashNow();
            return;
        }
        Engine::ChangeTopics topics;
        if (now.groups != repoRevisions_->groups) {
            const auto hash = groupsHashNow();
            if (hash != groupsHash_) topics |= Engine::ChangeTopic::Groups | Engine::ChangeTopic::Profiles;
            groupsHash_ = hash;
        }
        if (now.profiles != repoRevisions_->profiles) topics |= Engine::ChangeTopic::Profiles;
        if (now.routes != repoRevisions_->routes) topics |= Engine::ChangeTopic::Status;
        if (now.settings != repoRevisions_->settings) {
            const auto hash = settingsHashNow();
            if (hash != settingsHash_) topics |= Engine::ChangeTopic::Settings | Engine::ChangeTopic::Status;
            settingsHash_ = hash;
        }
        repoRevisions_ = now;
        noteChanged(topics);
    }

    void Router::pollLogs() {
        if (releaseFeedsOnExit()) return;
        const quint64 newest = Logging::BufferedNewestSeq();
        for (const auto &pending : std::as_const(logWaiters_)) {
            if (!pending.waiter->done && newest > pending.after) answer(pending.waiter, logsResponse(true, pending.after, pending.limit));
        }
        logWaiters_.removeIf([](const FeedWaiter &pending) { return pending.waiter->done; });
        if (logWaiters_.isEmpty()) logPoll_->stop();
    }

    bool Router::releaseFeedsOnExit() {
        if (!Configs::dataManager->settingsRepo->prepare_exit) return false;
        const auto exiting = Response::Error(503, QStringLiteral("exiting"), QStringLiteral("Throne is shutting down"));
        for (const auto &pending : std::exchange(eventWaiters_, {})) answer(pending.waiter, exiting);
        for (const auto &pending : std::exchange(logWaiters_, {})) answer(pending.waiter, exiting);
        eventSettle_->stop();
        statusPoll_->stop();
        logPoll_->stop();
        return true;
    }
}
