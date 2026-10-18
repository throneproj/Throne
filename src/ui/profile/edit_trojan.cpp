#include "include/ui/profile/edit_trojan.h"

#include "include/ui/utils/ValidationForm.h"

EditTrojan::EditTrojan(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditTrojan) {
    ui->setupUi(this);
}

EditTrojan::~EditTrojan() {
    delete ui;
}

void EditTrojan::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = ent->Trojan();

    ui->password->setText(outbound->password);
}

bool EditTrojan::onEnd() {
    auto outbound = ent->Trojan();

    outbound->password = ui->password->text();
    return true;
}

void EditTrojan::bindValidation(Validation::Form &form) {
    form.bind(ui->password, Validation::Rules::Required(), this).raw();
}