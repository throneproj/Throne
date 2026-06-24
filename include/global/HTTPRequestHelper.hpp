#pragma once

#include <QObject>
#include <functional>

namespace Configs_network {
    struct HTTPResponse {
        QString error;
        QByteArray data;
        QList<QPair<QByteArray, QByteArray>> header;
    };

    struct DownloadProgressReport
    {
        QString fileName;
        qint64 downloadedSize;
        qint64 totalSize;
    };

    class NetworkRequestHelper : QObject {
        Q_OBJECT

        explicit NetworkRequestHelper(QObject *parent) : QObject(parent){};

        ~NetworkRequestHelper() override = default;
        ;

    public:
        // forceSecure overrides the global net_insecure setting and always
        // enforces TLS certificate verification. Used for trust-critical
        // channels (e.g. the auto-update download) that must never be served
        // over an unverified connection.
        static HTTPResponse HttpGet(const QString &url, bool sendHwid = false, bool useProxy = false, bool forceSecure = false);

        static QString GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name);

        static QString DownloadAsset(const QString &url, const QString &fileName, bool forceSecure = false);
    };
} // namespace Configs_network

using namespace Configs_network;
