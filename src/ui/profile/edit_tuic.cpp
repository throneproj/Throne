#include "include/ui/profile/edit_tuic.h"

#include "include/configs/common/utils.h"
#include "include/ui/utils/Disclosure.hpp"
#include "include/ui/utils/ValidationForm.h"

namespace {
    // Older builds saved translated item text; unknown values fall back to the first item.
    void editTuicSelectIdentifier(QComboBox* combo, const QString& value) {
        combo->setCurrentIndex(qMax(0, combo->findText(value.trimmed().replace(' ', '_'), Qt::MatchFixedString)));
    }

    // Index 0 is the translated "Default", stored empty so the core's cubic applies; any other stored value stays as is.
    void editTuicSelectCongestion(QComboBox* combo, const QString& value) {
        const auto congestion = Configs::LegacyComboValue(value, Configs::ccAlgorithms);
        if (congestion.isEmpty()) {
            combo->setCurrentIndex(0);
            return;
        }
        int index = combo->findText(congestion);
        if (index <= 0) {
            combo->addItem(congestion);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    }
}

EditTuic::EditTuic(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditTuic) {

    ui->setupUi(this);
    Disclosure::Setup(ui->advanced_toggle, ui->advanced_box, [this] {
        if (editor_state_changed) editor_state_changed();
    });
}

EditTuic::~EditTuic() {
    delete ui;
}

void EditTuic::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = ent->TUIC();

    ui->uuid->setText(outbound->uuid);
    ui->password->setText(outbound->password);
    editTuicSelectCongestion(ui->congestion_control, outbound->congestion_control);
    editTuicSelectIdentifier(ui->udp_relay_mode, outbound->udp_relay_mode);
    ui->udp_over_stream->setChecked(outbound->udp_over_stream);
    ui->zero_rtt->setChecked(outbound->zero_rtt_handshake);
    ui->heartbeat->setText(outbound->heartbeat);
    ui->advanced_toggle->setChecked(ui->congestion_control->currentIndex() != 0 || ui->udp_relay_mode->currentIndex() != 0 ||
                                    outbound->udp_over_stream || outbound->zero_rtt_handshake || !outbound->heartbeat.isEmpty());
}

bool EditTuic::onEnd() {
    auto outbound = ent->TUIC();

    outbound->uuid = ui->uuid->text().trimmed();
    outbound->password = ui->password->text();
    outbound->congestion_control = ui->congestion_control->currentIndex() <= 0 ? QString() : ui->congestion_control->currentText().trimmed();
    outbound->udp_relay_mode = ui->udp_relay_mode->currentText().trimmed();
    outbound->udp_over_stream = ui->udp_over_stream->isChecked();
    outbound->zero_rtt_handshake = ui->zero_rtt->isChecked();
    outbound->heartbeat = ui->heartbeat->text().trimmed();
    return true;
}

void EditTuic::bindValidation(Validation::Form &form) {
    const auto duration = Validation::Rules::Duration();
    form.bind(ui->uuid, Validation::Rules::Uuid(), this);
    form.bind(ui->heartbeat, duration, this);
    Disclosure::CheckCollapsed(form, ui->advanced_toggle, ui->advanced_box, {Disclosure::FieldCheck(ui->heartbeat, duration)}, this);
}