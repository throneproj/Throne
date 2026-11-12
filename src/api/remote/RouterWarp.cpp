#include "include/api/remote/Router.hpp"

#include <QJsonArray>
#include <QPointer>

#include "include/database/DatabaseManager.h"
#include "include/engine/EngineControl.h"
#include "include/engine/services/ServiceTypes.h"
#include "include/engine/services/WarpService.h"

namespace RemoteApi::RouterWarpImpl {
    constexpr int kRegisterWaitMs = 90000;

    QJsonObject warpJson() {
        const auto status = Engine::WarpService::Current();
        return {
            {"enabled", status.enabled},
            {"mode", status.mode},
            {"configured", status.configured},
            {"endpoint", status.endpoint},
            {"addresses", QJsonArray::fromStringList(status.addresses)},
            {"terms_accepted", status.termsAccepted},
            {"terms_url", QString::fromLatin1(Engine::WarpService::kTermsUrl)},
            {"registering", status.registering},
        };
    }

    // The start and test workers read the warp_* settings off the main thread.
    Engine::ServiceError busyConnection() {
        return {409, QStringLiteral("busy"), QStringLiteral("A profile is starting or stopping")};
    }
}

namespace RemoteApi {
    void Router::getWarp(const Request &, const Responder &respond, int) {
        respond(Response::Ok(RouterWarpImpl::warpJson()));
    }

    void Router::putWarp(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        Engine::WarpService::Change change;
        if (!bodyObject(request, true, &body, &error) || !readBool(body, QStringLiteral("enabled"), &change.enabled, &error) ||
            !readString(body, QStringLiteral("mode"), &change.mode, &error)) {
            respond(error);
            return;
        }
        if (!change.enabled && !change.mode) {
            respond(badRequest(QStringLiteral("Set \"enabled\", \"mode\" or both")));
            return;
        }
        Engine::WarpService::ChangeResult result;
        const auto beforeWrite = [this]() -> std::optional<Engine::ServiceError> {
            if (connectionBusy()) return RouterWarpImpl::busyConnection();
            return std::nullopt;
        };
        if (const auto refused = Engine::WarpService::Apply(change, beforeWrite, &result)) {
            respond(serviceError(*refused));
            return;
        }
        if (result.restartNeeded) engine_->noteRestartNeeded(Engine::RestartReason::Routing);
        if (result.changed) noteChanged(Engine::ChangeTopic::Settings | Engine::ChangeTopic::Status);
        auto json = RouterWarpImpl::warpJson();
        json.insert(QStringLiteral("restart_needed"), engine_->restartPending());
        respond(Response::Ok(json));
    }

    void Router::postWarpRegister(const Request &request, const Responder &respond, int) {
        QJsonObject body;
        Response error;
        std::optional<QString> mode;
        std::optional<bool> acceptTerms;
        if (!bodyObject(request, false, &body, &error) || !readString(body, QStringLiteral("mode"), &mode, &error) ||
            !readBool(body, QStringLiteral("accept_terms"), &acceptTerms, &error)) {
            respond(error);
            return;
        }

        const QPointer<Router> self(this);
        const auto waiter = makeWaiter(respond, RouterWarpImpl::kRegisterWaitMs, [self] {
            auto json = RouterWarpImpl::warpJson();
            json.insert(QStringLiteral("restart_needed"), self != nullptr && self->engine_->restartPending());
            return Response::Ok(json, 202);
        });
        const auto beforeStore = [self]() -> std::optional<Engine::ServiceError> {
            if (self != nullptr && self->connectionBusy()) return RouterWarpImpl::busyConnection();
            return std::nullopt;
        };
        const auto refused = Engine::WarpService::Register(
            mode.value_or(Configs::dataManager->settingsRepo->warp_mode), acceptTerms.value_or(false), beforeStore,
            [self, waiter](const Engine::WarpService::RegisterResult &result) {
                if (self == nullptr) return;
                if (result.error) {
                    self->noteChanged(Engine::ChangeTopic::Settings);
                    self->fail(waiter, serviceError(*result.error));
                    return;
                }
                if (result.restartNeeded) self->engine_->noteRestartNeeded(Engine::RestartReason::Routing);
                self->noteChanged(Engine::ChangeTopic::Settings | Engine::ChangeTopic::Status);
                auto json = RouterWarpImpl::warpJson();
                json.insert(QStringLiteral("restart_needed"), self->engine_->restartPending());
                self->answer(waiter, Response::Ok(json));
            });
        if (refused) {
            answer(waiter, serviceError(*refused));
            return;
        }
        // registering, and possibly terms_accepted, changed.
        noteChanged(Engine::ChangeTopic::Settings);
    }
}
