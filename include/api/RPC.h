#pragma once

#ifndef Q_MOC_RUN
#include "libcore.pb.h"
#endif
#include <QString>
#include "3rdparty/protorpc/rpc_client.h"

namespace API {
    class Client {
    public:
        explicit Client(std::function<void(const QString &)> onError, const QString &host, int port);

        // QString returns is error string

        QString Start(bool *rpcOK, const libcore::LoadConfigReq &request);

        QString Stop(bool *rpcOK);

        libcore::QueryStatsResp QueryStats();

        libcore::TestResp Test(bool *rpcOK, const libcore::TestReq &request);

        void StopTests(bool *rpcOK);

        libcore::QueryURLTestResponse QueryURLTest(bool *rpcOK);

        QString SetSystemDNS(bool *rpcOK, bool clear) const;

        libcore::ListConnectionsResp ListConnections(bool *rpcOK) const;

        QString CheckConfig(bool *rpcOK, const QString& config) const;

        bool IsPrivileged(bool *rpcOK) const;

        libcore::SpeedTestResponse SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request);

        libcore::QuerySpeedTestResponse QueryCurrentSpeedTests(bool *rpcOK);

    private:
        std::function<std::unique_ptr<protorpc::Client>()> make_rpc_client;
        std::function<void(const QString &)> onError;
    };

    inline Client *defaultClient;
} // namespace API
