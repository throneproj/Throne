#include "include/ui/profile/edit_vmess.h"

#include <QUuid>

#include <limits>

#include "include/ui/utils/ValidationForm.h"

EditVMess::EditVMess(QWidget *parent) : QWidget(parent), ui(new Ui::EditVMess) {
    ui->setupUi(this);
    connect(ui->uuidgen, &QPushButton::clicked, this, [=,this] { ui->uuid->setText(QUuid::createUuid().toString().remove("{").remove("}")); });
    ui->packet_encoding->addItems(Configs::vPacketEncoding);
}

EditVMess::~EditVMess() {
    delete ui;
}

void EditVMess::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->VMess();

    ui->uuid->setText(outbound->uuid);
    ui->aid->setText(Int2String(outbound->alter_id));
    ui->packet_encoding->setCurrentText(outbound->packet_encoding);
    ui->security->setCurrentText(outbound->security);
}

bool EditVMess::onEnd() {
    auto outbound = this->ent->VMess();

    outbound->uuid = ui->uuid->text().trimmed();
    outbound->alter_id = ui->aid->text().trimmed().toInt();
    outbound->packet_encoding = ui->packet_encoding->currentText().trimmed();
    outbound->security = ui->security->currentText().trimmed();

    return true;
}

void EditVMess::bindValidation(Validation::Form &form) {
    form.bind(ui->uuid, Validation::Rules::Required(), this);
    // The core treats a negative alter id as 0.
    form.bind(ui->aid, Validation::Rules::Int(std::numeric_limits<int>::min(), std::numeric_limits<int>::max()), this);
    form.bind(ui->security, Validation::Rules::OneOf(Configs::vmessSecurity), this);
}
