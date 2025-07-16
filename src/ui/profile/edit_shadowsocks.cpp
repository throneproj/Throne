#include "include/ui/profile/edit_shadowsocks.h"

#include "include/configs/proxy/ShadowSocksBean.hpp"
#include "include/configs/proxy/Preset.hpp"

EditShadowSocks::EditShadowSocks(QWidget *parent) : QWidget(parent),
                                                    ui(new Ui::EditShadowSocks) {
    ui->setupUi(this);
    ui->method->addItems(Preset::SingBox::ShadowsocksMethods);
}

EditShadowSocks::~EditShadowSocks() {
    delete ui;
}

void EditShadowSocks::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->ShadowSocksBean();

    ui->method->setCurrentText(bean->method);
    ui->uot->setCurrentIndex(bean->uot);
    ui->password->setText(bean->password);
    auto ssPlugin = bean->plugin.split(";");
    if (!ssPlugin.empty()) {
        ui->plugin->setCurrentText(ssPlugin[0]);
        ui->plugin_opts->setText(SubStrAfter(bean->plugin, ";"));
    }
}

bool EditShadowSocks::onEnd() {
    auto bean = this->ent->ShadowSocksBean();

    bean->method = ui->method->currentText();
    bean->password = ui->password->text();
    bean->uot = ui->uot->currentIndex();
    bean->plugin = ui->plugin->currentText();
    if (!bean->plugin.isEmpty()) {
        bean->plugin += ";" + ui->plugin_opts->text();
    }

    return true;
}
