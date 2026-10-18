#include "include/ui/profile/edit_juicity.h"

#include "include/ui/utils/ValidationForm.h"

EditJuicity::EditJuicity(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditJuicity) {

    ui->setupUi(this);
}

EditJuicity::~EditJuicity() {
    delete ui;
}

void EditJuicity::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = ent->Juicity();

    ui->uuid->setText(outbound->uuid);
    ui->password->setText(outbound->password);
}

bool EditJuicity::onEnd() {
    auto outbound = ent->Juicity();

    outbound->uuid = ui->uuid->text().trimmed();
    outbound->password = ui->password->text();
    return true;
}

void EditJuicity::bindValidation(Validation::Form &form) {
    form.bind(ui->uuid, Validation::Rules::Uuid(), this);
}