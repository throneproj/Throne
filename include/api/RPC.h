#pragma once

#ifndef Q_MOC_RUN
#include <core/server/gen/libcore.pb.h>
#endif
#include <QString>

class QLocalSocket;

namespace API {
    class Client {
    public:
        explicit Client(std::function<void(const QString &)> onError, QLocalSocket *socket);

        ~Client();

        // QString returns is error string

        QString Start(bool *rpcOK, const libcore::LoadConfigReq &request);

        QString Stop(bool *rpcOK);

        libcore::QueryStatsResp QueryStats();

        libcore::TestResp Test(bool *rpcOK, const libcore::TestReq &request);

        void StopTests(bool *rpcOK);

        libcore::QueryURLTestResponse QueryURLTest(bool *rpcOK);

        libcore::IPTestResp IPTest(bool *rpcOK, const libcore::IPTestRequest &request);

        libcore::QueryIPTestResponse QueryIPTest(bool *rpcOK);

        QString SetSystemDNS(bool *rpcOK, bool clear) const;

        [[nodiscard]] libcore::ListConnectionsResp ListConnections() const;

        QString CheckConfig(bool *rpcOK, const QString& config) const;

        bool IsPrivileged(bool *rpcOK) const;

        libcore::SpeedTestResponse SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request);

        libcore::QuerySpeedTestResponse QueryCurrentSpeedTests(bool *rpcOK);

        libcore::QueryCountryTestResponse QueryCountryTestResults(bool *rpcOK);

        libcore::GenWgKeyPairResponse GenWgKeyPair(bool *rpcOK);

    private:
        class LocalSocketChannel;
        std::unique_ptr<LocalSocketChannel> channel;
        std::function<void(const QString &)> onError;
    };

    inline Client *defaultClient;
} // namespace API
