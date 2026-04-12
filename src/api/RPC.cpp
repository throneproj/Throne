#include "include/api/RPC.h"
#include <utility>

#include "include/global/Configs.hpp"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QTimer>
#include <QtEndian>
#include <QThread>
#include <QMutex>
#include <QAbstractNetworkCache>

namespace QtGrpc {
    const char *GrpcAcceptEncodingHeader = "grpc-accept-encoding";
    const char *AcceptEncodingHeader = "accept-encoding";
    const char *TEHeader = "te";
    const char *GrpcStatusHeader = "grpc-status";
    const char *GrpcStatusMessage = "grpc-message";
    const int GrpcMessageSizeHeaderSize = 5;

    class NoCache : public QAbstractNetworkCache {
    public:
        QNetworkCacheMetaData metaData(const QUrl &url) override {
            return {};
        }
        void updateMetaData(const QNetworkCacheMetaData &metaData) override {
        }
        QIODevice *data(const QUrl &url) override {
            return nullptr;
        }
        bool remove(const QUrl &url) override {
            return false;
        }
        [[nodiscard]] qint64 cacheSize() const override {
            return 0;
        }
        QIODevice *prepare(const QNetworkCacheMetaData &metaData) override {
            return nullptr;
        }
        void insert(QIODevice *device) override {
        }
        void clear() override {
        }
    };

    class Http2GrpcChannelPrivate {
    private:
        QThread *thread;
        QNetworkAccessManager *nm;

        QString url_base;
        QString serviceName;

        // async
        QNetworkReply *post(const QString &method, const QString &service, const QByteArray &args) {
            QUrl callUrl = url_base + "/" + service + "/" + method;

            QNetworkRequest request(callUrl);
            request.setAttribute(QNetworkRequest::Http2DirectAttribute, true);
            request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String{"application/grpc"});
            request.setRawHeader("Cache-Control", "no-store");
            request.setRawHeader(GrpcAcceptEncodingHeader, QByteArray{"identity,deflate,gzip"});
            request.setRawHeader(AcceptEncodingHeader, QByteArray{"identity,gzip"});
            request.setRawHeader(TEHeader, QByteArray{"trailers"});

            QByteArray msg(GrpcMessageSizeHeaderSize, '\0');
            *reinterpret_cast<int *>(msg.data() + 1) = qToBigEndian((int) args.size());
            msg += args;

            QNetworkReply *networkReply = nm->post(request, msg);
            return networkReply;
        }

        static QByteArray processReply(QNetworkReply *networkReply, QNetworkReply::NetworkError &statusCode) {
            // Check if no network error occured
            if (networkReply->error() != QNetworkReply::NoError) {
                statusCode = networkReply->error();
                return {};
            }

            // Check if server answer with error
            auto errCode = networkReply->rawHeader(GrpcStatusHeader).toInt();
            if (errCode != 0) {
                QStringList errstr;
                errstr << "grpc-status error code:" << Int2String(errCode) << ", error msg:"
                       << QLatin1String(networkReply->rawHeader(GrpcStatusMessage));
                MW_show_log(errstr.join(" "));
                statusCode = QNetworkReply::NetworkError::ProtocolUnknownError;
                return {};
            }
            statusCode = QNetworkReply::NetworkError::NoError;
            return networkReply->readAll().mid(GrpcMessageSizeHeaderSize);
        }

        QNetworkReply::NetworkError call(const QString &method, const QString &service, const QByteArray &args, QByteArray &qByteArray, int timeout_ms) {
            QNetworkReply *networkReply = post(method, service, args);

            QTimer *abortTimer = nullptr;
            if (timeout_ms > 0) {
                abortTimer = new QTimer;
                abortTimer->setSingleShot(true);
                abortTimer->setInterval(timeout_ms);
                QObject::connect(abortTimer, &QTimer::timeout, networkReply, &QNetworkReply::abort);
                abortTimer->start();
            }

            {
                QEventLoop loop;
                QObject::connect(networkReply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                loop.exec();
            }

            if (abortTimer != nullptr) {
                abortTimer->stop();
                abortTimer->deleteLater();
            }

            auto grpcStatus = QNetworkReply::NetworkError::ProtocolUnknownError;
            qByteArray = processReply(networkReply, grpcStatus);

            networkReply->deleteLater();
            return grpcStatus;
        }

    public:
        Http2GrpcChannelPrivate(const QString &url_, const QString &serviceName_) {
            url_base = "http://" + url_;
            serviceName = serviceName_;
            //
            thread = new QThread;
            nm = new QNetworkAccessManager();
            nm->setCache(new NoCache);
            nm->moveToThread(thread);
            thread->start();
        }

        ~Http2GrpcChannelPrivate() {
            nm->deleteLater();
            thread->quit();
            thread->wait();
            thread->deleteLater();
        }

        QNetworkReply::NetworkError Call(const QString &methodName,
                                         const std::string req, std::vector<uint8_t> &rsp,
                                         int timeout_ms = 0) {
            if (!Configs::dataManager->settingsRepo->core_running) return QNetworkReply::NetworkError(-1919);

            auto requestArray = QByteArray::fromStdString(req);

            QByteArray responseArray;
            QNetworkReply::NetworkError err;
            QMutex lock;
            lock.lock();

            runOnThread(
                [&] {
                    err = call(methodName, serviceName, requestArray, responseArray, timeout_ms);
                    lock.unlock();
                },
                nm);

            lock.lock();
            lock.unlock();
            // qDebug() << "rsp err" << err;
            // qDebug() << "rsp array" << responseArray;

            if (err != QNetworkReply::NetworkError::NoError) {
                return err;
            }
            rsp.assign(responseArray.begin(), responseArray.end());
            return QNetworkReply::NetworkError::NoError;
        }
    };
} // namespace QtGrpc

