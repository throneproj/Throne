#include "include/ui/profile/edit_openconnect.h"

#include <QInputDialog>

#include "include/ui/profile/edit_openconnect_advanced.h"
#include "include/database/DatabaseManager.h"
#include "include/database/OtpProfilesRepo.h"
#include "include/global/OtpPlaceholder.hpp"
#include "include/global/Utils.hpp"
#include "include/configs/common/utils.h"
#include "include/ui/utils/ValidationForm.h"

EditOpenConnect::EditOpenConnect(QWidget *parent) : QWidget(parent), ui(new Ui::EditOpenConnect) {
    ui->setupUi(this);

    ui->flavor->addItems({"", "anyconnect", "gp", "fortinet", "f5", "pulse", "nc"});
    ui->tunnel_dns->addItem(tr("None"), QString(Configs::kTunnelDnsNone));
    ui->tunnel_dns->addItem(tr("Prefer"), QString(Configs::kTunnelDnsPrefer));
    ui->tunnel_dns->addItem(tr("Strict"), QString(Configs::kTunnelDnsStrict));

    ui->otp_profile_l->setToolTip(tr("<html><head/><body><p>Binds an authenticator entry to this profile. %1 in the "
                                     "username, password, software token or form entry fields is replaced with a "
                                     "generated code at connect time.</p></body></html>")
                                      .arg(Configs::kOtpPlaceholder));
    ui->password->setPlaceholderText(tr("%1 is replaced at connect time").arg(Configs::kOtpPlaceholder));

    connect(ui->certificate_authority, &QPushButton::clicked, this,
            [=, this] { editPem(ui->certificate_authority, tr("CA Certificate"), CACHE.certificateAuthority); });
    connect(ui->client_certificate, &QPushButton::clicked, this,
            [=, this] { editPem(ui->client_certificate, tr("Client Certificate"), CACHE.clientCertificate); });
    connect(ui->client_key, &QPushButton::clicked, this,
            [=, this] { editPem(ui->client_key, tr("Client Key"), CACHE.clientKey); });

    connect(ui->advanced_button, &QPushButton::clicked, this, [=, this] {
        if (ent == nullptr) return;
        auto advanced = new EditOpenConnectAdvanced(this, ent, [this](Configs::openconnect &outbound) { saveInto(outbound); });
        advanced->setAttribute(Qt::WA_DeleteOnClose);
        connect(advanced, &QDialog::accepted, this, [this] {
            revalidateInput();
            if (editor_state_changed) editor_state_changed();
        });
        advanced->show();
    });
}

EditOpenConnect::~EditOpenConnect() {
    delete ui;
}

void EditOpenConnect::editPem(QPushButton *button, const QString &title, QStringList &target) {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, title, "", target.join("\n"), &ok);
    if (!ok) return;
    target = txt.split("\n", Qt::SkipEmptyParts);
    if (editor_cache_updated) editor_cache_updated();
    else button->setText(target.isEmpty() ? tr("Not set") : tr("Already set"));
    revalidateInput();
}

QList<QPair<QPushButton *, QString>> EditOpenConnect::get_editor_cached() {
    return {
        {ui->certificate_authority, CACHE.certificateAuthority.join("\n")},
        {ui->client_certificate, CACHE.clientCertificate.join("\n")},
        {ui->client_key, CACHE.clientKey.join("\n")},
    };
}

void EditOpenConnect::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->OpenConnect();

    ui->flavor->setCurrentText(outbound->flavor);
    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
    ui->auth_group->setText(outbound->auth_group);
    ui->server_path->setText(outbound->server_path);
    ui->mtu->setText(outbound->mtu == 0 ? QString() : Int2String(outbound->mtu));
    ui->only_advertised_routes->setChecked(outbound->only_advertised_routes);
    auto tunnelDnsIndex = ui->tunnel_dns->findData(outbound->tunnel_dns);
    if (tunnelDnsIndex < 0) tunnelDnsIndex = ui->tunnel_dns->findData(QString(Configs::kTunnelDnsPrefer));
    ui->tunnel_dns->setCurrentIndex(tunnelDnsIndex);

    ui->otp_profile->addItem(tr("None"), -1);
    for (const auto &otp: Configs::dataManager->otpProfilesRepo->GetAllOtpProfiles()) {
        ui->otp_profile->addItem(otp->DisplayName(), otp->id);
    }
    auto otpIndex = ui->otp_profile->findData(outbound->otp_profile_id);
    ui->otp_profile->setCurrentIndex(otpIndex < 0 ? 0 : otpIndex);

    ui->insecure->setChecked(outbound->tls->insecure);
    ui->tls_server_name->setText(outbound->tls->server_name);
    ui->client_key_password->setText(outbound->tls->client_key_password);

    CACHE.certificateAuthority = outbound->tls->certificate_authority;
    CACHE.clientCertificate = outbound->tls->client_certificate;
    CACHE.clientKey = outbound->tls->client_key;
}

