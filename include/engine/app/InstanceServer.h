#pragma once

#include <functional>

#include <QLocalServer>
#include <QString>
#include <QStringList>

class QLocalSocket;

namespace Engine {
    // The single-instance socket: a later launch hands over its payload, one url per line.
    class InstanceServer {
    public:
        using LineHandler = std::function<void(QLocalSocket *socket, const QString &line)>;
        using ConnectHandler = std::function<void(QLocalSocket *socket)>;

        // Declare it after the app object: the server's parent is the app.
        InstanceServer(const QString &name, QLocalServer::SocketOptions options);

        InstanceServer(const InstanceServer &) = delete;
        InstanceServer &operator=(const InstanceServer &) = delete;

        bool listen();

        void close();

        [[nodiscard]] QString errorString() const;

        [[nodiscard]] const QString &name() const { return m_name; }

        // onConnected runs after the bytes that already arrived were handed to onLine.
        void setHandlers(LineHandler onLine, ConnectHandler onConnected);

        // aboutToQuit: stop listening, remove the socket, Logging::Shutdown. Connect it before anything else hooks aboutToQuit.
        void installQuitCleanup();

        // Client side; false when nothing answered within 250 ms.
        static bool HandOff(const QString &serverName, const QStringList &payload);

    private:
        QString m_name;
        QLocalServer m_server;
    };
} // namespace Engine
