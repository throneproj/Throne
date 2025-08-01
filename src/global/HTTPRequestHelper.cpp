#include "include/global/HTTPRequestHelper.hpp"

#include <QByteArray>
#include <QFile>
#include "cpr/cpr.h"

#include "include/global/Configs.hpp"
#include "include/ui/mainwindow.h"

namespace Configs_network {

    HTTPResponse NetworkRequestHelper::HttpGet(const QString &url) {
        cpr::Session session;
        if (Configs::dataStore->sub_use_proxy || Configs::dataStore->spmode_system_proxy) {
            if (Configs::dataStore->started_id < 0) {
                return HTTPResponse{QObject::tr("Request with proxy but no profile started.")};
            }
            session.SetProxies({{"http", "127.0.0.1:" + QString(Int2String(Configs::dataStore->inbound_socks_port)).toStdString()},
                                {"https", "127.0.0.1:" + QString(Int2String(Configs::dataStore->inbound_socks_port)).toStdString()}});
        }
        if (Configs::dataStore->sub_insecure) {
            session.SetVerifySsl(cpr::VerifySsl{false});
        }
        session.SetUserAgent(cpr::UserAgent{Configs::dataStore->GetUserAgent().toStdString()});
        session.SetTimeout(cpr::Timeout(8000));
        session.SetUrl(cpr::Url(url.toStdString()));
        session.SetSslOptions(cpr::Ssl(cpr::ssl::NoRevoke{true}));
        auto resp = session.Get();
        auto headerPairs = QList<QPair<QByteArray, QByteArray>>();
        for (const auto &item: resp.header) {
            headerPairs.append(std::pair<QByteArray, QByteArray>(QByteArray(item.first.c_str()), QByteArray(item.second.c_str())));
        }
        auto err = resp.error.message.empty() ? (resp.status_code == 200 ? "" : resp.status_line) : resp.error.message;
        auto result = HTTPResponse{ err.c_str(),
                                    resp.text.c_str(), headerPairs};
        return result;
    }

    QString NetworkRequestHelper::GetHeader(const QList<QPair<QByteArray, QByteArray>> &header, const QString &name) {
        for (const auto &p: header) {
            if (QString(p.first).toLower() == name.toLower()) return p.second;
        }
        return "";
    }

    QString NetworkRequestHelper::DownloadAsset(const QString &url, const QString &fileName) {
        cpr::Session session;
        session.SetUrl(cpr::Url{url.toStdString()});
        session.SetSslOptions(cpr::Ssl(cpr::ssl::NoRevoke{true}));
        session.SetProgressCallback(cpr::ProgressCallback(
            [&](cpr::cpr_pf_arg_t downloadTotal, cpr::cpr_pf_arg_t downloadNow, cpr::cpr_pf_arg_t /*uploadTotal*/, cpr::cpr_pf_arg_t /*uploadNow*/, intptr_t /*userdata*/) {
                    runOnUiThread([=]{
                        GetMainWindow()->setDownloadReport(DownloadProgressReport{fileName, downloadNow, downloadTotal}, true);
                        GetMainWindow()->UpdateDataView();
                    });
                return true;
            }));
        if (Configs::dataStore->spmode_system_proxy) {
            session.SetProxies({{"http", "127.0.0.1:" + QString(Int2String(Configs::dataStore->inbound_socks_port)).toStdString()},
                                {"https", "127.0.0.1:" + QString(Int2String(Configs::dataStore->inbound_socks_port)).toStdString()}});
        }
        auto filePath = Configs::GetBasePath()+ "/" + fileName;
        auto tempFilePath = QString(filePath) + ".part";
        QFile::remove(tempFilePath);

        std::ofstream fout;
        fout.open(tempFilePath.toStdString(), std::ios::trunc | std::ios::out | std::ios::binary);
        auto r = session.Download(fout);
        fout.close();

        runOnUiThread([=]
        {
            GetMainWindow()->setDownloadReport({}, false);
            GetMainWindow()->UpdateDataView(true);
        });
        auto tmpFile = QFile(tempFilePath);
        if (r.status_code != 200 && r.error.code != cpr::ErrorCode::OK) {
            tmpFile.remove();
            if (r.status_code == 0) {
                return "Please check the URL and your network Connectivity";
            }
            return r.status_line.c_str();
        }
        QFile::remove(filePath);
        if (!tmpFile.rename(filePath)) {
            tmpFile.remove();
            return tmpFile.errorString();
        }
        return "";
    }

} // namespace Configs_network
