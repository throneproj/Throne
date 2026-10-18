#include "include/ui/profile/edit_hysteria_realm.h"

#include <QGuiApplication>
#include <QScreen>

#include "include/ui/utils/GuiUtils.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

EditHysteriaRealm::EditHysteriaRealm(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent)
    : QDialog(parent)
    , ui(new Ui::EditHysteriaRealm)
{
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    // Presence is checked by the editor, and only while realm is switched on.
    form->bind(ui->realm_server_url, Validation::Rules::Url({"http", "https"}));
    form->bind(ui->realm_stun_servers, Validation::Rules::List(Validation::Split::Comma, Validation::Rules::HostPort(false)));
    form->bind(ui->realm_port_mapping_timeout, Validation::Rules::Duration());
    form->bind(ui->realm_port_mapping_lifetime, Validation::Rules::Duration());
    ent = _ent;
    auto outbound = ent->Hysteria();

    ui->realm_server_url->setText(outbound->realm_server_url);
    ui->realm_id->setText(outbound->realm_id);
    ui->realm_token->setText(outbound->realm_token);
    ui->realm_stun_servers->setText(outbound->realm_stun_servers.join(","));
    ui->realm_ip_version->setCurrentText(outbound->realm_ip_version > 0 ? Int2String(outbound->realm_ip_version) : QString());
    ui->realm_port_mapping->setChecked(outbound->realm_port_mapping);
    ui->realm_port_mapping_timeout->setText(outbound->realm_port_mapping_timeout);
    ui->realm_port_mapping_lifetime->setText(outbound->realm_port_mapping_lifetime);

    connect(ui->realm_ip_version, &QComboBox::currentTextChanged, this, [this](const QString &) { syncPortMappingFields(); });
    connect(ui->realm_port_mapping, &QCheckBox::toggled, this, [this](bool) { syncPortMappingFields(); });
    syncPortMappingFields();

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    ADD_ASTERISK(this)
    form->markAll();

    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    if (scr != nullptr) resize(sizeHint().boundedTo(scr->availableGeometry().size()));
}

EditHysteriaRealm::~EditHysteriaRealm()
{
    delete ui;
}

void EditHysteriaRealm::syncPortMappingFields() {
    // Gateway mappings are IPv4-only, so the core rejects them alongside "ip_version": 6.
    const auto available = ui->realm_ip_version->currentText() != "6";
    ui->realm_port_mapping->setEnabled(available);
    const auto timing = available && ui->realm_port_mapping->isChecked();
    ui->realm_port_mapping_timeout->setEnabled(timing);
    ui->realm_port_mapping_timeout_l->setEnabled(timing);
    ui->realm_port_mapping_lifetime->setEnabled(timing);
    ui->realm_port_mapping_lifetime_l->setEnabled(timing);
}

void EditHysteriaRealm::accept() {
    if (!form->validateAll()) return;
    auto outbound = ent->Hysteria();
    outbound->realm_server_url = ui->realm_server_url->text().trimmed();
    outbound->realm_id = ui->realm_id->text().trimmed();
    outbound->realm_token = ui->realm_token->text();
    outbound->realm_stun_servers = SplitAndTrim(ui->realm_stun_servers->text(), ",", false);
    outbound->realm_ip_version = ui->realm_ip_version->currentText().trimmed().toInt();
    outbound->realm_port_mapping = ui->realm_port_mapping->isChecked();
    outbound->realm_port_mapping_timeout = ui->realm_port_mapping_timeout->text().trimmed();
    outbound->realm_port_mapping_lifetime = ui->realm_port_mapping_lifetime->text().trimmed();
    QDialog::accept();
}
