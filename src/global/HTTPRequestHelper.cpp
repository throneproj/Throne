#include "include/global/HTTPRequestHelper.hpp"

#include <QByteArray>
#include <QMetaEnum>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QApplication>
#include "cpr/cpr.h"

#include "include/global/NekoGui.hpp"

namespace NekoGui_network {

    NekoHTTPResponse NetworkRequestHelper::HttpGet(const QString &url) {
        cpr::Session session;
        if (NekoGui::dataStore->sub_use_proxy || NekoGui::dataStore->spmode_system_proxy) {
            session.SetProxies({{"http", "127.0.0.1:" + QString(Int2String(NekoGui::dataStore->inbound_socks_port)).toStdString()},
                                {"https", "127.0.0.1:" + QString(Int2String(NekoGui::dataStore->inbound_socks_port)).toStdString()}});
            if (NekoGui::dataStore->started_id < 0 && NekoGui::dataStore->sub_use_proxy) {
                return NekoHTTPResponse{QObject::tr("Request with proxy but no profile started.")};
            }
        }
        if (NekoGui::dataStore->sub_insecure) {
            session.SetVerifySsl(cpr::VerifySsl{false});
        }
        session.SetUserAgent(cpr::UserAgent{NekoGui::dataStore->GetUserAgent().toStdString()});
        session.SetTimeout(cpr::Timeout(10000));
        session.SetUrl(cpr::Url(url.toStdString()));
        auto resp = session.Get();
        auto headerPairs = QList<QPair<QByteArray, QByteArray>>();
        for (const auto &item: resp.header) {
            headerPairs.append(std::pair<QByteArray, QByteArray>(QByteArray(item.first.c_str()), QByteArray(item.second.c_str())));
        }
        auto err = resp.error.message.empty() ? (resp.status_code == 200 ? "" : resp.status_line) : resp.error.message;
        auto result = NekoHTTPResponse{ err.c_str(),
                                       resp.text.c_str(), headerPairs};
        return result;
    }

    QString NetworkRequestHelper::GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name) {
        for (const auto &p: header) {
            if (QString(p.first).toLower() == name.toLower()) return p.second;
        }
        return "";
    }

    QString NetworkRequestHelper::DownloadAsset(const QString &url, const QString &fileName, bool isTemp) {
        cpr::Session session;
        session.SetUrl(cpr::Url{url.toStdString()});
        if (NekoGui::dataStore->spmode_system_proxy) {
            session.SetProxies({{"http", "127.0.0.1:" + QString(Int2String(NekoGui::dataStore->inbound_socks_port)).toStdString()},
                                {"https", "127.0.0.1:" + QString(Int2String(NekoGui::dataStore->inbound_socks_port)).toStdString()}});
        }
        auto filePath = NekoGui::GetBasePath()+ "/" + fileName;
        auto tempFilePath = QString(filePath);
        if(isTemp) tempFilePath += ".1";
        QFile::remove(tempFilePath);

        std::ofstream fout;
        fout.open(tempFilePath.toStdString(), std::ios::trunc | std::ios::out | std::ios::binary);
        auto r = session.Download(fout);
        fout.close();

        auto tmpFile = QFile(tempFilePath);
        if (r.status_code != 200) {
            tmpFile.remove();
            if (r.status_code == 0) {
                return "Please check the URL and your network Connectivity";
            }
            return r.status_line.c_str();
        }
        if(isTemp) {
            QFile(filePath).remove();
            if (!tmpFile.rename(filePath)) {
                tmpFile.remove();
                return tmpFile.errorString();
            }
        }
        return "";
    }

} // namespace NekoGui_network
