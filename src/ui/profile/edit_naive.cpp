#include "include/ui/profile/edit_naive.h"

EditNaive::EditNaive(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditNaive) {

    ui->setupUi(this);
}

EditNaive::~EditNaive() {
    delete ui;
}

void EditNaive::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = ent->Naive();

    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
    ui->uot->setChecked(outbound->uot);
    ui->quic->setChecked(outbound->quic);
    ui->congestion_control->setCurrentText(outbound->congestion_control.isEmpty() ? "bbr" : outbound->congestion_control);
}

bool EditNaive::onEnd() {
    auto outbound = ent->Naive();

    outbound->username = ui->username->text();
    outbound->password = ui->password->text();
    outbound->uot = ui->uot->isChecked();
    outbound->quic = ui->quic->isChecked();
    outbound->congestion_control = ui->congestion_control->currentText();
    return true;
}