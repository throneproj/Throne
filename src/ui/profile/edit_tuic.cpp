#include "include/ui/profile/edit_tuic.h"

#include "include/ui/utils/ValidationForm.h"

namespace {
    // Older builds saved translated item text, e.g. "BBR" or "New Reno"; unknown values fall back to the first item.
    void editTuicSelectIdentifier(QComboBox* combo, const QString& value) {
        combo->setCurrentIndex(qMax(0, combo->findText(value.trimmed().replace(' ', '_'), Qt::MatchFixedString)));
    }
}

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
    editTuicSelectIdentifier(ui->congestion_control, outbound->congestion_control);
    editTuicSelectIdentifier(ui->udp_relay_mode, outbound->udp_relay_mode);
    ui->udp_over_stream->setChecked(outbound->udp_over_stream);
    ui->zero_rtt->setChecked(outbound->zero_rtt_handshake);
    ui->heartbeat->setText(outbound->heartbeat);
}

bool EditTuic::onEnd() {
    auto outbound = ent->TUIC();

    outbound->uuid = ui->uuid->text().trimmed();
    outbound->password = ui->password->text();
    outbound->congestion_control = ui->congestion_control->currentText().trimmed();
    outbound->udp_relay_mode = ui->udp_relay_mode->currentText().trimmed();
    outbound->udp_over_stream = ui->udp_over_stream->isChecked();
    outbound->zero_rtt_handshake = ui->zero_rtt->isChecked();
    outbound->heartbeat = ui->heartbeat->text().trimmed();
    return true;
}

void EditTuic::bindValidation(Validation::Form &form) {
    form.bind(ui->uuid, Validation::Rules::Uuid(), this);
    form.bind(ui->heartbeat, Validation::Rules::Duration(), this);
}