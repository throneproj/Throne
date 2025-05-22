#include "include/ui/profile/edit_wireguard.h"

#include "include/configs/proxy/WireguardBean.h"

EditWireguard::EditWireguard(QWidget *parent) : QWidget(parent), ui(new Ui::EditWireguard) {
    ui->setupUi(this);
}

EditWireguard::~EditWireguard() {
    delete ui;
}

void EditWireguard::onStart(std::shared_ptr<NekoGui::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->WireguardBean();

#ifndef Q_OS_LINUX
    ui->enable_gso->hide();
    adjustSize();
#endif

    ui->private_key->setText(bean->privateKey);
    ui->public_key->setText(bean->publicKey);
    ui->preshared_key->setText(bean->preSharedKey);
    auto reservedStr = bean->FormatReserved().replace("-", ",");
    ui->reserved->setText(reservedStr);
    ui->persistent_keepalive->setText(Int2String(bean->persistentKeepalive));
    ui->mtu->setText(Int2String(bean->MTU));
    ui->sys_ifc->setChecked(bean->useSystemInterface);
    ui->enable_gso->setChecked(bean->enableGSO);
    ui->local_addr->setText(bean->localAddress.join(","));
    ui->workers->setText(Int2String(bean->workerCount));
}

bool EditWireguard::onEnd() {
    auto bean = this->ent->WireguardBean();

    bean->privateKey = ui->private_key->text();
    bean->publicKey = ui->public_key->text();
    bean->preSharedKey = ui->preshared_key->text();
    auto rawReserved = ui->reserved->text();
    bean->reserved = {};
    for (const auto& item: rawReserved.split(",")) {
        if (item.trimmed().isEmpty()) continue;
        bean->reserved += item.trimmed().toInt();
    }
    bean->persistentKeepalive = ui->persistent_keepalive->text().toInt();
    bean->MTU = ui->mtu->text().toInt();
    bean->useSystemInterface = ui->sys_ifc->isChecked();
    bean->enableGSO = ui->enable_gso->isChecked();
    bean->localAddress = ui->local_addr->text().replace(" ", "").split(",");
    bean->workerCount = ui->workers->text().toInt();

    return true;
}