bool EditOpenConnect::onEnd() {
    saveInto(*ent->OpenConnect());
    return true;
}

void EditOpenConnect::saveInto(Configs::openconnect &outbound) const {
    outbound.flavor = ui->flavor->currentText().trimmed();
    outbound.username = ui->username->text().trimmed();
    outbound.password = ui->password->text();
    outbound.auth_group = ui->auth_group->text().trimmed();
    outbound.server_path = ui->server_path->text().trimmed();
    outbound.mtu = ui->mtu->text().trimmed().toInt();
    outbound.otp_profile_id = ui->otp_profile->currentData().toInt();
    outbound.only_advertised_routes = ui->only_advertised_routes->isChecked();
    outbound.tunnel_dns = ui->tunnel_dns->currentData().toString();

    outbound.tls->insecure = ui->insecure->isChecked();
    outbound.tls->server_name = ui->tls_server_name->text().trimmed();
    outbound.tls->certificate_authority = CACHE.certificateAuthority;
    outbound.tls->client_certificate = CACHE.clientCertificate;
    outbound.tls->client_key = CACHE.clientKey;
    outbound.tls->client_key_password = ui->client_key_password->text();
}

std::shared_ptr<Configs::openconnect> EditOpenConnect::withInput() const {
    auto outbound = ent->OpenConnect()->DetachedCopy();
    saveInto(*outbound);
    return outbound;
}

void EditOpenConnect::revalidateInput() {
    if (!form) return;
    for (auto *widget: std::as_const(checkedWidgets)) form->revalidate(widget);
}

void EditOpenConnect::bindValidation(Validation::Form &form) {
    using namespace Validation;
    using namespace Configs::Checks;
    using Check = std::function<QString(const Configs::openconnect &)>;
    this->form = &form;
    form.bind(ui->server_path, OpenConnectServerPath, this);
    form.bind(ui->mtu, Rules::Int(0, 65535), this);
    form.bind(ui->mtu, OpenConnectMtuFloor, this).warning();
    form.bind(ui->tls_server_name, ServerName, this);

    // The Advanced dialog holds the file paths, the token and the form entries, already applied to the profile.
    const auto rule = [this, &form](const QList<QWidget *> &participants, const Check &check) {
        form.addRule(participants, [this, check] { return check(*withInput()); }, this);
        for (auto *widget: participants) {
            if (!checkedWidgets.contains(widget)) checkedWidgets << widget;
        }
    };
    rule({ui->certificate_authority}, [](const Configs::openconnect &o) {
        return VpnMaterialConflict(o.tls->certificate_authority, o.tls->certificate_authority_path);
    });
    rule({ui->client_certificate}, [](const Configs::openconnect &o) {
        return VpnMaterialConflict(o.tls->client_certificate, o.tls->client_certificate_path);
    });
    rule({ui->client_key}, [](const Configs::openconnect &o) { return VpnMaterialConflict(o.tls->client_key, o.tls->client_key_path); });
    rule({ui->client_certificate, ui->client_key}, [](const Configs::openconnect &o) {
        return OpenConnectCertificatePair(o.tls->client_certificate, o.tls->client_certificate_path, o.tls->client_key, o.tls->client_key_path);
    });
    rule({ui->flavor}, [](const Configs::openconnect &o) {
        if (OpenConnectReportedOs(o.flavor, o.reported_os).isEmpty()) return QString();
        return tr("This protocol does not accept the reported OS \"%1\" set under Advanced.").arg(o.reported_os);
    });
    rule({ui->username, ui->otp_profile}, [](const Configs::openconnect &o) { return OtpPlaceholderUnbound(o.username, o.otp_profile_id); });
    rule({ui->password, ui->otp_profile}, [](const Configs::openconnect &o) { return OtpPlaceholderUnbound(o.password, o.otp_profile_id); });
    rule({ui->otp_profile}, [](const Configs::openconnect &o) {
        QStringList advanced = {o.token->pin, o.token->password};
        for (const auto &entry: o.form_entries) advanced << entry->value;
        for (const auto &value: advanced) {
            if (!OtpPlaceholderUnbound(value, o.otp_profile_id).isEmpty()) {
                return tr("The token or form entries under Advanced use %1; select an authenticator entry.").arg(Configs::kOtpPlaceholder);
            }
        }
        return QString();
    });
}

bool EditOpenConnect::serverPortOptional() {
    return true;
}
