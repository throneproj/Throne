#include "include/ui/profile/edit_shadowsocks.h"

#include "include/ui/utils/ValidationForm.h"

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
    // Older entries may hold the obfs-local plugin under its simple-obfs or Clash name.
    const QString plugin = outbound->plugin.trimmed();
    ui->plugin->setCurrentText(plugin == "simple-obfs" || plugin == "obfs" ? QStringLiteral("obfs-local") : plugin);
    ui->plugin_opts->setText(outbound->plugin_opts);
}

bool EditShadowSocks::onEnd() {
    auto outbound = this->ent->ShadowSocks();

    outbound->method = ui->method->currentText().trimmed();
    outbound->password = ui->password->text();
    outbound->uot = ui->uot->isChecked();
    outbound->plugin = ui->plugin->currentText().trimmed();
    outbound->plugin_opts = ui->plugin_opts->text().trimmed();

    return true;
}

void EditShadowSocks::bindValidation(Validation::Form &form) {
    form.bind(ui->method, Validation::Rules::OneOf(Configs::shadowsocksMethods, Validation::Empty::Required), this);
    form.addRule({ui->password, ui->method}, [this] {
        return Configs::Checks::ShadowsocksPassword(ui->method->currentText().trimmed(), ui->password->text());
    }, this);
    form.addRule({ui->plugin_opts, ui->plugin}, [this] {
        return Configs::Checks::ShadowsocksPluginOpts(ui->plugin->currentText(), ui->plugin_opts->text().trimmed());
    }, this);
}
