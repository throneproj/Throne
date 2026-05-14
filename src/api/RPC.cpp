#include "include/api/RPC.h"
#include <utility>

#include "include/global/Configs.hpp"

#include <QLocalSocket>
#include <QDataStream>
#include <QAtomicInt>
#include <QMap>
#include <QMutex>
#include <QThread>

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

namespace API {

    // -----------------------------------------------------------------------
    // LocalSocketChannel — multiplexed framing over QLocalSocket
    //
    // Request wire format (little-endian):
    //   [uint32: request ID]
    //   [uint16: method name length][method name bytes]
    //   [uint32: payload length][payload bytes]
    //
    // Response wire format (little-endian):
    //   [uint32: request ID]
    //   [uint8:  status (0=OK, 1=error)]
    //   [uint32: data length][data bytes]
    //
    // Multiple calls can be in flight simultaneously. The io_thread owns the
    // socket and processes incoming data via readyRead. Each Call() dispatches
    // its write via QMetaObject::invokeMethod (event-loop safe, FIFO order)
    // and then blocks on a per-call condition variable until the matching
    // response ID arrives.
    // -----------------------------------------------------------------------
    class Client::LocalSocketChannel {

        struct PendingCall {
            std::mutex          mu;
            std::condition_variable cv;
            bool                done   = false;
            quint8              status = 1;     // default: error
            QByteArray          data;
        };

        QThread      *io_thread;
        QLocalSocket *sock;
        QByteArray    read_buf;   // only accessed on io_thread

        QAtomicInt    next_id{1};
        std::mutex    pending_mu;
        QMap<quint32, PendingCall*> pending;

        static constexpr int kIOTimeoutMs = 30000;

        // Called on io_thread via readyRead
        void onReadyRead() {
            read_buf += sock->readAll();
            processBuffer();
        }

        void wakeAllWithError() {
            std::lock_guard<std::mutex> lock(pending_mu);
            for (auto *call : pending) {
                std::lock_guard<std::mutex> cg(call->mu);
                call->done = true;
                call->cv.notify_one();
            }
            pending.clear();
        }

        void processBuffer() {
            // Parse as many complete response frames as possible.
            // Response header: 4 (reqId) + 1 (status) + 4 (dataLen) = 9 bytes
            while (read_buf.size() >= 9) {
                quint32 reqId, dataLen;
                quint8  status;
                {
                    QDataStream ds(read_buf);
                    ds.setByteOrder(QDataStream::LittleEndian);
                    ds >> reqId >> status >> dataLen;
                }

                int totalSize = 9 + static_cast<int>(dataLen);
                if (read_buf.size() < totalSize) break;  // frame not yet complete

                QByteArray data = read_buf.mid(9, static_cast<int>(dataLen));
                read_buf.remove(0, totalSize);

                std::unique_lock<std::mutex> lock(pending_mu);
                PendingCall *call = pending.value(reqId, nullptr);
                if (call) {
                    pending.remove(reqId);
                    lock.unlock();
                    {
                        std::lock_guard<std::mutex> cg(call->mu);
                        call->status = status;
                        call->data   = data;
                        call->done   = true;
                    }
                    call->cv.notify_one();
                }
            }
        }

    public:
        explicit LocalSocketChannel(QLocalSocket *socket) : sock(socket) {
            io_thread = new QThread;
            sock->setParent(nullptr);
            sock->moveToThread(io_thread);

            // readyRead and disconnected run on io_thread (sock is context)
            QObject::connect(sock, &QLocalSocket::readyRead, sock,
                [this]() { onReadyRead(); });
            QObject::connect(sock, &QLocalSocket::disconnected, sock,
                [this]() { wakeAllWithError(); });

            io_thread->start();
        }

        ~LocalSocketChannel() {
            wakeAllWithError();

            // Close socket and stop event loop on io_thread
            QMetaObject::invokeMethod(sock, [this]() {
                sock->close();
                sock->deleteLater();
                io_thread->quit();
            }, Qt::QueuedConnection);

            io_thread->wait();
            delete io_thread;
        }

