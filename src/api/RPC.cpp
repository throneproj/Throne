#include "include/api/RPC.h"
#include <utility>

#include "include/global/Configs.hpp"

#include <QLocalSocket>
#include <QDataStream>
#include <QAtomicInt>
#include <QMap>
#include <QMutex>
#include <QWaitCondition>
#include <QDeadlineTimer>
#include <QObject>
#include <QThread>

#include <climits>
#include <mutex>
#include <atomic>
#include <exception>
#include <memory>

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
    // The channel is created once and lives for the whole app. On core
    // restart the *connection* is replaced via Reconnect(): the old socket
    // is torn down and the new one adopted, all on the io_thread, while the
    // channel object (and therefore `defaultClient`) stays stable so that
    // worker threads calling into it never touch freed memory.
    //
    // `sock` and `read_buf` are io_thread-only state. All socket operations
    // are dispatched as queued lambdas to `io_anchor`, a stable QObject that
    // lives on io_thread for the channel's entire lifetime — callers never
    // dereference `sock` directly, so swapping it cannot race with a write.
    //
    // PendingCall is owned through a shared_ptr held jointly by Call() and
    // the `pending` map, so a timed-out / spuriously-woken Call() can never
    // destroy it while the io_thread is still writing to it or notifying.
    // -----------------------------------------------------------------------
    class Client::LocalSocketChannel {

        struct PendingCall {
            QMutex              mu;
            QWaitCondition      cv;
            bool                done   = false;
            quint8              status = 1;     // default: error
            QByteArray          data;
        };

        // QWaitCondition has no predicate+timeout overload (and unlike
        // std::condition_variable it doesn't pull GetSystemTimePreciseAsFileTime
        // into the import table, which breaks Win7). Block until the call
        // completes or `ms` elapses, looping past spurious wakeups. `call.mu`
        // must be held on entry and is still held on return.
        static bool awaitDone(PendingCall &call, int ms) {
            QDeadlineTimer deadline(ms);
            while (!call.done) {
                qint64 remaining = deadline.remainingTime();
                if (remaining == 0) break;                  // timed out
                call.cv.wait(&call.mu, remaining < 0
                    ? ULONG_MAX
                    : static_cast<unsigned long>(remaining));
            }
            return call.done;
        }

        QThread      *io_thread;
        QObject      *io_anchor;          // stable dispatch target on io_thread
        QLocalSocket *sock = nullptr;     // io_thread only
        QByteArray    read_buf;           // io_thread only

        QAtomicInt    next_id{1};         // monotonic across reconnects
        std::mutex    pending_mu;
        QMap<quint32, std::shared_ptr<PendingCall>> pending;

        static constexpr int kIOTimeoutMs = 30000;

        // Called on io_thread via readyRead
        void onReadyRead() {
            if (sock == nullptr) return;
            read_buf += sock->readAll();
            processBuffer();
        }

        void wakeAllWithError() {
            std::lock_guard<std::mutex> lock(pending_mu);
            for (auto &call : pending) {
                QMutexLocker cg(&call->mu);
                call->done = true;
                call->cv.wakeOne();
            }
            pending.clear();
        }

        // io_thread only
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

                qint64 totalSize = qint64(9) + dataLen;
                if (read_buf.size() < totalSize) break;  // frame not yet complete

                QByteArray data = read_buf.mid(9, static_cast<int>(dataLen));
                read_buf.remove(0, totalSize);

                std::shared_ptr<PendingCall> call;
                {
                    std::lock_guard<std::mutex> lock(pending_mu);
                    call = pending.value(reqId, nullptr);
                    if (call) pending.remove(reqId);
                }
                if (call) {
                    // Hold call->mu across the notify so the cv/mutex cannot
                    // be destroyed by a waking Call() mid-notification.
                    QMutexLocker cg(&call->mu);
                    call->status = status;
                    call->data   = std::move(data);
                    call->done   = true;
                    call->cv.wakeOne();
                }
            }
        }

    public:
        LocalSocketChannel() {
            io_thread = new QThread;
            io_anchor = new QObject;
            io_anchor->moveToThread(io_thread);
            io_thread->start();
        }

        ~LocalSocketChannel() {
            wakeAllWithError();

            // Close socket and stop event loop on io_thread
            QMetaObject::invokeMethod(io_anchor, [this]() {
                if (sock) {
                    sock->close();
                    delete sock;
                    sock = nullptr;
                }
                io_thread->quit();
            }, Qt::QueuedConnection);

            io_thread->wait();
            delete io_anchor;   // safe: io_thread has finished
            delete io_thread;
        }

        // Replace the underlying connection. Must be called on the thread that
        // currently owns `newSock` (the UI thread, same as before).
        void Reconnect(QLocalSocket *newSock) {
            // Fail every in-flight call so blocked Call()s return an error.
            wakeAllWithError();

            // Hand the socket over to the io_thread.
            newSock->setParent(nullptr);
            newSock->moveToThread(io_thread);

            // Swap on the io_thread. This is queued before any later write
            // dispatch (FIFO), so a write that arrives after Reconnect always
            // targets the new socket.
            QMetaObject::invokeMethod(io_anchor, [this, newSock]() {
                if (sock) {
                    sock->disconnect(io_anchor);   // drop old readyRead/disconnected
                    sock->close();
                    sock->deleteLater();
                }
                sock = newSock;
                read_buf.clear();
                QObject::connect(sock, &QLocalSocket::readyRead, io_anchor,
                    [this]() { onReadyRead(); });
                QObject::connect(sock, &QLocalSocket::disconnected, io_anchor,
                    [this]() { wakeAllWithError(); });
                if (sock->bytesAvailable() > 0) onReadyRead();
            }, Qt::QueuedConnection);
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
            auto call = std::make_shared<PendingCall>();
            {
                std::lock_guard<std::mutex> lock(pending_mu);
                pending[reqId] = call;
            }

            // Dispatch write through the stable io anchor (FIFO, never
            // touches `sock` on this thread).
            QMetaObject::invokeMethod(io_anchor, [this, frame]() {
                if (sock) sock->write(frame);
            }, Qt::QueuedConnection);

            // Wait for response
            call->mu.lock();
            bool ok = awaitDone(*call, ms);
            call->mu.unlock();   // never hold call->mu while taking pending_mu

            if (!ok) {
                // Timed out — reclaim our slot unless processBuffer took it.
                bool claimedByReader = false;
                {
                    std::lock_guard<std::mutex> plock(pending_mu);
                    if (pending.remove(reqId) == 0) claimedByReader = true;
                }
                if (claimedByReader) {
                    // A response is inbound; give it a brief bounded chance.
                    call->mu.lock();
                    ok = awaitDone(*call, ms);
                    call->mu.unlock();
                }
            }

            QMutexLocker g(&call->mu);
            if (!ok || call->status != 0) {
                if (ok && call->status != 0)
                    MW_show_log("[Core error] " + QString::fromUtf8(call->data));
                return 1;
            }
            rsp.assign(call->data.begin(), call->data.end());
            return 0;
        }
    };

    // -----------------------------------------------------------------------
    // Client
    // -----------------------------------------------------------------------

    namespace {
        // spb throws std::runtime_error on any malformed/torn input. These
        // calls run on worker QThreads, so an uncaught throw would terminate
        // the whole process. Turn a bad frame into a failed RPC instead.
        template <typename T>
        bool tryDeserialize(const std::vector<uint8_t> &resp, T &out) {
            try {
                out = spb::pb::deserialize<T>(resp);
                return true;
            } catch (const std::exception &e) {
                MW_show_log(QString("[RPC] dropped malformed response: ") + e.what());
                return false;
            } catch (...) {
                MW_show_log("[RPC] dropped malformed response");
                return false;
            }
        }
    }

    Client::~Client() = default;

    Client::Client() {
        this->channel = std::make_unique<LocalSocketChannel>();
    }

    void Client::Reconnect(QLocalSocket *socket) {
        channel->Reconnect(socket);
    }

