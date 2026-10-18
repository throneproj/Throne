#include "include/ui/profile/edit_anytls.h"

#include <QUuid>
#include <QRegularExpressionValidator>
#include <limits>
#include "include/ui/utils/GuiUtils.hpp"
#include "include/ui/utils/ValidationForm.h"

EditAnyTLS::EditAnyTLS(QWidget *parent) : QWidget(parent), ui(new Ui::EditAnyTLS) {
    ui->setupUi(this);
    ui->min->setValidator(QRegExpValidator_Number);
}

EditAnyTLS::~EditAnyTLS() {
    delete ui;
}

void EditAnyTLS::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->AnyTLS();

    ui->password->setText(outbound->password);
    ui->interval->setText(outbound->idle_session_check_interval);
    ui->timeout->setText(outbound->idle_session_timeout);
    ui->min->setText(Int2String(outbound->min_idle_session));
}

bool EditAnyTLS::onEnd() {
    auto outbound = this->ent->AnyTLS();

    outbound->password = ui->password->text();
    outbound->idle_session_check_interval = ui->interval->text().trimmed();
    outbound->idle_session_timeout = ui->timeout->text().trimmed();
    outbound->min_idle_session = ui->min->text().trimmed().toInt();

    return true;
}

void EditAnyTLS::bindValidation(Validation::Form &form) {
    form.bind(ui->password, Validation::Rules::Required(), this).raw();
    form.bind(ui->interval, Validation::Rules::Duration(), this);
    form.bind(ui->timeout, Validation::Rules::Duration(), this);
    form.bind(ui->min, Validation::Rules::Int(0, std::numeric_limits<int>::max()), this);
}