        // Returns 0 on success, non-zero on failure.
        int Call(const QString &methodName, const std::string &req,
                 std::vector<uint8_t> &rsp, int timeout_ms = 0) {
            if (!Configs::dataManager->settingsRepo->core_running) return -1919;

            const int ms = (timeout_ms > 0) ? timeout_ms : kIOTimeoutMs;

            quint32 reqId = static_cast<quint32>(next_id.fetchAndAddOrdered(1));

            // Build request frame
            auto methodBytes = methodName.toUtf8();
            auto reqBytes    = QByteArray::fromStdString(req);

            QByteArray frame;
            {
                QDataStream ds(&frame, QIODevice::WriteOnly);
                ds.setByteOrder(QDataStream::LittleEndian);
                ds << reqId;
                ds << static_cast<quint16>(methodBytes.size());
                ds.writeRawData(methodBytes.constData(), methodBytes.size());
                ds << static_cast<quint32>(reqBytes.size());
                ds.writeRawData(reqBytes.constData(), reqBytes.size());
            }

            // Register before sending (never miss the response)
            PendingCall call;
            {
                std::lock_guard<std::mutex> lock(pending_mu);
                pending[reqId] = &call;
            }

            // Dispatch write through io_thread's event loop (safe, FIFO)
            QMetaObject::invokeMethod(sock, [this, frame]() {
                sock->write(frame);
            }, Qt::QueuedConnection);

            // Wait for response
            std::unique_lock<std::mutex> lock(call.mu);
            bool ok = call.cv.wait_for(lock,
                std::chrono::milliseconds(ms),
                [&call] { return call.done; });

            if (!ok) {
                // Timed out — remove from pending if processBuffer hasn't picked it up
                std::lock_guard<std::mutex> plock(pending_mu);
                if (pending.contains(reqId)) {
                    pending.remove(reqId);
                } else {
                    // processBuffer already claimed the pointer; wait for its signal
                    call.cv.wait(lock, [&call] { return call.done; });
                    ok = true;
                }
            }

            if (!ok || call.status != 0) {
                if (ok && call.status != 0)
                    MW_show_log("[Core error] " + QString::fromUtf8(call.data));
                return 1;
            }
            rsp.assign(call.data.begin(), call.data.end());
            return 0;
        }
    };

    // -----------------------------------------------------------------------
    // Client
    // -----------------------------------------------------------------------

    Client::~Client() = default;

    Client::Client(std::function<void(const QString &)> onError, QLocalSocket *socket) {
        this->channel = std::make_unique<LocalSocketChannel>(socket);
        this->onError = std::move(onError);
    }

#define CALL_OK 0

#define NOT_OK      \
    *rpcOK = false; \
    onError(QString("IPC call failed (code %1)\n").arg(status));

    QString Client::Start(bool *rpcOK, const libcore::LoadConfigReq &request) {
        libcore::ErrorResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("Start", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("Stop", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("QueryStats", spb::pb::serialize<std::string>(request), resp, 500);

        if (status == CALL_OK) {
            reply = spb::pb::deserialize<libcore::QueryStatsResp>(resp);
            return reply;
        } else {
            return {};
        }
    }

    libcore::TestResp Client::Test(bool *rpcOK, const libcore::TestReq &request) {
        libcore::TestResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("Test", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("StopTest", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("QueryURLTest", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("IPTest", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("QueryIPTest", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("SetSystemDNS", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
            *rpcOK = true;
            return "";
        } else {
            NOT_OK
            return "IPC error";
        }
    }

    libcore::ListConnectionsResp Client::ListConnections() const
    {
        libcore::EmptyReq request;
        libcore::ListConnectionsResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("ListConnections", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
            reply = spb::pb::deserialize<libcore::ListConnectionsResp>(resp);
            return reply;
        } else {
            MW_show_log("Failed to list connections: IPC error");
            return {};
        }
    }

    QString Client::CheckConfig(bool* rpcOK, const QString& config) const
    {
        libcore::LoadConfigReq request{.core_config = config.toStdString()};
        libcore::ErrorResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("CheckConfig", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK)
        {
            reply = spb::pb::deserialize<libcore::ErrorResp>(resp);
            *rpcOK = true;
            return QString::fromStdString(reply.error.value());
        } else
        {
            NOT_OK
            return "IPC error";
        }
    }

    bool Client::IsPrivileged(bool* rpcOK) const
    {
        libcore::EmptyReq request;
        libcore::IsPrivilegedResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("IsPrivileged", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK)
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
        auto status = channel->Call("SpeedTest", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("QuerySpeedTest", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("QueryCountryTest", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
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
        auto status = channel->Call("GenWgKeyPair", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK) {
            auto reply = spb::pb::deserialize<libcore::GenWgKeyPairResponse>(resp);
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

} // namespace API
