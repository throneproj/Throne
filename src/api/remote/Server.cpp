#include "include/api/remote/Server.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QMutexLocker>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QtEndian>

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>

#include "include/global/Utils.hpp"
#include "include/sys/KillSwitch.hpp"

namespace RemoteApi {
    class Server::Worker : public QObject {
    public:
        explicit Worker(Server *server);

        ~Worker() override;

        ApplyResult apply(const Config &config);

        // Stops listening and gives replies still in the write buffers up to kDrainMs in total to leave.
        void drain();

    private:
        struct Mailbox {
            QMutex mutex;
            Worker *worker = nullptr;
        };

        // Shared by every copy of one Responder; a Responder dropped unanswered answers 500.
        struct Exchange {
            std::shared_ptr<Mailbox> mailbox;
            quint64 connectionId = 0;
            quint64 number = 0;
            std::atomic_bool answered{false};

            ~Exchange();

            void answer(const Response &response);
        };

        enum class Phase { Hello, Ready, Pending };

        struct Connection : QObject {
            using QObject::QObject;

            quint64 id = 0;
            QHostAddress address;
            QString peer;
            QTcpSocket *socket = nullptr;
            QTimer *timer = nullptr;
            Phase phase = Phase::Hello;
            bool closed = false;
            QByteArray serverRandom;
            SessionKeys keys;
            quint64 received = 0;
            quint64 sent = 0;
            quint64 exchanges = 0;
            QByteArray header;
            quint32 length = 0;
            QByteArray body;
        };

        struct FailureRecord {
            QList<qint64> recent;
            qint64 refusedUntil = 0;
        };

        static constexpr int kMaxConnections = 32;
        static constexpr int kMaxConnectionsPerPeer = 8;
        static constexpr int kHelloTimeoutMs = 10 * 1000;
        static constexpr int kIdleTimeoutMs = 60 * 1000;
        static constexpr int kFailureLimit = 5;
        static constexpr qint64 kFailureWindowMs = 60 * 1000;
        static constexpr qint64 kRefusalMs = 60 * 1000;
        // A peer that proved the key this recently is not refused: its failures come from a stale client sharing the address.
        static constexpr qint64 kAuthExemptMs = 10 * 60 * 1000;
        static constexpr qint64 kPeerLogIntervalMs = 60 * 1000;
        static constexpr int kMaxTrackedPeers = 1024;
        static constexpr qint64 kReadBufferSize = 64 * 1024;
        static constexpr qint64 kDrainMs = 1000;

        static void Log(const QString &line);

        static QHostAddress NormalizedPeer(const QHostAddress &address);

        void acceptPending();

        void admit(QTcpSocket *socket);

        bool isLocal(const QHostAddress &address) const;

        bool isAllowed(const QHostAddress &address) const;

        bool isRefused(const QString &peer) const;

        void noteAuthenticated(const QString &peer);

        void processInput(Connection *conn);

        void handleRequest(Connection *conn, const QByteArray &plaintext);

        void deliver(quint64 connectionId, quint64 number, const Response &response);

        void sendResponse(Connection *conn, const Response &response);

        void fail(Connection *conn, const QString &reason);

        void closeConnection(Connection *conn);

        void closeAll();

        void stopListening();

        void logPeer(const QString &peer, const QString &line);

        ApplyResult finish(const ApplyResult &result);

        Server *server_;
        QTcpServer *listener_;
        std::shared_ptr<Mailbox> mailbox_;
        Config config_;
        ApplyResult result_;
        // The specific address the listener is bound to; null for loopback, every address or the settings' listener.
        QHostAddress localAddress_;
        bool listening_ = false;
        QHash<quint64, Connection *> connections_;
        QHash<QString, int> perPeer_;
        QHash<QString, FailureRecord> failures_;
        QHash<QString, qint64> authenticated_;
        QHash<QString, qint64> peerLogged_;
        QElapsedTimer clock_;
        quint64 nextId_ = 1;
    };

    Server::Worker::Exchange::~Exchange() {
        if (!answered) answer(Response::Error(500, QStringLiteral("internal"), Server::tr("The request was dropped without an answer")));
    }

    void Server::Worker::Exchange::answer(const Response &response) {
        if (answered.exchange(true)) return;
        // Held while posting, so the worker cannot be destroyed between the check and the post.
        QMutexLocker lock(&mailbox->mutex);
        auto *worker = mailbox->worker;
        if (worker == nullptr) return;
        QMetaObject::invokeMethod(worker, [worker, id = connectionId, n = number, response] { worker->deliver(id, n, response); }, Qt::QueuedConnection);
    }

