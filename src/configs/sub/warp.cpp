#include <include/configs/sub/warp.h>
#include <include/api/RPC.h>
#include <include/global/Configs.hpp>
#include <include/global/HTTPRequestHelper.hpp>
#include <include/global/LocalNetwork.hpp>
#include <QMessageBox>
#include <QObject>
#include <QUrl>

namespace Configs_network {
    namespace {
        QString warpProxy(QString *error) {
            if (!RequestUsesMixedInbound(false, error)) return {};
            const auto &settings = Configs::dataManager->settingsRepo;
            QString host = LocalNetwork::InboundConnectHost();
            if (host.contains(':')) host = "[" + host + "]";
            QString credentials;
            if (settings->inbound_auth) {
                credentials = QString::fromLatin1(QUrl::toPercentEncoding(settings->inbound_user)) + ":" +
                              QString::fromLatin1(QUrl::toPercentEncoding(settings->inbound_pass)) + "@";
            }
            return "http://" + credentials + host + ":" + QString::number(settings->inbound_socks_port);
        }
    }

    std::shared_ptr<WarpIdentity> RegisterWarp(const QString &tunnelType, QString *error) {
        const auto proxy = warpProxy(error);
        if (!error->isEmpty()) return nullptr;

        bool rpcOK = false;
        const auto reply = API::defaultClient->WarpRegister(&rpcOK, tunnelType, proxy,
                                                            Configs::dataManager->settingsRepo->warp_api_hosts);
        if (!rpcOK) {
            *error = QObject::tr("Failed to reach the core.");
            return nullptr;
        }
        if (const auto coreError = reply.error.value_or(""); !coreError.empty()) {
            *error = QString::fromStdString(coreError);
            return nullptr;
        }

        auto identity = std::make_shared<WarpIdentity>();
        identity->deviceId = QString::fromStdString(reply.device_id.value_or(""));
        identity->token = QString::fromStdString(reply.token.value_or(""));
        identity->privateKey = QString::fromStdString(reply.private_key.value_or(""));
        identity->peerPublicKey = QString::fromStdString(reply.peer_public_key.value_or(""));
        identity->endpoint = QString::fromStdString(reply.endpoint.value_or(""));
        identity->ipv4 = QString::fromStdString(reply.ipv4.value_or(""));
        identity->ipv6 = QString::fromStdString(reply.ipv6.value_or(""));
        for (const auto byte : reply.reserved) identity->reserved << byte;
        return identity;
    }

    bool ConfirmWarpTerms(QWidget *parent) {
        const auto &settings = Configs::dataManager->settingsRepo;
        if (settings->warp_tos_accepted) return true;
        if (!IsGuiApp()) {
            LogHeadlessDialog(QStringLiteral("ConfirmWarpTerms"), QStringLiteral("Cloudflare WARP"),
                              QStringLiteral("Accept the Cloudflare WARP terms of service?"), QStringLiteral("false"));
            return settings->warp_tos_accepted;
        }

        QMessageBox box(QMessageBox::Question, QObject::tr("Cloudflare WARP"),
                        QObject::tr("Generating a WARP identity registers a new device with Cloudflare.<br><br>"
                                    "Do you accept the <a href=\"%1\">Cloudflare WARP terms of service</a>?")
                            .arg(QStringLiteral("https://www.cloudflare.com/application/terms/")),
                        QMessageBox::Yes | QMessageBox::No, parent);
        box.setTextFormat(Qt::RichText);
        box.setTextInteractionFlags(Qt::TextBrowserInteraction);
        if (box.exec() != QMessageBox::Yes) return false;

        settings->warp_tos_accepted = true;
        settings->Save();
        return true;
    }
}
