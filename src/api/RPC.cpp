#include "include/api/RPC.h"

#include "include/global/Configs.hpp"

namespace API {

    Client::Client(std::function<void(const QString &)> onError, const QString &host, int port) {
        this->make_rpc_client = [=]() { return std::make_unique<protorpc::Client>(host.toStdString().c_str(), port); };
        this->onError = std::move(onError);
    }

#define NOT_OK      \
    *rpcOK = false; \
    onError(QString("LibcoreService error: %1\n").arg(QString::fromStdString(err.String())));

    QString Client::Start(bool *rpcOK, const libcore::LoadConfigReq &request) {
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

    QStringList Client::GetGeoList(bool *rpcOK, GeoRuleSetType mode, const QString& basePath) {
        switch (mode) {
            case GeoRuleSetType::ip: {
                libcore::GeoListRequest request;
                libcore::GetGeoIPListResponse reply;
                request.path = basePath.toStdString();
                std::string resp, req = spb::pb::serialize<std::string>(request);
                auto err = make_rpc_client()->CallMethod("LibcoreService.GetGeoIPList", &req, &resp);

                if(err.IsNil()) {
                    QStringList res;
                    reply = spb::pb::deserialize< libcore::GetGeoIPListResponse >( resp );
                    for (const auto & i : reply.items) {
                        res.append(QString::fromStdString(i));
                    }
                    *rpcOK = true;
                    return res;
                } else {
                    NOT_OK
                    return {};
                }
            }
            case GeoRuleSetType::site: {
                libcore::GeoListRequest request;
                libcore::GetGeoSiteListResponse reply;
                request.path = basePath.toStdString();
                std::string resp, req = spb::pb::serialize<std::string>(request);
                auto err = make_rpc_client()->CallMethod("LibcoreService.GetGeoSiteList", &req, &resp);

                if(err.IsNil()) {
                    QStringList res;
                    reply = spb::pb::deserialize< libcore::GetGeoSiteListResponse >( resp );
                    for (const auto & i : reply.items) {
                        res.append(QString::fromStdString(i));
                    }
                    *rpcOK = true;
                    return res;
                } else {
                    NOT_OK
                    return {};
                }
            }
        }
        return {};
    }

    QString Client::CompileGeoSet(bool *rpcOK, GeoRuleSetType mode, std::string category, const QString& basePath) {
        switch (mode) {
            case ip: {
                libcore::CompileGeoIPToSrsRequest request;
                libcore::EmptyResp reply;
                request.item = category;
                request.path = basePath.toStdString();
                std::string resp, req = spb::pb::serialize<std::string>(request);

                auto err = make_rpc_client()->CallMethod("LibcoreService.CompileGeoIPToSrs", &req, &resp);
                if(err.IsNil()) {
                    *rpcOK = true;
                    return "";
                } else {
                    NOT_OK
                    return QString::fromStdString(err.String());
                }
            }
            case site: {
                libcore::CompileGeoSiteToSrsRequest request;
                libcore::EmptyResp reply;
                request.item = category;
                request.path = basePath.toStdString();
                std::string resp, req = spb::pb::serialize<std::string>(request);

                auto err = make_rpc_client()->CallMethod("LibcoreService.CompileGeoSiteToSrs", &req, &resp);
                if(err.IsNil()) {
                    *rpcOK = true;
                    return "";
                } else {
                    NOT_OK
                    return QString::fromStdString(err.String());
                }
            }
        }
        return "";
    }

    QString Client::SetSystemDNS(bool *rpcOK, const bool clear) const {
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


} // namespace API