#define CALL_OK 0

#define NOT_OK      \
    *rpcOK = false; \
    MW_show_log(QString("IPC call failed (code %1)\n").arg(status));

    QString Client::Start(bool *rpcOK, const libcore::LoadConfigReq &request) {
        libcore::ErrorResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("Start", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
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

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
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

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
            return reply;
        }
        return {};
    }

    libcore::TestResp Client::Test(bool *rpcOK, const libcore::TestReq &request) {
        libcore::TestResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("Test", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
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

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
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

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
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

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
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

    libcore::QueryConnectionsResp Client::QueryConnections() const
    {
        libcore::EmptyReq request;
        libcore::QueryConnectionsResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("QueryConnections", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
            return reply;
        }
        if (status != CALL_OK) MW_show_log("Failed to query connections: IPC error");
        return {};
    }

    QString Client::CheckConfig(bool* rpcOK, const QString& config, bool isXray) const
    {
        libcore::LoadConfigReq request;
        if (isXray)
        {
            request.need_xray = true;
            request.xray_config = config.toStdString();
        } else
        {
            request.core_config = config.toStdString();
        }
        libcore::ErrorResp reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("CheckConfig", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK && tryDeserialize(resp, reply))
        {
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

        if (status == CALL_OK && tryDeserialize(resp, reply))
        {
            *rpcOK = true;
            return reply.has_privilege.value();
        } else
        {
            NOT_OK
            return false;
        }
    }

    QString Client::GetDefaultInterface(bool* rpcOK) const
    {
        libcore::EmptyReq request;
        libcore::GetDefaultInterfaceResponse reply;
        std::vector<uint8_t> resp;
        // Short timeout: this feeds a synchronous config build, so a hung core
        // must not freeze it. On any failure the caller keeps the loopback bridge.
        auto status = channel->Call("GetDefaultInterface", spb::pb::serialize<std::string>(request), resp, 3000);

        if (status == CALL_OK && tryDeserialize(resp, reply))
        {
            *rpcOK = true;
            return QString::fromStdString(reply.name.value());
        } else
        {
            NOT_OK
            return "";
        }
    }

    libcore::SpeedTestResponse Client::SpeedTest(bool *rpcOK, const libcore::SpeedTestRequest &request)
    {
        libcore::SpeedTestResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("SpeedTest", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
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

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
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

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
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
        libcore::GenWgKeyPairResponse reply;
        std::vector<uint8_t> resp;
        auto status = channel->Call("GenWgKeyPair", spb::pb::serialize<std::string>(request), resp);

        if (status == CALL_OK && tryDeserialize(resp, reply)) {
            *rpcOK = true;
            return reply;
        } else {
            NOT_OK
            return {};
        }
    }

} // namespace API
