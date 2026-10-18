#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

#include "include/api/remote/Message.hpp"

class QRegularExpression;
class QTimer;

namespace Configs {
    class Group;
    class Profile;
    class RouteProfile;
}

namespace Engine {
    class EngineControl;
    struct ServiceError;
}

namespace Subscription {
    struct UpdateOutcome;
}

namespace RemoteApi {
    // The endpoints of spec/api.md in throneproj/Throne-api. UI thread only.
    class Router : public QObject {
        Q_OBJECT

    public:
        Router(Engine::EngineControl *engine, QObject *parent);

        void dispatch(const Request &request, const Responder &respond);

        // Main thread; also the headless instance socket's HEALTH reply.
        QJsonObject statusJson() const;

    private:
        // Answered exactly once: by the change it waits for, or with onDeadline() once its timer fires.
        struct Waiter {
            Responder respond;
            std::function<Response()> onDeadline;
            QTimer *deadline = nullptr;
            bool done = false;
        };
        using WaiterPtr = std::shared_ptr<Waiter>;

        struct WaitOptions {
            bool wait = true;
            int timeoutMs = 30000;
        };

        struct Job {
            int id = 0;
            QString kind;
            QString state;
            QList<int> profileIds;
            qint64 startedAt = 0;
            qint64 finishedAt = 0;
            bool cancelRequested = false;
            QList<WaiterPtr> waiters;
        };

        // id is the {id} path parameter, -1 for a path without one.
        using Handler = void (Router::*)(const Request &request, const Responder &respond, int id);

        void getStatus(const Request &request, const Responder &respond, int id);
        void getGroups(const Request &request, const Responder &respond, int id);
        void getProfiles(const Request &request, const Responder &respond, int id);
        void getProfile(const Request &request, const Responder &respond, int id);
        void putConnection(const Request &request, const Responder &respond, int id);
        void deleteConnection(const Request &request, const Responder &respond, int id);
        void putModes(const Request &request, const Responder &respond, int id);
        void getRoutes(const Request &request, const Responder &respond, int id);
        void putActiveRoute(const Request &request, const Responder &respond, int id);
        void postTest(const Request &request, const Responder &respond, int id);
        void getTests(const Request &request, const Responder &respond, int id);
        void getTest(const Request &request, const Responder &respond, int id);
        void deleteTest(const Request &request, const Responder &respond, int id);
        void deleteResults(const Request &request, const Responder &respond, int id);
        void getLiveStats(const Request &request, const Responder &respond, int id);
        void getTrafficStats(const Request &request, const Responder &respond, int id);

        // Management (RouterManage.cpp). dispatch() refuses the admin ones for a request without Request::admin.
        void postImport(const Request &request, const Responder &respond, int id);
        void postGroup(const Request &request, const Responder &respond, int id);
        void postGroupUpdate(const Request &request, const Responder &respond, int id);
        void postGroupsUpdate(const Request &request, const Responder &respond, int id);
        void deleteGroup(const Request &request, const Responder &respond, int id);
        void deleteProfiles(const Request &request, const Responder &respond, int id);
        void getSettings(const Request &request, const Responder &respond, int id);
        void putSettings(const Request &request, const Responder &respond, int id);
        void postRuleSetsUpdate(const Request &request, const Responder &respond, int id);
        void getGeoAssets(const Request &request, const Responder &respond, int id);
        void postGeoAssetsUpdate(const Request &request, const Responder &respond, int id);
        void postRestart(const Request &request, const Responder &respond, int id);
        void postShutdown(const Request &request, const Responder &respond, int id);

        // Backup and restore (RouterBackup.cpp).
        void postBackup(const Request &request, const Responder &respond, int id);
        void getBackup(const Request &request, const Responder &respond, int id);
        void deleteBackup(const Request &request, const Responder &respond, int id);
        void postRestore(const Request &request, const Responder &respond, int id);
        void putRestore(const Request &request, const Responder &respond, int id);
        void getRestore(const Request &request, const Responder &respond, int id);
        void postRestoreApply(const Request &request, const Responder &respond, int id);
        void deleteRestore(const Request &request, const Responder &respond, int id);