    Server::Worker::Worker(Server *server)
        : server_(server), listener_(new QTcpServer(this)), mailbox_(std::make_shared<Mailbox>()) {
        mailbox_->worker = this;
        clock_.start();
        connect(listener_, &QTcpServer::newConnection, this, [this] { acceptPending(); });
    }

    Server::Worker::~Worker() {
        QMutexLocker lock(&mailbox_->mutex);
        mailbox_->worker = nullptr;
    }

    void Server::Worker::Log(const QString &line) {
        MW_show_log(QStringLiteral("[API] ") + line);
    }

    // A dual-stack listener reports IPv4 peers as IPv4-mapped IPv6 addresses.
    QHostAddress Server::Worker::NormalizedPeer(const QHostAddress &address) {
        bool isIPv4 = false;
        const auto ipv4 = address.toIPv4Address(&isIPv4);
        return isIPv4 ? QHostAddress(ipv4) : address;
    }

    ApplyResult Server::Worker::apply(const Config &config) {
        const bool keyValid = config.key.size() == kKeySize;
        const bool sameListener = listening_ && config.enabled && keyValid && config.port == config_.port
                                  && config.lan == config_.lan && config.address == config_.address && config.key == config_.key;
        const bool allowChanged = config.allow != config_.allow;
        config_ = config;

        if (sameListener) {
            if (allowChanged) {
                const auto open = connections_.values();
                for (auto *conn : open) {
                    if (!isAllowed(conn->address)) closeConnection(conn);
                }
            }
            return result_;
        }

        closeAll();
        stopListening();
        ApplyResult result;
        result.port = config.port;
        if (!config.enabled) {
            result.text = Server::tr("Off");
            return finish(result);
        }
        if (!keyValid) {
            result.state = ApplyResult::State::KeyInvalid;
            result.text = Server::tr("Not listening: the key is missing or invalid");
            return finish(result);
        }

        const bool explicitAddress = !config.address.isNull();
        if (explicitAddress) result.address = config.address;
        else result.address = config.lan ? QHostAddress(QHostAddress::Any) : QHostAddress(QHostAddress::LocalHost);
        if (!listener_->listen(result.address, config.port)) {
            result.state = ApplyResult::State::BindFailed;
            result.error = listener_->serverError();
            result.text = result.error == QAbstractSocket::AddressInUseError
                              ? Server::tr("Port %1 is in use").arg(config.port)
                              : Server::tr("Cannot listen on port %1: %2").arg(config.port).arg(listener_->errorString());
            Log(result.text);
            Sys::KillSwitch::SetApiPort(0);
            return finish(result);
        }
        listening_ = true;
        // Loopback replies already pass the guard, and its port-only exemption would also cover other programs on this port.
        Sys::KillSwitch::SetApiPort(result.address.isLoopback() ? quint16(0) : listener_->serverPort());
        result.state = ApplyResult::State::Listening;
        const bool everyAddress = result.address == QHostAddress::Any || result.address == QHostAddress::AnyIPv4
                                  || result.address == QHostAddress::AnyIPv6;
        if (explicitAddress && !everyAddress) localAddress_ = NormalizedPeer(result.address);
        if (explicitAddress ? everyAddress : config.lan) {
            result.text = Server::tr("Listening on port %1 (local network)").arg(config.port);
        } else {
            const auto host = !explicitAddress ? QStringLiteral("127.0.0.1")
                              : result.address.protocol() == QAbstractSocket::IPv6Protocol
                                  ? QStringLiteral("[%1]").arg(result.address.toString())
                                  : result.address.toString();
            result.text = Server::tr("Listening on %1:%2").arg(host).arg(config.port);
        }
        Log(result.text);
        return finish(result);
    }

    ApplyResult Server::Worker::finish(const ApplyResult &result) {
        result_ = result;
        server_->publishResult(result);
        return result;
    }

    void Server::Worker::drain() {
        stopListening();
        QElapsedTimer elapsed;
        elapsed.start();
        const auto open = connections_.values();
        for (auto *conn : open) {
            while (!conn->closed && conn->socket->bytesToWrite() > 0 && elapsed.elapsed() < kDrainMs) {
                if (!conn->socket->waitForBytesWritten(static_cast<int>(kDrainMs - elapsed.elapsed()))) break;
            }
        }
        closeAll();
    }

