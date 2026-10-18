#include "include/ui/profile/edit_tailscale.h"

#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

EditTailScale::EditTailScale(QWidget *parent) : QWidget(parent), ui(new Ui::EditTailScale) {
    ui->setupUi(this);
}

EditTailScale::~EditTailScale() {
    delete ui;
}

void EditTailScale::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Tailscale();

    ui->state_dir->setText(outbound->state_directory);
    ui->auth_key->setText(outbound->auth_key);
    ui->control_plane->setText(outbound->control_url);
    ui->ephemeral->setChecked(outbound->ephemeral);
    ui->hostname->setText(outbound->hostname);
    ui->accept_route->setChecked(outbound->accept_routes);
    ui->exit_node->setText(outbound->exit_node);
    ui->exit_node_lan_access->setChecked(outbound->exit_node_allow_lan_access);
    ui->advertise_routes->setText(outbound->advertise_routes.join(","));
    ui->advertise_exit_node->setChecked(outbound->advertise_exit_node);
    ui->global_dns->setChecked(outbound->globalDNS);
}

bool EditTailScale::onEnd() {
    auto outbound = this->ent->Tailscale();

    outbound->state_directory = ui->state_dir->text().trimmed();
    outbound->auth_key = ui->auth_key->text();
    outbound->control_url = ui->control_plane->text().trimmed();
    outbound->ephemeral = ui->ephemeral->isChecked();
    outbound->hostname = ui->hostname->text().trimmed();
    outbound->accept_routes = ui->accept_route->isChecked();
    outbound->exit_node = ui->exit_node->text().trimmed();
    outbound->exit_node_allow_lan_access = ui->exit_node_lan_access->isChecked();
    outbound->advertise_routes = SplitAndTrim(ui->advertise_routes->text(), ",", false);
    outbound->advertise_exit_node = ui->advertise_exit_node->isChecked();
    outbound->globalDNS = ui->global_dns->isChecked();

    return true;
}

void EditTailScale::bindValidation(Validation::Form &form) {
    using namespace Validation;
    form.bind(ui->control_plane, Configs::Checks::TailscaleControlUrl, this);
    form.bind(ui->advertise_routes, Rules::List(Split::Comma, Configs::Checks::TailscaleRoute), this);
    form.addRule({ui->exit_node, ui->advertise_exit_node}, [this] {
        return Configs::Checks::TailscaleExitNode(ui->exit_node->text(), ui->advertise_exit_node->isChecked());
    }, this);
}
