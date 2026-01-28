#include "include/ui/profile/edit_anytls.h"

#include <QUuid>
#include <QRegularExpressionValidator>
#include "include/global/GuiUtils.hpp"

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
    outbound->idle_session_check_interval = ui->interval->text();
    outbound->idle_session_timeout = ui->timeout->text();
    outbound->min_idle_session = ui->min->text().toInt();

    return true;
}
