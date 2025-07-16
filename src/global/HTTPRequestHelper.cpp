#include "include/global/HTTPRequestHelper.hpp"

#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QFile>
#include <QApplication>

#include "include/global/Configs.hpp"
#include "include/ui/mainwindow.h"

namespace Configs_network {

    HTTPResponse NetworkRequestHelper::HttpGet(const QString &url) {
        QNetworkRequest request;
        QNetworkAccessManager accessManager;
        request.setUrl(url);
        if (Configs::dataStore->sub_use_proxy || Configs::dataStore->spmode_system_proxy) {
            QNetworkProxy p;
            p.setType(QNetworkProxy::HttpProxy);
            p.setHostName("127.0.0.1");
            p.setPort(Configs::dataStore->inbound_socks_port);
            accessManager.setProxy(p);
            if (Configs::dataStore->started_id < 0) {
                return HTTPResponse{QObject::tr("Request with proxy but no profile started.")};
            }
        }
        // Set attribute
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::KnownHeaders::UserAgentHeader, Configs::dataStore->GetUserAgent());
        if (Configs::dataStore->sub_insecure) {
            QSslConfiguration c;
            c.setPeerVerifyMode(QSslSocket::PeerVerifyMode::VerifyNone);
            request.setSslConfiguration(c);
        }
        //
        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::sslErrors, _reply, [](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QString("SSL Errors: %1 %2").arg(error_str.join(","), Configs::dataStore->sub_insecure ? "(Ignored)" : ""));
        });
        // Wait for response
        auto abortTimer = new QTimer;
        abortTimer->setSingleShot(true);
        abortTimer->setInterval(10000);
        connect(abortTimer, &QTimer::timeout, _reply, &QNetworkReply::abort);
        abortTimer->start();
        {
            QEventLoop loop;
            connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
        }
        if (abortTimer != nullptr) {
            abortTimer->stop();
            abortTimer->deleteLater();
        }
        //
        auto result = HTTPResponse{_reply->error() == QNetworkReply::NetworkError::NoError ? "" : _reply->errorString(),
                                       _reply->readAll(), _reply->rawHeaderPairs()};
        _reply->deleteLater();
        return result;
    }

    QString NetworkRequestHelper::GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name) {
        for (const auto &p: header) {
            if (QString(p.first).toLower() == name.toLower()) return p.second;
        }
        return "";
    }

    QString NetworkRequestHelper::DownloadAsset(const QString &url, const QString &fileName) {
        QNetworkRequest request;
        QNetworkAccessManager accessManager;
        request.setUrl(url);
        if (Configs::dataStore->spmode_system_proxy) {
            QNetworkProxy p;
            p.setType(QNetworkProxy::HttpProxy);
            p.setHostName("127.0.0.1");
            p.setPort(Configs::dataStore->inbound_socks_port);
            accessManager.setProxy(p);
            if (Configs::dataStore->started_id < 0) {
                return QObject::tr("Request with proxy but no profile started.");
            }
        }

        auto _reply = accessManager.get(request);
        connect(_reply, &QNetworkReply::sslErrors, _reply, [](const QList<QSslError> &errors) {
            QStringList error_str;
            for (const auto &err: errors) {
                error_str << err.errorString();
            }
            MW_show_log(QString("SSL Errors: %1").arg(error_str.join(",")));
        });
        connect(_reply, &QNetworkReply::downloadProgress, _reply, [&](qint64 bytesReceived, qint64 bytesTotal)
        {
            runOnUiThread([=]{
                GetMainWindow()->setDownloadReport(DownloadProgressReport{fileName, bytesReceived, bytesTotal}, true);
                GetMainWindow()->UpdateDataView();
            });
        });
        QEventLoop loop;
        connect(_reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        runOnUiThread([=]
        {
            GetMainWindow()->setDownloadReport({}, false);
            GetMainWindow()->UpdateDataView(true);
        });
        if(_reply->error() != QNetworkReply::NetworkError::NoError) {
            return _reply->errorString();
        }

        auto filePath = Configs::GetBasePath()+ "/" + fileName;
        auto file = QFile(filePath);
        if (file.exists()) {
            file.remove();
        }
        if (!file.open(QIODevice::WriteOnly)) {
            return QObject::tr("Could not open file.");
        }
        file.write(_reply->readAll());
        file.close();
        return "";
    }

} // namespace Configs_network
