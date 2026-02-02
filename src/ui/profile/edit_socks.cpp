#include "include/ui/profile/edit_socks.h"

EditSocks::EditSocks(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditSocks) {

    ui->setupUi(this);
}

EditSocks::~EditSocks() {
    delete ui;
}

void EditSocks::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Socks();

    ui->version->setCurrentText(Int2String(outbound->version));
    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
}

bool EditSocks::onEnd() {
    auto outbound = this->ent->Socks();

    outbound->version = ui->version->currentText().toInt();
    outbound->username = ui->username->text();
    outbound->password = ui->password->text();
    return true;
}