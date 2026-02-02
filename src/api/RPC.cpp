#include "include/api/RPC.h"



#include "include/global/Configs.hpp"

namespace API {

    Client::Client(std::function<void(const QString &)> onError, const QString &host, int port) {
        this->make_rpc_client = [=,this]() { return std::make_unique<protorpc::Client>(host.toStdString().c_str(), port); };
        this->onError = std::move(onError);
    }

#define CHECK(method) \
if (!Configs::dataManager->settingsRepo->core_running) MW_show_log("Cannot invoke method " + QString(method) + ", core is not running");

#define NOT_OK      \
    *rpcOK = false; \
    onError(QString("LibcoreService error: %1\n").arg(QString::fromStdString(err.String())));

    QString Client::Start(bool *rpcOK, const libcore::LoadConfigReq &request) {
        CHECK("Start")
        libcore::ErrorResp reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.Start", &req, &resp);

        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::ErrorResp >(resp);
            *rpcOK = true;
            return QString::fromStdString(reply.error.value());
        } else {
            NOT_OK
            return "";
        }
    }

    QString Client::Stop(bool *rpcOK) {
        CHECK("Stop")
        libcore::EmptyReq request;
        libcore::ErrorResp reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.Stop", &req, &resp);

        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::ErrorResp >( resp );
            *rpcOK = true;
            return QString::fromStdString(reply.error.value());
        } else {
            NOT_OK
            return "";
        }
    }

    libcore::QueryStatsResp Client::QueryStats() {
        CHECK("QueryStats")
        libcore::EmptyReq request;
        libcore::QueryStatsResp reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.QueryStats", &req, &resp);

        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::QueryStatsResp >( resp );
            return reply;
        } else {
            return {};
        }
    }

    libcore::TestResp Client::Test(bool *rpcOK, const libcore::TestReq &request) {
        CHECK("Test")
        libcore::TestResp reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.Test", &req, &resp);

        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::TestResp >( resp );
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    void Client::StopTests(bool *rpcOK) {
        CHECK("StopTests")
        const libcore::EmptyReq request;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.StopTest", &req, &resp);

        if(err.IsNil()) {
            *rpcOK = true;
        } else {
            NOT_OK
        }
    }

    libcore::QueryURLTestResponse Client::QueryURLTest(bool *rpcOK)
    {
        CHECK("QueryURLTest")
        libcore::EmptyReq request;
        libcore::QueryURLTestResponse reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.QueryURLTest", &req, &resp);

        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::QueryURLTestResponse >( resp );
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    QString Client::SetSystemDNS(bool *rpcOK, const bool clear) const {
        CHECK("SetSystemDNS")
        libcore::SetSystemDNSRequest request;
        request.clear = clear;
        std::string resp, req = spb::pb::serialize<std::string>(request);

        auto err = make_rpc_client()->CallMethod("LibcoreService.SetSystemDNS", &req, &resp);
        if(err.IsNil()) {
            *rpcOK = true;
            return "";
        } else {
            NOT_OK
            return QString::fromStdString(err.String());
        }
    }

    libcore::ListConnectionsResp Client::ListConnections(bool* rpcOK) const
    {
        CHECK("ListConnections")
        libcore::EmptyReq request;
        libcore::ListConnectionsResp reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.ListConnections", &req, &resp);
        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::ListConnectionsResp >( resp );
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            MW_show_log(QString("Failed to list connections: ") + QString::fromStdString(err.String()));
            return {};
        }
    }

    QString Client::CheckConfig(bool* rpcOK, const QString& config) const
    {
        CHECK("CheckConfig")
        libcore::LoadConfigReq request;
        libcore::ErrorResp reply;
        request.core_config = config.toStdString();
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.CheckConfig", &req, &resp);

        if(err.IsNil())
        {
            reply = spb::pb::deserialize< libcore::ErrorResp >( resp );
            *rpcOK = true;
            return QString::fromStdString(reply.error.value());
        } else
        {
            NOT_OK
            return QString::fromStdString(err.String());
        }

    }

    bool Client::IsPrivileged(bool* rpcOK) const
    {
        CHECK("IsPrivileged")
        libcore::EmptyReq request;
        libcore::IsPrivilegedResponse reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.IsPrivileged", &req, &resp);

        if(err.IsNil())
        {
            reply = spb::pb::deserialize< libcore::IsPrivilegedResponse >( resp );
            *rpcOK = true;
            return reply.has_privilege.value();
        } else
        {
            NOT_OK
            return false;
        }
    }

    libcore::SpeedTestResponse Client::SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request)
    {
        CHECK("SpeedTest")
        libcore::SpeedTestResponse reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.SpeedTest", &req, &resp);

        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::SpeedTestResponse >( resp );
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    libcore::QuerySpeedTestResponse Client::QueryCurrentSpeedTests(bool *rpcOK)
    {
        CHECK("QueryCurrentSpeedTests")
        const libcore::EmptyReq request;
        libcore::QuerySpeedTestResponse reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.QuerySpeedTest", &req, &resp);

        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::QuerySpeedTestResponse >( resp );
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    libcore::QueryCountryTestResponse Client::QueryCountryTestResults(bool* rpcOK)
    {
        CHECK("QueryCountryTestResults")
        const libcore::EmptyReq request;
        libcore::QueryCountryTestResponse reply;
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.QueryCountryTest", &req, &resp);

        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::QueryCountryTestResponse >( resp );
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return reply;
        }
    }

    QString Client::Clash2Singbox(bool* rpcOK, const QString& config) const
    {
        CHECK("Clash2Singbox")
        libcore::Clash2SingboxRequest request;
        libcore::Clash2SingboxResponse reply;
        request.clash_config = config.toStdString();
        std::string resp, req = spb::pb::serialize<std::string>(request);
        auto err = make_rpc_client()->CallMethod("LibcoreService.Clash2Singbox", &req, &resp);

        if(err.IsNil()) {
            reply = spb::pb::deserialize< libcore::Clash2SingboxResponse >( resp );
            *rpcOK = true;
            QString error = QString::fromStdString(reply.error.value());
            if (!error.isEmpty()) {
                MW_show_log(QString("Failed to convert Clash config:\n") + error);
            }
            return QString::fromStdString(reply.singbox_config.value());
        } else {
            NOT_OK
            return "";
        }
    }

} // namespace API
