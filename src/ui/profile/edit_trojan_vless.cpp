#include "include/ui/profile/edit_trojan_vless.h"

#include "include/configs/proxy/TrojanVLESSBean.hpp"
#include "include/configs/proxy/Preset.hpp"

EditTrojanVLESS::EditTrojanVLESS(QWidget *parent) : QWidget(parent), ui(new Ui::EditTrojanVLESS) {
    ui->setupUi(this);
    flow_ = ui->flow;
}

EditTrojanVLESS::~EditTrojanVLESS() {
    delete ui;
}

void EditTrojanVLESS::onStart(std::shared_ptr<NekoGui::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->TrojanVLESSBean();
    if (bean->proxy_type == NekoGui_fmt::TrojanVLESSBean::proxy_VLESS) {
        ui->label->setText("UUID");
    }
    if (bean->proxy_type != NekoGui_fmt::TrojanVLESSBean::proxy_VLESS) {
        ui->flow->hide();
        ui->flow_l->hide();
    }
    ui->password->setText(bean->password);
    ui->flow->addItems(Preset::SingBox::Flows );
    ui->flow->setCurrentText(bean->flow);
}

bool EditTrojanVLESS::onEnd() {
    auto bean = this->ent->TrojanVLESSBean();
    bean->password = ui->password->text();
    bean->flow = ui->flow->currentText();
    return true;
}
