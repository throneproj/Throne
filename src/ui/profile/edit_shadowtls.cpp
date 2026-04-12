#include "include/ui/profile/edit_shadowtls.h"

EditShadowTLS::EditShadowTLS(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditShadowTLS) {

    ui->setupUi(this);
}

EditShadowTLS::~EditShadowTLS() {
    delete ui;
}

void EditShadowTLS::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = ent->ShadowTLS();

    ui->version->setCurrentText(Int2String(outbound->version));
    ui->password->setText(outbound->password);
}

bool EditShadowTLS::onEnd() {
    auto outbound = ent->ShadowTLS();

    outbound->version = ui->version->currentText().toInt();
    outbound->password = ui->password->text();
    return true;
}