    void Server::Worker::stopListening() {
        if (!listening_) return;
        listener_->close();
        listening_ = false;
        localAddress_.clear();
        Sys::KillSwitch::SetApiPort(0);
        Log(Server::tr("Stopped listening"));
    }

    void Server::Worker::acceptPending() {
        while (auto *socket = listener_->nextPendingConnection()) admit(socket);
    }

    void Server::Worker::admit(QTcpSocket *socket) {
        const auto address = NormalizedPeer(socket->peerAddress());
        const auto peer = address.toString();
        QString refusal;
        const bool local = isLocal(address);
        if (!local && isRefused(peer)) {
            refusal = Server::tr("Refused %1: too many failed attempts");
        } else if (!isAllowed(address)) {
            refusal = Server::tr("Refused %1: not in the allowed addresses");
        } else if (connections_.size() >= kMaxConnections
                   || (!local && perPeer_.value(peer) >= kMaxConnectionsPerPeer)) {
            refusal = Server::tr("Refused %1: too many connections");
        }
        if (!refusal.isEmpty()) {
            logPeer(peer, refusal.arg(peer));
            socket->abort();
            socket->deleteLater();
            return;
        }

        auto *conn = new Connection(this);
        conn->id = nextId_++;
        conn->address = address;
        conn->peer = peer;
        conn->socket = socket;
        conn->timer = new QTimer(conn);
        conn->timer->setSingleShot(true);
        socket->setParent(conn);
        // A bounded buffer stops reading from the OS, so a client that sends ahead of its responses is throttled by TCP.
        socket->setReadBufferSize(kReadBufferSize);
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        connections_.insert(conn->id, conn);
        ++perPeer_[peer];

        connect(socket, &QTcpSocket::readyRead, conn, [this, conn] { processInput(conn); });
        connect(socket, &QTcpSocket::bytesWritten, conn, [this, conn] {
            if (conn->socket->bytesToWrite() == 0) processInput(conn);
        });
        connect(socket, &QTcpSocket::disconnected, conn, [this, conn] { closeConnection(conn); });
        connect(socket, &QTcpSocket::errorOccurred, conn, [this, conn] { closeConnection(conn); });
        connect(conn->timer, &QTimer::timeout, conn, [this, conn] { closeConnection(conn); });

        conn->serverRandom = RandomBytes(kRandomSize);
        socket->write(conn->serverRandom);
        conn->timer->start(kHelloTimeoutMs);
        processInput(conn);
    }

    // Outside packets with a local source address are dropped, and a blind spoofer cannot finish the handshake.
    bool Server::Worker::isLocal(const QHostAddress &address) const {
        return address.isLoopback() || (!localAddress_.isNull() && address == localAddress_);
    }

    bool Server::Worker::isAllowed(const QHostAddress &address) const {
        if (isLocal(address)) return true;
        if (!config_.lan) return false;
        if (config_.allow.isEmpty()) return true;
        return std::any_of(config_.allow.cbegin(), config_.allow.cend(), [&address](const QPair<QHostAddress, int> &subnet) {
            return address.isInSubnet(subnet.first, subnet.second);
        });
    }

    bool Server::Worker::isRefused(const QString &peer) const {
        const auto it = failures_.constFind(peer);
        return it != failures_.cend() && it->refusedUntil > clock_.elapsed();
    }

    void Server::Worker::processInput(Connection *conn) {
        auto *socket = conn->socket;
        while (!conn->closed) {
            if (conn->phase == Phase::Hello) {
                if (socket->bytesAvailable() < kClientHelloSize) return;
                const auto hello = socket->read(kClientHelloSize);
                if (hello.at(0) != kProtocolVersion) {
                    fail(conn, Server::tr("unsupported protocol version"));
                    return;
                }
                conn->keys = DeriveKeys(config_.key, conn->serverRandom, hello.mid(1));
                conn->phase = Phase::Ready;
                conn->timer->start(kIdleTimeoutMs);
                continue;
            }

            // A request waits until the previous response has left the write buffer.
            if (conn->phase != Phase::Ready || socket->bytesToWrite() > 0) return;
            if (conn->header.isEmpty()) {
                if (socket->bytesAvailable() < kFrameHeaderSize) return;
                conn->header = socket->read(kFrameHeaderSize);
                conn->length = qFromBigEndian<quint32>(conn->header.constData());
                if (conn->length < quint32(kTagSize) || conn->length > kMaxClientFrame) {
                    fail(conn, Server::tr("invalid frame length"));
                    return;
                }
            }
            conn->body += socket->read(qint64(conn->length) - conn->body.size());
            if (conn->body.size() < qsizetype(conn->length)) return;

            if (conn->received == std::numeric_limits<quint64>::max()) {
                closeConnection(conn);
                return;
            }
            QByteArray plaintext;
            const bool opened = OpenFrame(conn->keys.c2s, conn->received, conn->header, conn->body, &plaintext);
            conn->header.clear();
            conn->body.clear();
            if (!opened) {
                fail(conn, Server::tr("a frame failed authentication, most likely the key is wrong"));
                return;
            }
            ++conn->received;
            if (!isLocal(conn->address)) noteAuthenticated(conn->peer);
            handleRequest(conn, plaintext);
        }
    }