        static Response badRequest(const QString &message);
        static Response notFound(const QString &message);
        static Response noProfiles();
        static Response forbidden();
        // what: "Throne's settings window", "this group", ...
        static Response editing(const QString &what);
        static Response serviceError(const Engine::ServiceError &error);
        static QJsonObject updateResultJson(const Subscription::UpdateOutcome &outcome);

        // The readers leave *out alone for a missing or null field, and fail with a bad_request in *error for a mistyped one.
        static bool bodyObject(const Request &request, bool required, QJsonObject *body, Response *error);
        static bool present(const QJsonObject &body, const QString &key);
        static bool readBool(const QJsonObject &body, const QString &key, std::optional<bool> *out, Response *error);
        static bool readNumber(const QJsonObject &body, const QString &key, std::optional<double> *out, Response *error);
        static bool readInteger(const QJsonObject &body, const QString &key, std::optional<qint64> *out, Response *error);
        static bool readId(const QJsonObject &body, const QString &key, std::optional<int> *out, Response *error);
        static bool readString(const QJsonObject &body, const QString &key, std::optional<QString> *out, Response *error);
        static bool readWait(const QJsonObject &body, bool defaultWait, int defaultTimeoutSec, WaitOptions *out, Response *error);
        static bool readNameFilter(const QString &pattern, QRegularExpression *out, Response *error);
        // `profiles`, or `group` with an optional `name`; in order, without repeats.
        bool readSelection(const QJsonObject &body, QList<int> *ids, Response *error) const;

        QList<std::shared_ptr<Configs::Profile>> groupProfiles(int gid, const QRegularExpression &nameFilter) const;
        bool connectionBusy() const;

        QJsonObject profileJson(const std::shared_ptr<Configs::Profile> &profile) const;
        static QJsonObject groupJson(const Configs::Group &group);
        static QJsonObject routeJson(const Configs::RouteProfile &route);
        QJsonObject jobJson(const Job &job, bool withResults) const;
        Response jobResponse(const Job &job) const;
        Response accepted() const;

        WaiterPtr makeWaiter(const Responder &respond, int timeoutMs, std::function<Response()> onDeadline);
        void answer(const WaiterPtr &waiter, const Response &response);
        // Answers the waiter, or logs the error when nobody waits for it any more.
        void fail(const WaiterPtr &waiter, const Response &error);
        void completeStart(const WaiterPtr &waiter, const std::optional<Response> &failure);
        // Runs apply(serial), which returns whether it restarted the profile with that serial, then waits for the restart.
        void applyWithRestart(const WaitOptions &options, const Responder &respond, const std::function<bool(quint64)> &apply);
        // Starts the profile unattended; returns the response instead when no start is needed or possible.
        std::optional<Response> launch(int profileId, bool restart, const WaiterPtr &waiter);
        void startLowestLatency(const QJsonObject &body, bool restart, const WaitOptions &options, const Responder &respond);
        void startBestCandidate(const QList<int> &candidates, qint64 requestedAt, qint64 maxAge, bool restart, const WaiterPtr &waiter);
        void finishJob(int jobId, bool stopped);
        std::shared_ptr<Job> findJob(int jobId) const;
        // Runs then() on the main thread once the running profile is out of the way: at once unless stopRunning and a
        // profile runs, else after an unattended, non-blocking stop finished. Returns busy instead while a start or stop
        // is in flight. then() runs even when the request's waiter has already timed out.
        std::optional<Response> afterStop(bool stopRunning, const std::function<void()> &then);

        Engine::EngineControl *engine_;
        // Keyed by StartRequest serial; called once, with no failure when the profile started.
        QHash<quint64, std::function<void(const std::optional<Response> &failure)>> starts_;
        QList<std::function<void()>> stops_;
        // Started, waiting for the window to leave the connecting state before the Status is final.
        QList<WaiterPtr> settling_;
        // Newest first.
        QList<std::shared_ptr<Job>> jobs_;
        int lastJobId_ = 0;

        // A backup kept for download, or an archive being uploaded for a restore; defined in RouterBackup.cpp.
        struct Transfer;
        QList<std::shared_ptr<Transfer>> backups_;
        QList<std::shared_ptr<Transfer>> restores_;
        int lastTransferId_ = 0;
        QTimer *transferSweep_ = nullptr;
        // A restore was applied and the restart is queued: every request is answered 503 restarting.
        bool restarting_ = false;
    };
}
