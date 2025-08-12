#include "include/ui/profile/edit_tailscale.h"

#include "include/configs/proxy/Tailscale.hpp"

EditTailScale::EditTailScale(QWidget *parent) : QWidget(parent), ui(new Ui::EditTailScale) {
    ui->setupUi(this);
}

EditTailScale::~EditTailScale() {
    delete ui;
}

void EditTailScale::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->TailscaleBean();

    ui->state_dir->setText(bean->state_directory);
    ui->auth_key->setText(bean->auth_key);
    ui->control_plane->setText(bean->control_url);
    ui->ephemeral->setChecked(bean->ephemeral);
    ui->hostname->setText(bean->hostname);
    ui->accept_route->setChecked(bean->accept_routes);
    ui->exit_node->setText(bean->exit_node);
    ui->exit_node_lan_access->setChecked(bean->exit_node_allow_lan_access);
    ui->advertise_routes->setText(bean->advertise_routes.join(","));
    ui->advertise_exit_node->setChecked(bean->advertise_exit_node);
    ui->global_dns->setChecked(bean->globalDNS);
}

bool EditTailScale::onEnd() {
    auto bean = this->ent->TailscaleBean();

    bean->state_directory = ui->state_dir->text();
    bean->auth_key = ui->auth_key->text();
    bean->control_url = ui->control_plane->text();
    bean->ephemeral = ui->ephemeral->isChecked();
    bean->hostname = ui->hostname->text();
    bean->accept_routes = ui->accept_route->isChecked();
    bean->exit_node = ui->exit_node->text();
    bean->exit_node_allow_lan_access = ui->exit_node_lan_access->isChecked();
    bean->advertise_routes = ui->advertise_routes->text().replace(" ", "").split(",");
    bean->advertise_exit_node = ui->advertise_exit_node->isChecked();
    bean->globalDNS = ui->global_dns->isChecked();

    return true;
}
