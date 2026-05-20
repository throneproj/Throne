#pragma once

#ifndef Q_MOC_RUN
#include <core/server/gen/libcore.pb.h>
#endif
#include <QString>

class QLocalSocket;

namespace API {
    class Client {
    public:
        Client();

        ~Client();

        // Adopt a freshly connected socket, replacing any previous
        // connection. The Client itself is long-lived and never recreated.
        void Reconnect(QLocalSocket *socket);

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

        bool CheckNaive(bool* rpcOK) const;

        libcore::DebugCheckResult DebugCheck(bool *rpcOK, const libcore::DebugCheckRequest &request);

    private:
        class LocalSocketChannel;
        std::unique_ptr<LocalSocketChannel> channel;
    };

    inline Client *defaultClient;
} // namespace API
