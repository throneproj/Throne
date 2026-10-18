#include "include/engine/app/InstanceServer.h"

#include <memory>

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QLocalSocket>

#include "include/global/Logger.hpp"

namespace Engine {
    InstanceServer::InstanceServer(const QString &name, QLocalServer::SocketOptions options)
        : m_name(name), m_server(QCoreApplication::instance()) {
        m_server.setSocketOptions(options);
    }

    bool InstanceServer::listen() {
        return m_server.listen(m_name);
    }

    void InstanceServer::close() {
        m_server.close();
    }

    QString InstanceServer::errorString() const {
        return m_server.errorString();
    }

    void InstanceServer::setHandlers(LineHandler onLine, ConnectHandler onConnected) {
        QObject::connect(&m_server, &QLocalServer::newConnection, QCoreApplication::instance(),
                         [this, onLine = std::move(onLine), onConnected = std::move(onConnected)] {
            auto s = m_server.nextPendingConnection();
            qDebug() << "Another instance tried to wake us up on " << m_name << s;
            // One url per line; the tail carries no trailing newline, so it is only flushed on disconnect.
            auto pending = std::make_shared<QByteArray>();
            auto handleLine = [s, onLine](const QString &line) { onLine(s, line); };
            auto readPayload = [s, pending, handleLine](bool last) {
                pending->append(s->readAll());
                while (true) {
                    const auto at = pending->indexOf('\n');
                    if (at < 0) break;
                    handleLine(QString::fromUtf8(pending->first(at)).trimmed());
                    pending->remove(0, at + 1);
                }
                if (last) {
                    handleLine(QString::fromUtf8(*pending).trimmed());
                    pending->clear();
                }
            };
            QObject::connect(s, &QLocalSocket::readyRead, s, [readPayload] { readPayload(false); });
            QObject::connect(s, &QLocalSocket::disconnected, s, [readPayload] { readPayload(true); });
            QObject::connect(s, &QLocalSocket::disconnected, s, &QLocalSocket::deleteLater);
            readPayload(false); // in case the payload already arrived
            onConnected(s);
        });
    }

    void InstanceServer::installQuitCleanup() {
        QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, &m_server, [this]
        {
            m_server.close();
            QLocalServer::removeServer(m_name);
            // Every quit path lands here; missing it is reported as a crash next start.
            Logging::Shutdown();
        });
    }

    // Framing is one url per line, so paths go over as file:// urls: a newline in a name would break it.
    bool InstanceServer::HandOff(const QString &serverName, const QStringList &payload) {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (!socket.waitForConnected(250)) return false;
        qDebug() << "Another instance answered on" << serverName;
        if (!payload.isEmpty()) {
            socket.write(payload.join('\n').toUtf8());
            socket.flush();
            socket.waitForBytesWritten(250);
        }
        socket.disconnectFromServer();
        return true;
    }
} // namespace Engine
