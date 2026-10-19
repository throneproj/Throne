#pragma once

#include <QObject>
#include <QSsl>
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

    struct HttpGetOptions {
        bool useProxy = false;
        // > 0 aborts the transfer once the body exceeds it and reports an error.
        qint64 maxBytes = 0;
        // Empty sends the global User-Agent.
        QString userAgent;
        QList<QPair<QByteArray, QByteArray>> headers;
        QSsl::SslProtocol tlsProtocol = QSsl::SecureProtocols;
        bool http2 = true;
        // Verifies the certificate even when net_insecure is on.
        bool strictTls = false;
    };

    // Whether an app request goes through the mixed inbound. With net_use_proxy, the system proxy mode or useProxy it
    // needs a running profile, and without the mixed inbound a running Tun Mode captures the request instead.
    // False with *error set when nothing can proxy it.
    bool RequestUsesMixedInbound(bool useProxy, QString *error);

    class NetworkRequestHelper : QObject {
        Q_OBJECT

        explicit NetworkRequestHelper(QObject *parent) : QObject(parent){};

        ~NetworkRequestHelper() override = default;
        ;

    public:
        static HTTPResponse HttpGet(const QString &url, bool useProxy = false);

        static HTTPResponse HttpGet(const QString &url, const HttpGetOptions &options);

        static QString GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name);

        static QString DownloadAsset(const QString &url, const QString &fileName, bool useProxy = false, bool strictTls = false);

        // Receives DownloadAsset progress on the main thread: show false ends the report. Without a sink nothing is reported.
        using DownloadProgressSink = std::function<void(const DownloadProgressReport &report, bool show)>;

        static void SetDownloadProgressSink(DownloadProgressSink sink);
    };
} // namespace Configs_network

using namespace Configs_network;
