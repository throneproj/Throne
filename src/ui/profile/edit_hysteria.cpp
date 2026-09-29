#include "include/ui/profile/edit_hysteria.h"

#include "include/global/Utils.hpp"
#include "include/ui/profile/edit_hysteria_realm.h"

EditHysteria::EditHysteria(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditHysteria) {
    ui->setupUi(this);

    const auto updateLayout = [this] {
        editHysteriaLayout(ui->protocol_version->currentText(), ui->obfuscation_type->currentText());
        if (editor_state_changed) editor_state_changed();
    };
    connect(ui->protocol_version, &QComboBox::currentTextChanged, this, updateLayout);
    connect(ui->obfuscation_type, &QComboBox::currentTextChanged, this, updateLayout);
    connect(ui->realm_enabled, &QCheckBox::toggled, this, updateLayout);

    connect(ui->realm_options, &QPushButton::clicked, this, [this] {
        if (ent == nullptr) return;
        auto realm = new EditHysteriaRealm(this, ent);
        realm->exec();
        realm->deleteLater();
    });
}

EditHysteria::~EditHysteria() {
    delete ui;
}

void EditHysteria::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = _ent->Hysteria();

    ui->protocol_version->setCurrentText(outbound->protocol_version);
    ui->server_ports->setText(outbound->server_ports.join(","));
    ui->hop_interval->setText(outbound->hop_interval);
    ui->hop_interval_max->setText(outbound->hop_interval_max);
    ui->up_mbps->setText(Int2String(outbound->up_mbps));
    ui->down_mbps->setText(Int2String(outbound->down_mbps));
    ui->obfs->setText(outbound->obfs);
    ui->auth_type->setCurrentText(outbound->auth_type);
    ui->auth->setText(outbound->auth);
    ui->recv_window->setText(Int2String(outbound->recv_window));
    ui->recv_window_conn->setText(Int2String(outbound->recv_window_conn));
    ui->disable_mtu_discovery->setChecked(outbound->disable_mtu_discovery);
    ui->password->setText(outbound->password);
    ui->min_packet_size->setText(Int2String(outbound->min_packet_size));
    ui->max_packet_size->setText(Int2String(outbound->max_packet_size));
    ui->obfuscation_type->setCurrentText(outbound->obfs_type);
    ui->bbr_profile->setCurrentText(outbound->bbr_profile);
    ui->password->setText(outbound->password);
    ui->disable_chrome_parrot->setChecked(outbound->disable_chrome_parrot);
    ui->realm_enabled->setChecked(outbound->realm_enabled);
    editHysteriaLayout(outbound->protocol_version, outbound->obfs_type);
}

bool EditHysteria::onEnd() {
    auto outbound = ent->Hysteria();
    // The core only checks realm fields when it first dials, so a bad one passes validation and fails every connection.
    if (ui->realm_enabled->isChecked() && ui->protocol_version->currentText() == "2" &&
        (outbound->realm_server_url.isEmpty() || outbound->realm_id.isEmpty() ||
         outbound->realm_stun_servers.isEmpty())) {
        MessageBoxWarning(software_name, tr("Realm needs a URL, an ID and at least one STUN server."));
        return false;
    }

    outbound->protocol_version = ui->protocol_version->currentText().trimmed();
    outbound->server_ports = SplitAndTrim(ui->server_ports->text(), ",", false);
    outbound->hop_interval = ui->hop_interval->text().trimmed();
    outbound->hop_interval_max = ui->hop_interval_max->text().trimmed();
    outbound->up_mbps = ui->up_mbps->text().trimmed().toInt();
    outbound->down_mbps = ui->down_mbps->text().trimmed().toInt();
    outbound->obfs = ui->obfs->text();
    outbound->auth_type = ui->auth_type->currentText().trimmed();
    outbound->auth = ui->auth->text();
    outbound->recv_window = ui->recv_window->text().trimmed().toInt();
    outbound->recv_window_conn = ui->recv_window_conn->text().trimmed().toInt();
    outbound->disable_mtu_discovery = ui->disable_mtu_discovery->isChecked();
    outbound->password = ui->password->text();
    outbound->min_packet_size = ui->min_packet_size->text().trimmed().toInt();
    outbound->max_packet_size = ui->max_packet_size->text().trimmed().toInt();
    outbound->obfs_type = ui->obfuscation_type->currentText().trimmed();
    outbound->bbr_profile = ui->bbr_profile->currentText().trimmed();
    outbound->disable_chrome_parrot = ui->disable_chrome_parrot->isChecked();
    outbound->realm_enabled = ui->realm_enabled->isChecked();
    if (outbound->RealmActive()) {
        // The parent dialog copies its address/port over ours next, and the core refuses a realm outbound with a server.
        outbound->server_ports.clear();
        if (set_edit_text_serverAddress) set_edit_text_serverAddress("");
        if (set_edit_text_serverPort) set_edit_text_serverPort("");
    }
    return true;
}

