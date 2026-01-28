#include "include/ui/profile/edit_tuic.h"

EditTuic::EditTuic(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditTuic) {

    ui->setupUi(this);
}

EditTuic::~EditTuic() {
    delete ui;
}

void EditTuic::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = ent->TUIC();

    ui->uuid->setText(outbound->uuid);
    ui->password->setText(outbound->password);
    ui->congestion_control->setCurrentText(outbound->congestion_control.isEmpty() ? "bbr" : outbound->congestion_control);
    ui->udp_relay_mode->setCurrentText(outbound->udp_relay_mode.isEmpty() ? "native" : outbound->udp_relay_mode);
    ui->udp_over_stream->setChecked(outbound->udp_over_stream);
    ui->zero_rtt->setChecked(outbound->zero_rtt_handshake);
    ui->heartbeat->setText(outbound->heartbeat);
}

bool EditTuic::onEnd() {
    auto outbound = ent->TUIC();

    outbound->uuid = ui->uuid->text();
    outbound->password = ui->password->text();
    outbound->congestion_control = ui->congestion_control->currentText();
    outbound->udp_relay_mode = ui->udp_relay_mode->currentText();
    outbound->udp_over_stream = ui->udp_over_stream->isChecked();
    outbound->zero_rtt_handshake = ui->zero_rtt->isChecked();
    outbound->heartbeat = ui->heartbeat->text();
    return true;
}