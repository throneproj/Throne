#include "include/ui/profile/edit_vmess.h"

#include <QUuid>

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

    outbound->uuid = ui->uuid->text();
    outbound->alter_id = ui->aid->text().toInt();
    outbound->packet_encoding = ui->packet_encoding->currentText();
    outbound->security = ui->security->currentText();

    return true;
}
