#include "include/ui/profile/edit_vless.h"

EditVless::EditVless(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditVless) {
    ui->setupUi(this);

    _flow = ui->flow;
    QStringList flows = {""};
    flows << Configs::vlessFlows;
    ui->flow->addItems(flows);
    ui->packet_encoding->addItems(Configs::vPacketEncoding);
}

EditVless::~EditVless() {
    delete ui;
}

void EditVless::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->VLESS();

    ui->uuid->setText(outbound->uuid);
    ui->flow->setCurrentText(outbound->flow);
    ui->packet_encoding->setCurrentText(outbound->packet_encoding);
}

bool EditVless::onEnd() {
    auto outbound = this->ent->VLESS();

    outbound->uuid = ui->uuid->text();
    outbound->flow = ui->flow->currentText();
    outbound->packet_encoding = ui->packet_encoding->currentText();
    return true;
}