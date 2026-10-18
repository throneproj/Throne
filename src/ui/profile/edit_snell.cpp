#include "include/ui/profile/edit_snell.h"

#include "include/ui/utils/GuiUtils.hpp"
#include "include/ui/utils/ValidationForm.h"

EditSnell::EditSnell(QWidget *parent) : QWidget(parent), ui(new Ui::EditSnell) {
    ui->setupUi(this);
    ui->version->addItems(Configs::snellVersions);
    ui->network->addItems(Configs::snellNetworks);
    ui->obfs_mode->addItems(Configs::snellObfsModes);
    ui->mode->addItems(Configs::snellV6Modes);
    connect(ui->version, &QComboBox::currentTextChanged, this, [this] { applyVersion(); });
}

EditSnell::~EditSnell() {
    delete ui;
}

// Obfuscation is v4-only and the shaping mode v6-only; the core rejects the other version's keys.
void EditSnell::applyVersion() {
    const bool v6 = ui->version->currentText() == "6";
    ui->obfs_mode_l->setVisible(!v6);
    ui->obfs_mode->setVisible(!v6);
    ui->obfs_host_l->setVisible(!v6);
    ui->obfs_host->setVisible(!v6);
    ui->mode_l->setVisible(v6);
    ui->mode->setVisible(v6);
}

void EditSnell::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Snell();

    ui->version->setCurrentText(QString::number(outbound->version));
    ui->psk->setText(outbound->psk);
    ui->userkey->setText(outbound->userkey);
    ui->network->setCurrentText(outbound->network);
    ui->obfs_mode->setCurrentText(outbound->obfs_mode);
    ui->obfs_host->setText(outbound->obfs_host);
    ui->mode->setCurrentText(outbound->mode);
    ui->reuse->setChecked(outbound->reuse);

    applyVersion();
}

bool EditSnell::onEnd() {
    auto outbound = this->ent->Snell();

    outbound->version = ui->version->currentText().trimmed().toInt();
    outbound->psk = ui->psk->text();
    outbound->userkey = ui->userkey->text();
    outbound->network = ui->network->currentText().trimmed();
    outbound->obfs_mode = ui->obfs_mode->currentText().trimmed();
    outbound->obfs_host = ui->obfs_host->text().trimmed();
    outbound->mode = ui->mode->currentText().trimmed();
    outbound->reuse = ui->reuse->isChecked();

    return true;
}

void EditSnell::bindValidation(Validation::Form &form) {
    form.bind(ui->psk, Validation::Rules::Required(), this).raw();
    form.bind(ui->userkey, [](const QString &text) { return Configs::Checks::SnellUserKey(text); }, this).raw();
    form.addRule({ui->obfs_host, ui->obfs_mode}, [this] {
        const QString mode = ui->obfs_mode->currentText();
        if (ui->obfs_host->text().trimmed().isEmpty() || mode == "http" || mode == "tls") return QString();
        return tr("Obfuscation Host is used only with http or tls obfuscation.");
    }, this, Validation::Severity::Warning);
}
