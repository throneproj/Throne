#include "include/ui/profile/edit_hysteria.h"

EditHysteria::EditHysteria(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditHysteria) {
    ui->setupUi(this);

    _protocol_version = ui->protocol_version;
}

EditHysteria::~EditHysteria() {
    delete ui;
}

void EditHysteria::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = _ent->Hysteria();

    ui->protocol_version->setCurrentText(outbound->protocol_version);
    ui->server_ports->setText(outbound->server_ports.join(","));
    ui->hop_interval->setText(outbound->hop_interval);
    ui->up_mbps->setText(Int2String(outbound->up_mbps));
    ui->down_mbps->setText(Int2String(outbound->down_mbps));
    ui->obfs->setText(outbound->obfs);
    ui->auth_type->setCurrentText(outbound->auth_type);
    ui->auth->setText(outbound->auth);
    ui->recv_window->setText(Int2String(outbound->recv_window));
    ui->recv_window_conn->setText(Int2String(outbound->recv_window_conn));
    ui->disable_mtu_discovery->setChecked(outbound->disable_mtu_discovery);
    ui->password->setText(outbound->password);
    editHysteriaLayout(outbound->protocol_version);
}

bool EditHysteria::onEnd() {
    auto outbound = ent->Hysteria();
    outbound->protocol_version = ui->protocol_version->currentText();
    outbound->server_ports = SplitAndTrim(ui->server_ports->text(), ",");
    outbound->hop_interval = ui->hop_interval->text();
    outbound->up_mbps = ui->up_mbps->text().toInt();
    outbound->down_mbps = ui->down_mbps->text().toInt();
    outbound->obfs = ui->obfs->text();
    outbound->auth_type = ui->auth_type->currentText();
    outbound->auth = ui->auth->text();
    outbound->recv_window = ui->recv_window->text().toInt();
    outbound->recv_window_conn = ui->recv_window_conn->text().toInt();
    outbound->disable_mtu_discovery = ui->disable_mtu_discovery->isChecked();
    outbound->password = ui->password->text();
    return true;
}

void EditHysteria::editHysteriaLayout(const QString& version) {
    if (version == "1")
    {
        ui->auth_type->setVisible(true);
        ui->auth_type_l->setVisible(true);
        ui->auth->setVisible(true);
        ui->auth_l->setVisible(true);
        ui->recv_window_conn->setVisible(true);
        ui->recv_window_conn_l->setVisible(true);
        ui->recv_window->setVisible(true);
        ui->recv_window_l->setVisible(true);
        ui->disable_mtu_discovery->setVisible(true);
        ui->password->setVisible(false);
        ui->password_l->setVisible(false);
    } else
    {
        ui->auth_type->setVisible(false);
        ui->auth_type_l->setVisible(false);
        ui->auth->setVisible(false);
        ui->auth_l->setVisible(false);
        ui->recv_window_conn->setVisible(false);
        ui->recv_window_conn_l->setVisible(false);
        ui->recv_window->setVisible(false);
        ui->recv_window_l->setVisible(false);
        ui->disable_mtu_discovery->setVisible(false);
        ui->password->setVisible(true);
        ui->password_l->setVisible(true);
    }
}