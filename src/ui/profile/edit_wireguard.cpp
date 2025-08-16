#include "include/ui/profile/edit_wireguard.h"

#include "include/configs/proxy/WireguardBean.h"

EditWireguard::EditWireguard(QWidget *parent) : QWidget(parent), ui(new Ui::EditWireguard) {
    ui->setupUi(this);
}

EditWireguard::~EditWireguard() {
    delete ui;
}

void EditWireguard::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->WireguardBean();

#ifndef Q_OS_LINUX
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
    ui->local_addr->setText(bean->localAddress.join(","));
    ui->workers->setText(Int2String(bean->workerCount));

    ui->enable_amnezia->setChecked(bean->enable_amnezia);
    ui->junk_packet_count->setText(Int2String(bean->junk_packet_count));
    ui->junk_packet_min_size->setText(Int2String(bean->junk_packet_min_size));
    ui->junk_packet_max_size->setText(Int2String(bean->junk_packet_max_size));
    ui->init_packet_junk_size->setText(Int2String(bean->init_packet_junk_size));
    ui->response_packet_junk_size->setText(Int2String(bean->response_packet_junk_size));
    ui->init_packet_magic_header->setText(Int2String(bean->init_packet_magic_header));
    ui->response_packet_magic_header->setText(Int2String(bean->response_packet_magic_header));
    ui->underload_packet_magic_header->setText(Int2String(bean->underload_packet_magic_header));
    ui->transport_packet_magic_header->setText(Int2String(bean->transport_packet_magic_header));
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
    bean->localAddress = ui->local_addr->text().replace(" ", "").split(",");
    bean->workerCount = ui->workers->text().toInt();

    bean->enable_amnezia = ui->enable_amnezia->isChecked();
    bean->junk_packet_count = ui->junk_packet_count->text().toInt();
    bean->junk_packet_min_size = ui->junk_packet_min_size->text().toInt();
    bean->junk_packet_max_size = ui->junk_packet_max_size->text().toInt();
    bean->init_packet_junk_size = ui->init_packet_junk_size->text().toInt();
    bean->response_packet_junk_size = ui->response_packet_junk_size->text().toInt();
    bean->init_packet_magic_header = ui->init_packet_magic_header->text().toInt();
    bean->response_packet_magic_header = ui->response_packet_magic_header->text().toInt();
    bean->underload_packet_magic_header = ui->underload_packet_magic_header->text().toInt();
    bean->transport_packet_magic_header = ui->transport_packet_magic_header->text().toInt();

    return true;
}