    void Server::Worker::handleRequest(Connection *conn, const QByteArray &plaintext) {
        Request request;
        QString error;
        if (!ParseRequest(plaintext, &request, &error)) {
            sendResponse(conn, Response::Error(400, QStringLiteral("bad_request"), error));
            return;
        }
        request.peer = conn->peer;
        request.admin = isLocal(conn->address) || config_.adminLan;
        conn->phase = Phase::Pending;
        conn->timer->stop();
        auto exchange = std::make_shared<Exchange>();
        exchange->mailbox = mailbox_;
        exchange->connectionId = conn->id;
        exchange->number = ++conn->exchanges;
        const Responder respond = [exchange](const Response &response) { exchange->answer(response); };
        auto *server = server_;
        QMetaObject::invokeMethod(server, [server, request, respond] { server->dispatch(request, respond); }, Qt::QueuedConnection);
    }

    void Server::Worker::deliver(quint64 connectionId, quint64 number, const Response &response) {
        auto *conn = connections_.value(connectionId);
        if (conn == nullptr || conn->phase != Phase::Pending || conn->exchanges != number) return;
        sendResponse(conn, response);
        processInput(conn);
    }

    void Server::Worker::sendResponse(Connection *conn, const Response &response) {
        auto plaintext = SerializeResponse(response);
        if (plaintext.size() > qsizetype(kMaxServerFrame) - kTagSize) {
            plaintext = SerializeResponse(Response::Error(500, QStringLiteral("internal"), Server::tr("The response is too large")));
        }
        if (conn->sent == std::numeric_limits<quint64>::max()) {
            closeConnection(conn);
            return;
        }
        conn->socket->write(SealFrame(conn->keys.s2c, conn->sent++, plaintext));
        conn->phase = Phase::Ready;
        conn->timer->start(kIdleTimeoutMs);
    }

    void Server::Worker::fail(Connection *conn, const QString &reason) {
        logPeer(conn->peer, Server::tr("Closed the connection from %1: %2").arg(conn->peer, reason));
        if (!isLocal(conn->address)) {
            const auto now = clock_.elapsed();
            if (failures_.size() >= kMaxTrackedPeers) {
                failures_.removeIf([now](QHash<QString, FailureRecord>::iterator it) {
                    return it->refusedUntil <= now && (it->recent.isEmpty() || now - it->recent.constLast() >= kFailureWindowMs);
                });
            }
            auto &record = failures_[conn->peer];
            record.recent.removeIf([now](qint64 at) { return now - at >= kFailureWindowMs; });
            record.recent.append(now);
            if (record.recent.size() >= kFailureLimit) {
                record.recent.clear();
                const auto proven = authenticated_.constFind(conn->peer);
                if (proven == authenticated_.cend() || now - proven.value() >= kAuthExemptMs) {
                    record.refusedUntil = now + kRefusalMs;
                    Log(QStringLiteral("Refusing every connection from %1 for %2 s after %3 failed attempts; a client there probably "
                                       "still uses an old key")
                            .arg(conn->peer, QString::number(kRefusalMs / 1000), QString::number(kFailureLimit)));
                }
            }
        }
        closeConnection(conn);
    }

    void Server::Worker::noteAuthenticated(const QString &peer) {
        const auto now = clock_.elapsed();
        if (authenticated_.size() >= kMaxTrackedPeers) {
            authenticated_.removeIf([now](QHash<QString, qint64>::iterator it) { return now - it.value() >= kAuthExemptMs; });
        }
        authenticated_.insert(peer, now);
    }

