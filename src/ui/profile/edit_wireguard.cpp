#include "include/ui/profile/edit_wireguard.h"

EditWireguard::EditWireguard(QWidget *parent) : QWidget(parent), ui(new Ui::EditWireguard) {
    ui->setupUi(this);
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
    ui->persistent_keepalive->setText(Int2String(outbound->peer->persistent_keepalive));
    ui->mtu->setText(Int2String(outbound->mtu));
    ui->sys_ifc->setChecked(outbound->system);
    ui->local_addr->setText(outbound->address.join(","));
    ui->workers->setText(Int2String(outbound->worker_count));

    ui->enable_amnezia->setChecked(outbound->enable_amnezia);
    ui->junk_packet_count->setText(Int2String(outbound->junk_packet_count));
    ui->junk_packet_min_size->setText(Int2String(outbound->junk_packet_min_size));
    ui->junk_packet_max_size->setText(Int2String(outbound->junk_packet_max_size));
    ui->init_packet_junk_size->setText(Int2String(outbound->init_packet_junk_size));
    ui->response_packet_junk_size->setText(Int2String(outbound->response_packet_junk_size));
    ui->init_packet_magic_header->setText(Int2String(outbound->init_packet_magic_header));
    ui->response_packet_magic_header->setText(Int2String(outbound->response_packet_magic_header));
    ui->underload_packet_magic_header->setText(Int2String(outbound->underload_packet_magic_header));
    ui->transport_packet_magic_header->setText(Int2String(outbound->transport_packet_magic_header));
}

bool EditWireguard::onEnd() {
    auto outbound = this->ent->Wireguard();

    outbound->private_key = ui->private_key->text();
    outbound->peer->public_key = ui->public_key->text();
    outbound->peer->pre_shared_key = ui->preshared_key->text();
    auto rawReserved = ui->reserved->text();
    outbound->peer->reserved = {};
    for (const auto& item: rawReserved.split(",")) {
        if (item.trimmed().isEmpty()) continue;
        outbound->peer->reserved += item.trimmed().toInt();
    }
    outbound->peer->persistent_keepalive = ui->persistent_keepalive->text().toInt();
    outbound->mtu = ui->mtu->text().toInt();
    outbound->system = ui->sys_ifc->isChecked();
    outbound->address = ui->local_addr->text().replace(" ", "").split(",");
    outbound->worker_count = ui->workers->text().toInt();

    outbound->enable_amnezia = ui->enable_amnezia->isChecked();
    outbound->junk_packet_count = ui->junk_packet_count->text().toInt();
    outbound->junk_packet_min_size = ui->junk_packet_min_size->text().toInt();
    outbound->junk_packet_max_size = ui->junk_packet_max_size->text().toInt();
    outbound->init_packet_junk_size = ui->init_packet_junk_size->text().toInt();
    outbound->response_packet_junk_size = ui->response_packet_junk_size->text().toInt();
    outbound->init_packet_magic_header = ui->init_packet_magic_header->text().toInt();
    outbound->response_packet_magic_header = ui->response_packet_magic_header->text().toInt();
    outbound->underload_packet_magic_header = ui->underload_packet_magic_header->text().toInt();
    outbound->transport_packet_magic_header = ui->transport_packet_magic_header->text().toInt();

    return true;
}