namespace API {

    Client::Client(std::function<void(const QString &)> onError, const QString &target) {
        this->make_grpc_channel = [=]() { return std::make_unique<QtGrpc::Http2GrpcChannelPrivate>(target, "libcore.LibcoreService"); };
        this->default_grpc_channel = make_grpc_channel();
        this->onError = std::move(onError);
    }

#define NOT_OK      \
    *rpcOK = false; \
    onError(QString("QNetworkReply::NetworkError code: %1\n").arg(status));

    QString Client::Start(bool *rpcOK, const libcore::LoadConfigReq &request) {
        libcore::ErrorResp reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("Start", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::ErrorResp>(resp);
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
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("Stop", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::ErrorResp>(resp);
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
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("QueryStats", spb::pb::serialize<std::string>(request), resp, 500);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QueryStatsResp>(resp);
            return reply;
        } else {
            return {};
        }
    }

    libcore::TestResp Client::Test(bool *rpcOK, const libcore::TestReq &request) {
        libcore::TestResp reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("Test", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::TestResp>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

    void Client::StopTests(bool *rpcOK) {
        const libcore::EmptyReq request;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("StopTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            *rpcOK = true;
        } else {
            NOT_OK
        }
    }

    libcore::QueryURLTestResponse Client::QueryURLTest(bool *rpcOK)
    {
        libcore::EmptyReq request;
        libcore::QueryURLTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("QueryURLTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QueryURLTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

    libcore::IPTestResp Client::IPTest(bool *rpcOK, const libcore::IPTestRequest &request) {
        libcore::IPTestResp reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("IPTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::IPTestResp>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

    libcore::QueryIPTestResponse Client::QueryIPTest(bool *rpcOK) {
        libcore::EmptyReq request;
        libcore::QueryIPTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("QueryIPTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QueryIPTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

    QString Client::SetSystemDNS(bool *rpcOK, const bool clear) const {
        libcore::SetSystemDNSRequest request{clear};
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("SetSystemDNS", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            *rpcOK = true;
            return "";
        } else {
            NOT_OK
            return qt_error_string(status);
        }
    }

    libcore::ListConnectionsResp Client::ListConnections() const
    {
        libcore::EmptyReq request;
        libcore::ListConnectionsResp reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("ListConnections", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::ListConnectionsResp>(resp);
            return reply;
        } else {
            MW_show_log(QString("Failed to list connections: " + qt_error_string(status)));
            return {};
        }
    }

    QString Client::CheckConfig(bool* rpcOK, const QString& config) const
    {
        libcore::LoadConfigReq request{.core_config = config.toStdString()};
        libcore::ErrorResp reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("CheckConfig", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError)
        {
            reply = spb::pb::deserialize<libcore::ErrorResp>(resp);
            *rpcOK = true;
            return QString::fromStdString(reply.error.value());
        } else
        {
            NOT_OK
            return qt_error_string(status);
        }

    }

    bool Client::IsPrivileged(bool* rpcOK) const
    {
        libcore::EmptyReq request;
        libcore::IsPrivilegedResponse reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("IsPrivileged", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError)
        {
            reply = spb::pb::deserialize<libcore::IsPrivilegedResponse>(resp);
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
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("SpeedTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::SpeedTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

    libcore::QuerySpeedTestResponse Client::QueryCurrentSpeedTests(bool *rpcOK)
    {
        const libcore::EmptyReq request;
        libcore::QuerySpeedTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("QuerySpeedTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QuerySpeedTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

    libcore::QueryCountryTestResponse Client::QueryCountryTestResults(bool* rpcOK)
    {
        const libcore::EmptyReq request;
        libcore::QueryCountryTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("QueryCountryTest", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError) {
            reply = spb::pb::deserialize<libcore::QueryCountryTestResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

    libcore::GenWgKeyPairResponse Client::GenWgKeyPair(bool *rpcOK)
    {
        const libcore::EmptyReq request;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("GenWgKeyPair", spb::pb::serialize<std::string>(request),  resp);

        if (status == QNetworkReply::NoError) {
            auto reply = spb::pb::deserialize<libcore::GenWgKeyPairResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

    bool Client::CheckNaive(bool* rpcOK) const
    {
        libcore::EmptyReq request;
        libcore::CheckNaiveResponse reply;
        std::vector<uint8_t> resp;
        auto status = default_grpc_channel->Call("CheckNaive", spb::pb::serialize<std::string>(request), resp);

        if (status == QNetworkReply::NoError)
        {
            reply = spb::pb::deserialize<libcore::CheckNaiveResponse>(resp);
            *rpcOK = true;
            return reply.has_naive.value();
        } else
        {
            NOT_OK
            return false;
        }
    }

} // namespace API
