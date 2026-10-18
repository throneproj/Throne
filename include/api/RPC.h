#pragma once

#ifndef Q_MOC_RUN
#include <core/gen/libcore.pb.h>
#endif
#include <optional>

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

class QLocalSocket;

namespace API {
    // The core's privilege report. The optionals are unset where the core cannot tell (another OS, an older core).
    struct PrivilegeInfo {
        // euid 0 or the Tun capabilities (Linux), as IsPrivileged always meant.
        bool privileged = false;
        // Linux: /dev/net/tun opened read-write.
        std::optional<bool> tunDevice;
        // Linux: CAP_NET_ADMIN in the effective set.
        std::optional<bool> netAdmin;
        // Linux: euid 0 and CAP_NET_ADMIN; macOS: euid 0.
        std::optional<bool> killSwitch;
        // The first failing reason, plain English; empty when nothing failed.
        QString detail;
    };

    class Client {
    public:
        Client();

        ~Client();

        void Reconnect(QLocalSocket *socket);

        // False until the GUI accepts the core's socket, which is after the core reports itself connected.
        [[nodiscard]] bool IsConnected() const;

        // QString returns is error string

        QString Start(bool *rpcOK, const libcore::LoadConfigReq &request);

        // timeoutMs 0 = the channel default.
        QString Stop(bool *rpcOK, int timeoutMs = 0);

        libcore::QueryStatsResp QueryStats();

        // coreError (optional): on RPC failure, receives the core's error message. timeoutMs 0 = the channel default.
        libcore::TestResp Test(bool *rpcOK, const libcore::TestReq &request, QString *coreError = nullptr, int timeoutMs = 0);

        void StopTests(bool *rpcOK);

        libcore::QueryURLTestResponse QueryURLTest(bool *rpcOK);

        libcore::IPTestResp IPTest(bool *rpcOK, const libcore::IPTestRequest &request, QString *coreError = nullptr, int timeoutMs = 0);

        libcore::QueryIPTestResponse QueryIPTest(bool *rpcOK);

        [[nodiscard]] libcore::QueryConnectionsResp QueryConnections() const;

        // Ids already gone are a no-op; closedCount (optional) receives how many were actually live.
        QString CloseConnections(bool *rpcOK, const QStringList &ids, int *closedCount = nullptr) const;

        QString UpdateRuleSets(bool *rpcOK, int *updatedCount = nullptr) const;

        QString CheckConfig(bool *rpcOK, const QString& config, bool isXray = false) const;

        // timeoutMs 0 = the channel default.
        bool IsPrivileged(bool *rpcOK, int timeoutMs = 0) const;

        // IsPrivileged with the core's capability fields (IsPrivilegedResponse 2-5).
        PrivilegeInfo Privileges(bool *rpcOK, int timeoutMs = 0) const;

        libcore::SpeedTestResponse SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request, QString *coreError = nullptr);

        libcore::QuerySpeedTestResponse QueryCurrentSpeedTests(bool *rpcOK);

        libcore::QueryCountryTestResponse QueryCountryTestResults(bool *rpcOK);

        libcore::GenWgKeyPairResponse GenWgKeyPair(bool *rpcOK);

        libcore::WarpRegisterResponse WarpRegister(bool *rpcOK, const QString &tunnelType, const QString &proxy,
                                                   const QStringList &apiHosts);

        libcore::GenQuicSignatureResponse GenQuicSignature(bool *rpcOK, const QString &domain);

        QString InstallDashboard(bool *rpcOK, const QString &archivePath, const QString &targetDir) const;

        // Empty name = the OS has no default route.
        [[nodiscard]] libcore::GetDefaultInterfaceResponse GetDefaultInterface(bool *rpcOK) const;

        // Clears no core-side counters, so polling it alongside QueryStats is safe.
        [[nodiscard]] libcore::QueryAutoSelectorsResponse QueryAutoSelectors(bool *rpcOK) const;

        // action: "recheck" (sweep now) | "select" (pin to member); an empty tag targets every group.
        QString AutoSelectorAction(bool *rpcOK, const QString &tag, const QString &action,
                                   const QString &member = {}) const;

        // Running instance only; a test box reports through Test itself (TestResp::vpn_status).
        [[nodiscard]] libcore::VPNStatusResponse QueryVPNStatus(bool *rpcOK, const QStringList &endpointTags,
                                                                int timeoutMs = 0) const;

        // OpenVPN reads username/password/secret; OpenConnect reads formValues keyed by submission_key.
        QString SubmitVPNChallenge(bool *rpcOK, const QString &endpointTag, const QString &challengeId,
                                   const QString &username, const QString &password, const QString &secret,
                                   const QMap<QString, QString> &formValues = {}) const;

        QString CancelVPNChallenge(bool *rpcOK, const QString &endpointTag, const QString &challengeId) const;

        // Blocks for the whole capture window; a timeout does not stop the core, only StopDiagnostics does.
        libcore::DiagnosticsResponse CaptureDiagnostics(bool *rpcOK, const libcore::DiagnosticsRequest &request, int timeoutMs);

        void StopDiagnostics(bool *rpcOK);

        // Scanner calls are scoped to request.session_id: StopScan cancels that session only, never URL tests.
        libcore::ScanProbeResponse ScanProbe(bool *rpcOK, const libcore::ScanProbeRequest &request, QString *coreError, int timeoutMs);

        [[nodiscard]] libcore::QueryScanResponse QueryScan(bool *rpcOK, const QString &sessionId, qint64 afterSeq) const;

        void StopScan(bool *rpcOK, const QString &sessionId) const;

        libcore::TestResp ScanURLTest(bool *rpcOK, const libcore::ScanURLTestRequest &request, QString *coreError, int timeoutMs);

        // Up when any target accepts a TCP connection through the default interface.
        [[nodiscard]] bool ScanCheckNetwork(bool *rpcOK, const QStringList &targets, int timeoutMs, QString *error = nullptr) const;

        // Returns the error text; empty on success.
        QString ParseRuleSet(bool *rpcOK, const QByteArray &content, QStringList *cidrs, int *skippedRules = nullptr) const;

    private:
        class LocalSocketChannel;
        std::unique_ptr<LocalSocketChannel> channel;
    };

    inline Client *defaultClient;
} // namespace API
