#include "include/ui/profile/edit_wireguard.h"

#include "include/ui/profile/edit_wireguard_amnezia.h"

#include "include/api/RPC.h"
#include "include/configs/sub/warp.h"
#include "include/global/Utils.hpp"

EditWireguard::EditWireguard(QWidget *parent) : QWidget(parent), ui(new Ui::EditWireguard) {
    ui->setupUi(this);

    connect(ui->amnezia_options, &QPushButton::clicked, this, [=, this] {
        if (ent == nullptr) return;
        auto *amnezia = new EditWireguardAmnezia(this, ent);
        amnezia->show();
    });

    connect(ui->warp_autogen, &QPushButton::clicked, this, [=, this] {
        auto originalText = ui->warp_autogen->text();
        // genWarpConfig spins a nested event loop, so the button stays clickable through the request.
        ui->warp_autogen->setEnabled(false);
        ui->warp_autogen->setText(tr("Getting keypair..."));
        bool ok;
        auto keyPair = API::defaultClient->GenWgKeyPair(&ok);
        if (!ok) {
            runOnUiThread([=] { MessageBoxWarning(tr("Failed to get key pair"), keyPair.error->c_str()); });
            ui->warp_autogen->setText(originalText);
            ui->warp_autogen->setEnabled(true);
            return;
        }
        ui->warp_autogen->setText(tr("Generating config..."));
        QString error;
        auto conf = Configs_network::genWarpConfig(&error, keyPair.private_key->c_str(), keyPair.public_key->c_str());
        if (!error.isEmpty()) {
            runOnUiThread([=] { MessageBoxWarning(tr("Failed to generate warp config"), error); });
            ui->warp_autogen->setText(originalText);
            ui->warp_autogen->setEnabled(true);
            return;
        }
        ui->private_key->setText(conf->privateKey);
        ui->public_key->setText(conf->publicKey);
        ui->local_addr->setText(conf->ipv4Address + "/32," + conf->ipv6Address + "/128");
        ui->mtu->setText("1280");
        ui->persistent_keepalive->setText("30");
        // The endpoint lives in the outer profile dialog's address/port fields.
        if (auto sep = conf->endpoint.lastIndexOf(':'); sep > 0) {
            if (set_edit_text_serverAddress) set_edit_text_serverAddress(conf->endpoint.left(sep));
            if (set_edit_text_serverPort) set_edit_text_serverPort(conf->endpoint.mid(sep + 1));
        }
        ui->reserved->setText(QListInt2QListString(conf->reserved).join(","));
        ui->warp_autogen->setText(tr("Success!"));
        setTimeout([=, this] {
            ui->warp_autogen->setText(originalText);
            ui->warp_autogen->setEnabled(true);
        }, this, 2000);
    });
}

EditWireguard::~EditWireguard() {
    delete ui;
}

void EditWireguard::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Wireguard();

#ifndef Q_OS_LINUX
    adjustSize();
#endif

    ui->private_key->setText(outbound->private_key);
    ui->public_key->setText(outbound->peer->public_key);
    ui->preshared_key->setText(outbound->peer->pre_shared_key);
    ui->reserved->setText(QListInt2QListString(outbound->peer->reserved).join(","));
    ui->persistent_keepalive->setText(outbound->peer->persistent_keepalive);
    ui->mtu->setText(Int2String(outbound->mtu));
    ui->sys_ifc->setChecked(outbound->system);
    ui->local_addr->setText(outbound->address.join(","));
    ui->workers->setText(Int2String(outbound->worker_count));

    ui->enable_amnezia->setChecked(outbound->enable_amnezia);
}

bool EditWireguard::onEnd() {
    auto outbound = this->ent->Wireguard();

    outbound->private_key = ui->private_key->text().trimmed();
    outbound->peer->public_key = ui->public_key->text().trimmed();
    outbound->peer->pre_shared_key = ui->preshared_key->text().trimmed();
    auto rawReserved = ui->reserved->text();
    outbound->peer->reserved = {};
    for (const auto& item: rawReserved.split(",")) {
        if (item.trimmed().isEmpty()) continue;
        outbound->peer->reserved += item.trimmed().toInt();
    }
    outbound->peer->persistent_keepalive = ui->persistent_keepalive->text().trimmed();
    outbound->mtu = ui->mtu->text().trimmed().toInt();
    outbound->system = ui->sys_ifc->isChecked();
    outbound->address = ui->local_addr->text().replace(" ", "").split(",");
    outbound->worker_count = ui->workers->text().trimmed().toInt();

    outbound->enable_amnezia = ui->enable_amnezia->isChecked();

    return true;
}