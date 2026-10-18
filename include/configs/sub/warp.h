#pragma once
#include <QList>
#include <QString>
#include <memory>

class QWidget;

namespace Configs_network {
    struct WarpIdentity {
        QString deviceId, token, privateKey, peerPublicKey, endpoint, ipv4, ipv6;
        QList<int> reserved;
    };

    // Blocking (RPC); never call on the UI thread. tunnelType: "wireguard" | "masque".
    std::shared_ptr<WarpIdentity> RegisterWarp(const QString &tunnelType, QString *error);
    // Asks once (persisted in settings warp_tos_accepted) whether the user accepts Cloudflare's WARP terms; UI thread only. Without a QApplication it returns the stored answer.
    bool ConfirmWarpTerms(QWidget *parent);
}