    void Server::Worker::closeConnection(Connection *conn) {
        if (conn->closed) return;
        conn->closed = true;
        conn->timer->stop();
        connections_.remove(conn->id);
        if (--perPeer_[conn->peer] <= 0) perPeer_.remove(conn->peer);
        QObject::disconnect(conn->socket, nullptr, conn, nullptr);
        conn->socket->abort();
        conn->deleteLater();
    }

    void Server::Worker::closeAll() {
        const auto open = connections_.values();
        for (auto *conn : open) closeConnection(conn);
    }

    void Server::Worker::logPeer(const QString &peer, const QString &line) {
        const auto now = clock_.elapsed();
        if (const auto it = peerLogged_.constFind(peer); it != peerLogged_.cend() && now - it.value() < kPeerLogIntervalMs) return;
        if (peerLogged_.size() >= kMaxTrackedPeers) {
            peerLogged_.removeIf([now](QHash<QString, qint64>::iterator it) { return now - it.value() >= kPeerLogIntervalMs; });
        }
        peerLogged_.insert(peer, now);
        Log(line);
    }

    Server *Server::instance() {
        static auto *server = new Server();
        return server;
    }

    Server::Server() : QObject(QCoreApplication::instance()), status_(tr("Off")) {
        lastResult_.text = status_;
        if (auto *app = QCoreApplication::instance(); app != nullptr) connect(app, &QCoreApplication::aboutToQuit, this, &Server::shutdown);
    }

    Server::~Server() {
        shutdown();
    }

    void Server::setDispatcher(Dispatcher dispatcher) {
        dispatcher_ = std::move(dispatcher);
    }

    bool Server::prepareWorker(const Config &config) {
        if (stopped_) return false;
        if (worker_ != nullptr) return true;
        if (!config.enabled) {
            ApplyResult off;
            off.port = config.port;
            off.text = tr("Off");
            publishResult(off);
            return false;
        }
        thread_ = new QThread(this);
        thread_->setObjectName(QStringLiteral("RemoteApi"));
        worker_ = new Worker(this);
        worker_->moveToThread(thread_);
        // QThread runs deferred deletes as it finishes, so the worker and its sockets die on their own thread.
        connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
        thread_->start();
        return true;
    }

    void Server::apply(const Config &config) {
        if (!prepareWorker(config)) return;
        QMetaObject::invokeMethod(worker_, [worker = worker_, config] { worker->apply(config); }, Qt::QueuedConnection);
    }

    ApplyResult Server::applyAndWait(const Config &config) {
        if (!prepareWorker(config)) return lastResult();
        ApplyResult result;
        // Applies queued before this one run first; the worker never waits on this thread.
        QMetaObject::invokeMethod(worker_, [worker = worker_, config, &result] { result = worker->apply(config); },
                                  Qt::BlockingQueuedConnection);
        return result;
    }

    ApplyResult Server::lastResult() const {
        QMutexLocker lock(&statusMutex_);
        return lastResult_;
    }

    void Server::shutdown() {
        if (stopped_) return;
        stopped_ = true;
        if (thread_ != nullptr) {
            // A reply posted before this call is written first, since the worker handles its queue in order.
            if (thread_->isRunning()) QMetaObject::invokeMethod(worker_, [worker = worker_] { worker->drain(); }, Qt::BlockingQueuedConnection);
            thread_->quit();
            thread_->wait();
            worker_ = nullptr;
        }
        ApplyResult off;
        off.text = tr("Off");
        publishResult(off);
    }

    QString Server::statusText() const {
        QMutexLocker lock(&statusMutex_);
        return status_;
    }

    void Server::publishResult(const ApplyResult &result) {
        {
            QMutexLocker lock(&statusMutex_);
            lastResult_ = result;
        }
        publishStatus(result.text);
    }

    void Server::publishStatus(const QString &text) {
        {
            QMutexLocker lock(&statusMutex_);
            if (status_ == text) return;
            status_ = text;
        }
        QMetaObject::invokeMethod(this, [this, text] { emit statusChanged(text); }, Qt::QueuedConnection);
    }

    void Server::dispatch(const Request &request, const Responder &respond) {
        if (stopped_ || !dispatcher_) {
            respond(Response::Error(503, QStringLiteral("exiting"), tr("Throne is shutting down")));
            return;
        }
        dispatcher_(request, respond);
    }
}
