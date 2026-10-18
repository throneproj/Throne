#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QString>

#include <functional>

namespace RemoteApi {
    struct Request {
        QString method;
        QString path;
        QHash<QString, QString> query;
        // Null when the request has no body.
        QJsonValue body;
        // The client's address, for the log.
        QString peer;
        // May use the management endpoints: the peer is loopback, or management from the LAN is allowed.
        bool admin = false;
    };

    struct Response {
        int status = 200;
        QJsonValue body;
        QString errorCode;
        QString errorMessage;

        static Response Ok(const QJsonValue &body, int status = 200);

        static Response Error(int status, const QString &code, const QString &message);
    };

    // Callable from any thread; only the first call is delivered.
    using Responder = std::function<void(const Response &)>;

    bool ParseRequest(const QByteArray &json, Request *request, QString *error);

    QByteArray SerializeResponse(const Response &response);
}
