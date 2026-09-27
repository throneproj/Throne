#include "include/ui/profile/edit_vless.h"

EditVless::EditVless(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditVless) {
    ui->setupUi(this);

    QStringList flows = {""};
    flows << Configs::vlessFlows;
    ui->flow->addItems(flows);
    ui->packet_encoding->addItems(Configs::vPacketEncoding);
    connect(ui->flow, &QComboBox::currentTextChanged, this, [this] {
        if (editor_state_changed) editor_state_changed();
    });
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

    outbound->uuid = ui->uuid->text().trimmed();
    outbound->flow = ui->flow->currentText().trimmed();
    outbound->packet_encoding = ui->packet_encoding->currentText().trimmed();
    return true;
}

bool EditVless::blocksMultiplex() {
    return ui->flow->currentText() == "xtls-rprx-vision";
}