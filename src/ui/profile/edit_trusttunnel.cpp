#include "include/ui/profile/edit_trusttunnel.h"

EditTrustTunnel::EditTrustTunnel(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditTrustTunnel) {

    ui->setupUi(this);
}

EditTrustTunnel::~EditTrustTunnel() {
    delete ui;
}

void EditTrustTunnel::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = ent->TrustTunnel();

    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
    ui->health_check->setChecked(outbound->health_check);
    ui->quic->setChecked(outbound->quic);
    ui->congestion_control->setCurrentText(outbound->congestion_control.isEmpty() ? "bbr" : outbound->congestion_control);
}

bool EditTrustTunnel::onEnd() {
    auto outbound = ent->TrustTunnel();

    outbound->username = ui->username->text();
    outbound->password = ui->password->text();
    outbound->health_check = ui->health_check->isChecked();
    outbound->quic = ui->quic->isChecked();
    outbound->congestion_control = ui->congestion_control->currentText();
    return true;
}