#include "include/api/remote/Router.hpp"

#include <QJsonArray>
#include <QPointer>
#include <QSet>

#include "include/database/DatabaseManager.h"
#include "include/engine/EngineControl.h"
#include "include/engine/services/StatsService.h"

namespace RemoteApi::RouterConnectionsImpl {
    constexpr int kCloseWaitMs = 40000;
    // The core's CloseConnections reply when no box runs (core/internal/rpc/connections.go).
    constexpr auto kCloseNotRunning = "no instance is running";

    Response notRunning() {
        return Response::Error(409, QStringLiteral("not_running"), QStringLiteral("No profile runs, so there is no connection to close"));
    }

    QJsonObject connectionJson(const Stats::ConnectionMetadata &row) {
        return {
            {"id", row.id},
            {"created_at", row.createdAtMs},
            {"closed_at", row.closedAtMs},
            {"network", row.network},
            {"protocol", row.protocol},
            {"source", row.source},
            {"source_name", row.sourceLocal ? QStringLiteral("Local") : row.sourceDisplay},
            {"destination", row.dest},
            {"domain", row.domain},
            {"process", row.process},
            {"process_path", row.processPath},
            {"outbound", row.outboundName},
            {"outbound_tag", row.outbound},
            {"rule", row.rule},
            {"rule_item", row.ruleItem},
            {"upload", row.upload},
            {"download", row.download},
            {"upload_speed", row.uploadSpeed},
            {"download_speed", row.downloadSpeed},
        };
    }
}

namespace RemoteApi {
    void Router::getConnections(const Request &, const Responder &respond, int) {
        engine_->watchConnections();
        if (engine_->runningSnapshot() == nullptr) {
            respond(Response::Ok(QJsonObject{{"running", false}, {"updated_at", 0}, {"connections", QJsonArray()}}));
            return;
        }
        // The lister polls the core only while statistics are on.
        if (!Configs::dataManager->settingsRepo->enable_stats) {
            respond(Response::Error(409, QStringLiteral("stats_disabled"), QStringLiteral("Statistics are turned off in Throne")));
            return;
        }
        const auto snapshot = engine_->connectionSnapshot();
        QJsonArray connections;
        for (const auto &row : snapshot.rows) connections.append(RouterConnectionsImpl::connectionJson(row));
        respond(Response::Ok(QJsonObject{
            {"running", true},
            {"updated_at", snapshot.updatedAtMs},
            {"connections", connections},
        }));
    }

    void Router::deleteConnections(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<bool> all;
        if (!bodyObject(request, true, &body, &error) || !readBool(body, QStringLiteral("all"), &all, &error)) {
            respond(error);
            return;
        }
        const bool byIds = present(body, QStringLiteral("ids"));
        if (byIds == all.has_value() || (all.has_value() && !*all)) {
            respond(badRequest(QStringLiteral("Name the connections with either \"ids\" or \"all\": true")));
            return;
        }
        QStringList ids;
        if (byIds) {
            const auto list = body.value(QStringLiteral("ids"));
            const auto mistyped = badRequest(QStringLiteral("\"ids\" must be a non-empty array of connection ids"));
            if (!list.isArray() || list.toArray().isEmpty()) {
                respond(mistyped);
                return;
            }
            QSet<QString> seen;
            for (const auto &item : list.toArray()) {
                if (!item.isString() || item.toString().isEmpty()) {
                    respond(mistyped);
                    return;
                }
                if (seen.contains(item.toString())) continue;
                seen.insert(item.toString());
                ids.append(item.toString());
            }
        }
        if (engine_->runningSnapshot() == nullptr) {
            respond(RouterConnectionsImpl::notRunning());
            return;
        }
        if (!byIds) {
            for (const auto &row : engine_->connectionSnapshot().rows) {
                if (row.closedAtMs == 0) ids.append(row.id);
            }
        }

        const auto waiter = makeWaiter(respond, RouterConnectionsImpl::kCloseWaitMs, [] {
            return Response::Error(503, QStringLiteral("core_unavailable"), QStringLiteral("Throne's core did not answer in time"));
        });
        const QPointer<Router> self(this);
        engine_->closeConnections(ids, [self, waiter](const Engine::ConnectionCloseResult &result) {
            if (self == nullptr) return;
            if (!result.rpcOK) {
                self->fail(waiter, Response::Error(503, QStringLiteral("core_unavailable"),
                                                   QStringLiteral("Throne's core process is not reachable: %1").arg(result.error)));
                return;
            }
            if (result.error.trimmed() == QLatin1String(RouterConnectionsImpl::kCloseNotRunning)) {
                self->fail(waiter, RouterConnectionsImpl::notRunning());
                return;
            }
            if (!result.error.isEmpty()) {
                self->fail(waiter, Response::Error(503, QStringLiteral("core_unavailable"), result.error));
                return;
            }
            self->answer(waiter, Response::Ok(QJsonObject{{"closed", result.closed}}));
        });
    }
}
