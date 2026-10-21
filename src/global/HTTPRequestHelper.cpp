#include "include/global/HTTPRequestHelper.hpp"

#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslSocket>
#include <QTimer>
#include <QFile>
#include <QEventLoop>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>



#include "include/global/Configs.hpp"
#include "include/global/LocalNetwork.hpp"

namespace Configs_network {
    namespace {
        constexpr int kDownloadAssetIdleTimeoutMs = 30000;

        QMutex downloadProgressSinkMutex;
        NetworkRequestHelper::DownloadProgressSink downloadProgressSink;

        NetworkRequestHelper::DownloadProgressSink loadDownloadProgressSink() {
            QMutexLocker locker(&downloadProgressSinkMutex);
            return downloadProgressSink;
        }

        // The sink is looked up again on the main thread, so one cleared meanwhile is never called.
        void reportDownloadProgress(const DownloadProgressReport &report, bool show) {
            if (!loadDownloadProgressSink()) return;
            runOnUiThread([report, show] {
                if (const auto sink = loadDownloadProgressSink()) sink(report, show);
            });
        }
    } // namespace

    QNetworkProxy MixedInboundProxy() {
        const auto &settings = Configs::dataManager->settingsRepo;
        QNetworkProxy p;
        p.setType(QNetworkProxy::HttpProxy);
        p.setHostName(LocalNetwork::InboundConnectHost());
        p.setPort(settings->inbound_socks_port);
        if (settings->inbound_auth) {
            p.setUser(settings->inbound_user);
            p.setPassword(settings->inbound_pass);
        }
        return p;
    }

    bool RequestUsesMixedInbound(bool useProxy, QString *error) {
        const auto &settings = Configs::dataManager->settingsRepo;
        if (!settings->net_use_proxy && !settings->spmode_system_proxy && !useProxy) return false;
        if (settings->started_id < 0) {
            *error = QObject::tr("Request with proxy but no profile started.");
            return false;
        }
        if (!settings->disable_mixed_inbound) return true;
        if (!settings->spmode_vpn) *error = QObject::tr("Request with proxy but the mixed inbound is disabled and Tun Mode is off.");
        return false;
    }

    void NetworkRequestHelper::SetDownloadProgressSink(DownloadProgressSink sink) {
        QMutexLocker locker(&downloadProgressSinkMutex);
        downloadProgressSink = std::move(sink);
    }

    HTTPResponse NetworkRequestHelper::HttpGet(const QString &url, bool useProxy) {
        HttpGetOptions options;
        options.useProxy = useProxy;
        return HttpGet(url, options);
    }

