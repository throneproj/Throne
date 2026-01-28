#include "include/ui/profile/edit_shadowsocks.h"

EditShadowSocks::EditShadowSocks(QWidget *parent) : QWidget(parent),
                                                    ui(new Ui::EditShadowSocks) {
    ui->setupUi(this);
    ui->method->addItems(Configs::shadowsocksMethods);
}

EditShadowSocks::~EditShadowSocks() {
    delete ui;
}

void EditShadowSocks::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->ShadowSocks();

    if (outbound->plugin.contains(";")) {
        outbound->plugin_opts = SubStrAfter(outbound->plugin, ";");
        outbound->plugin = SubStrBefore(outbound->plugin, ";");
    }
    ui->method->setCurrentText(outbound->method);
    ui->uot->setChecked(outbound->uot);
    ui->password->setText(outbound->password);
    ui->plugin->setCurrentText(outbound->plugin);
    ui->plugin_opts->setText(outbound->plugin_opts);
}

bool EditShadowSocks::onEnd() {
    auto outbound = this->ent->ShadowSocks();

    outbound->method = ui->method->currentText();
    outbound->password = ui->password->text();
    outbound->uot = ui->uot->isChecked();
    outbound->plugin = ui->plugin->currentText();
    outbound->plugin_opts = ui->plugin_opts->text();

    return true;
}