bool EditHysteria::locksServerAddress() {
    return ui->realm_enabled->isChecked() && ui->protocol_version->currentText() == "2";
}

void EditHysteria::editHysteriaLayout(const QString& version, const QString& obfs_type) {
    if (version == "1")
    {
        ui->auth_type->setVisible(true);
        ui->auth_type_l->setVisible(true);
        ui->auth->setVisible(true);
        ui->auth_l->setVisible(true);
        ui->recv_window_conn->setVisible(true);
        ui->recv_window_conn_l->setVisible(true);
        ui->recv_window->setVisible(true);
        ui->recv_window_l->setVisible(true);
        ui->disable_mtu_discovery->setVisible(true);
        ui->password->setVisible(false);
        ui->password_l->setVisible(false);
        ui->min_packet_size->setVisible(false);
        ui->min_packet_size_l->setVisible(false);
        ui->max_packet_size->setVisible(false);
        ui->max_packet_size_l->setVisible(false);
        ui->obfuscation_type->setVisible(false);
        ui->obfuscation_type_l->setVisible(false);
        ui->hop_interval_max->setVisible(false);
        ui->hop_interval_max_l->setVisible(false);
        ui->bbr_profile->setVisible(false);
        ui->bbr_profile_l->setVisible(false);
    } else
    {
        ui->auth_type->setVisible(false);
        ui->auth_type_l->setVisible(false);
        ui->auth->setVisible(false);
        ui->auth_l->setVisible(false);
        ui->recv_window_conn->setVisible(false);
        ui->recv_window_conn_l->setVisible(false);
        ui->recv_window->setVisible(false);
        ui->recv_window_l->setVisible(false);
        ui->disable_mtu_discovery->setVisible(false);
        ui->password->setVisible(true);
        ui->password_l->setVisible(true);
        ui->obfuscation_type->setVisible(true);
        ui->obfuscation_type_l->setVisible(true);
        ui->hop_interval_max->setVisible(true);
        ui->hop_interval_max_l->setVisible(true);
        ui->bbr_profile->setVisible(true);
        ui->bbr_profile_l->setVisible(true);
        if (obfs_type == "gecko") {
            ui->min_packet_size->setVisible(true);
            ui->min_packet_size_l->setVisible(true);
            ui->max_packet_size->setVisible(true);
            ui->max_packet_size_l->setVisible(true);
        }
        if (obfs_type == "salamander") {
            ui->min_packet_size->setVisible(false);
            ui->min_packet_size_l->setVisible(false);
            ui->max_packet_size->setVisible(false);
            ui->max_packet_size_l->setVisible(false);
        }
    }

    const auto realm = version == "2" && ui->realm_enabled->isChecked();
    ui->disable_chrome_parrot->setVisible(version == "2");
    ui->realm_enabled->setVisible(version == "2");
    // Not gated on `realm`: the realm settings stay editable whether or not realm is switched on.
    ui->realm_options->setVisible(version == "2");

    // realm carries the endpoint itself; port hopping needs a server port range it forbids.
    ui->server_ports->setEnabled(!realm);
    ui->label->setEnabled(!realm);
    ui->hop_interval->setEnabled(!realm);
    ui->label_2->setEnabled(!realm);
    ui->hop_interval_max->setEnabled(!realm);
    ui->hop_interval_max_l->setEnabled(!realm);
}