    HTTPResponse NetworkRequestHelper::HttpGet(const QString &url, const HttpGetOptions &options) {
        if (!QSslSocket::isProtocolSupported(options.tlsProtocol)) {
            return HTTPResponse{QObject::tr("The selected TLS version is not supported on this system.")};
        }
        const qint64 maxBytes = options.maxBytes;
        QNetworkRequest request;
        QNetworkAccessManager accessManager;
        accessManager.setTransferTimeout(10000);
        request.setUrl(url);
        QString proxyError;
        if (RequestUsesMixedInbound(options.useProxy, &proxyError)) accessManager.setProxy(MixedInboundProxy());
        else if (!proxyError.isEmpty()) return HTTPResponse{proxyError};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, options.http2);
        request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader,
                          options.userAgent.isEmpty() ? Configs::dataManager->settingsRepo->GetUserAgent() : options.userAgent);
        const bool insecure = Configs::dataManager->settingsRepo->net_insecure && !options.strictTls;
        auto ssl = request.sslConfiguration();
        ssl.setProtocol(options.tlsProtocol);
        if (insecure) ssl.setPeerVerifyMode(QSslSocket::PeerVerifyMode::VerifyNone);
        request.setSslConfiguration(ssl);
        for (const auto &[name, value] : options.headers) request.setRawHeader(name, value);
        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::sslErrors, _reply, [insecure](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QString("SSL Errors: %1 %2").arg(error_str.join(","), insecure ? "(Ignored)" : ""));
        });
        QByteArray body;
        bool tooLarge = false;
        connect(_reply, &QNetworkReply::readyRead, _reply, [&] {
            if (body.isEmpty()) {
                const auto expected = _reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
                const qint64 reserveCap = maxBytes > 0 ? maxBytes : 256LL * 1024 * 1024;
                if (expected > 0 && expected <= reserveCap) body.reserve(static_cast<qsizetype>(expected));
            }
            body += _reply->readAll();
            if (maxBytes > 0 && body.size() > maxBytes) {
                tooLarge = true;
                _reply->abort();
            }
        });
        QEventLoop loop;
        connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        body += _reply->readAll();

        HTTPResponse result;
        result.header = _reply->rawHeaderPairs();
        if (tooLarge) {
            result.error = QObject::tr("Response larger than %1 MB").arg(maxBytes / (1024 * 1024));
        } else {
            result.error = _reply->error() == QNetworkReply::NetworkError::NoError ? "" : _reply->errorString();
            result.data = std::move(body);
        }
        _reply->deleteLater();
        return result;
    }

    QString NetworkRequestHelper::GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name) {
        const QByteArray needle = name.toLatin1();
        for (const auto &p: header) {
            if (p.first.compare(needle, Qt::CaseInsensitive) == 0) return p.second;
        }
        return {};
    }

    QString NetworkRequestHelper::DownloadAsset(const QString &url, const QString &fileName, bool useProxy, bool strictTls) {
        QNetworkRequest request;
        QNetworkAccessManager accessManager;
        accessManager.setTransferTimeout(kDownloadAssetIdleTimeoutMs);
        request.setUrl(url);
        QString proxyError;
        if (RequestUsesMixedInbound(useProxy, &proxyError)) accessManager.setProxy(MixedInboundProxy());
        else if (!proxyError.isEmpty()) return proxyError;
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        const bool insecure = Configs::dataManager->settingsRepo->net_insecure && !strictTls;
        if (insecure) {
            QSslConfiguration c;
            c.setPeerVerifyMode(QSslSocket::PeerVerifyMode::VerifyNone);
            request.setSslConfiguration(c);
        }

        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::sslErrors, _reply, [insecure](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QString("SSL Errors: %1 %2").arg(error_str.join(","), insecure ? "(Ignored)" : ""));
        });
        connect(_reply, &QNetworkReply::downloadProgress, _reply, [&](qint64 bytesReceived, qint64 bytesTotal)
        {
            reportDownloadProgress(DownloadProgressReport{fileName, bytesReceived, bytesTotal}, true);
        });
        QEventLoop loop;
        connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        reportDownloadProgress({}, false);
        auto netErr = _reply->error();
        const QString netErrStr = _reply->errorString();
        const int httpStatus = _reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = _reply->readAll();
        _reply->deleteLater();

        // Nothing else aborts this reply, so a cancel is the idle transfer timeout firing.
        if (netErr == QNetworkReply::NetworkError::OperationCanceledError) {
            return QObject::tr("Download stalled: no data arrived for %1 seconds.").arg(kDownloadAssetIdleTimeoutMs / 1000);
        }
        if (netErr != QNetworkReply::NetworkError::NoError) {
            return netErrStr;
        }

        if (httpStatus != 0 && (httpStatus < 200 || httpStatus >= 300)) {
            return QObject::tr("Download failed: server returned HTTP status %1.").arg(httpStatus);
        }
        if (body.isEmpty()) {
            return QObject::tr("Download failed: the server returned an empty response.");
        }

        const auto filePath = Configs::GetBasePath() + "/" + fileName;
        const auto tmpPath = filePath + ".tmp";
        QFile tmp(tmpPath);
        if (!tmp.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return QObject::tr("Could not open file.");
        }
        if (tmp.write(body) != body.size() || !tmp.flush()) {
            tmp.close();
            tmp.remove();
            return QObject::tr("Could not write file.");
        }
        tmp.close();
        QFile::remove(filePath);
        if (!tmp.rename(filePath)) {
            tmp.remove();
            return QObject::tr("Could not save downloaded file.");
        }
        return "";
    }

} // namespace Configs_network
