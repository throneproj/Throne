#include "include/ui/profile/edit_openvpn.h"

#include <QInputDialog>

#include "include/ui/profile/edit_openvpn_advanced.h"
#include "include/database/DatabaseManager.h"
#include "include/database/OtpProfilesRepo.h"
#include "include/global/OtpPlaceholder.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

EditOpenVPN::EditOpenVPN(QWidget *parent) : QWidget(parent), ui(new Ui::EditOpenVPN) {
    ui->setupUi(this);

    ui->network->addItems(Configs::openvpnNetworks);
    ui->control_wrap_type->addItems({"", "tls_auth", "tls_crypt", "tls_crypt_v2"});
    ui->control_wrap_direction->addItems({"", "server", "client"});
    ui->tunnel_dns->addItem(tr("None"), QString(Configs::kTunnelDnsNone));
    ui->tunnel_dns->addItem(tr("Prefer"), QString(Configs::kTunnelDnsPrefer));
    ui->tunnel_dns->addItem(tr("Strict"), QString(Configs::kTunnelDnsStrict));

    ui->otp_profile_l->setToolTip(tr("<html><head/><body><p>Binds an authenticator entry to this profile. %1 in the "
                                     "username or password is replaced with a generated code at connect time, and a "
                                     "static challenge is answered with the same code.</p></body></html>")
                                      .arg(Configs::kOtpPlaceholder));
    ui->password->setPlaceholderText(tr("%1 is replaced at connect time").arg(Configs::kOtpPlaceholder));

    connect(ui->certificate, &QPushButton::clicked, this,
            [=, this] { editPem(ui->certificate, tr("CA Certificate"), CACHE.certificate); });
    connect(ui->client_certificate, &QPushButton::clicked, this,
            [=, this] { editPem(ui->client_certificate, tr("Client Certificate"), CACHE.clientCertificate); });
    connect(ui->client_key, &QPushButton::clicked, this,
            [=, this] { editPem(ui->client_key, tr("Client Key"), CACHE.clientKey); });
    connect(ui->control_wrap_key, &QPushButton::clicked, this,
            [=, this] { editPem(ui->control_wrap_key, tr("Control Channel Wrap Key"), CACHE.controlWrapKey); });

    connect(ui->advanced_button, &QPushButton::clicked, this, [=, this] {
        if (ent == nullptr) return;
        auto advanced = new EditOpenVPNAdvanced(this, ent, [this](Configs::openvpn &outbound) { saveInto(outbound); });
        advanced->setAttribute(Qt::WA_DeleteOnClose);
        // The server list it writes decides whether the outer address and port are needed.
        connect(advanced, &QDialog::accepted, this, [this] {
            revalidateInput();
            if (editor_state_changed) editor_state_changed();
        });
        advanced->show();
    });
}

EditOpenVPN::~EditOpenVPN() {
    delete ui;
}

void EditOpenVPN::editPem(QPushButton *button, const QString &title, QStringList &target) {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, title, "", target.join("\n"), &ok);
    if (!ok) return;
    target = txt.split("\n", Qt::SkipEmptyParts);
    if (editor_cache_updated) editor_cache_updated();
    else button->setText(target.isEmpty() ? tr("Not set") : tr("Already set"));
    revalidateInput();
}

QList<QPair<QPushButton *, QString>> EditOpenVPN::get_editor_cached() {
    return {
        {ui->certificate, CACHE.certificate.join("\n")},
        {ui->client_certificate, CACHE.clientCertificate.join("\n")},
        {ui->client_key, CACHE.clientKey.join("\n")},
        {ui->control_wrap_key, CACHE.controlWrapKey.join("\n")},
    };
}

void EditOpenVPN::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->OpenVPN();

    ui->network->setCurrentText(outbound->network);
    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
    ui->static_challenge->setText(outbound->static_challenge);
    ui->static_challenge_echo->setChecked(outbound->static_challenge_echo);
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

    ui->control_wrap_type->setCurrentText(outbound->tls->control_wrap->type);
    ui->control_wrap_direction->setCurrentText(outbound->tls->control_wrap->direction);

    CACHE.certificate = outbound->tls->certificate;
    CACHE.clientCertificate = outbound->tls->client_certificate;
    CACHE.clientKey = outbound->tls->client_key;
    CACHE.controlWrapKey = outbound->tls->control_wrap->key;
}

bool EditOpenVPN::onEnd() {
    saveInto(*ent->OpenVPN());
    return true;
}

void EditOpenVPN::saveInto(Configs::openvpn &outbound) const {
    outbound.network = ui->network->currentText().trimmed();
    outbound.username = ui->username->text().trimmed();
    outbound.password = ui->password->text();
    outbound.static_challenge = ui->static_challenge->text();
    outbound.static_challenge_echo = ui->static_challenge_echo->isChecked();
    outbound.mtu = ui->mtu->text().trimmed().toInt();
    outbound.otp_profile_id = ui->otp_profile->currentData().toInt();
    outbound.only_advertised_routes = ui->only_advertised_routes->isChecked();
    outbound.tunnel_dns = ui->tunnel_dns->currentData().toString();

    outbound.tls->certificate = CACHE.certificate;
    outbound.tls->client_certificate = CACHE.clientCertificate;
    outbound.tls->client_key = CACHE.clientKey;
    outbound.tls->control_wrap->type = ui->control_wrap_type->currentText().trimmed();
    outbound.tls->control_wrap->key = CACHE.controlWrapKey;
    outbound.tls->control_wrap->direction = ui->control_wrap_direction->currentText().trimmed();
}

std::shared_ptr<Configs::openvpn> EditOpenVPN::withInput() const {
    auto outbound = ent->OpenVPN()->DetachedCopy();
    saveInto(*outbound);
    return outbound;
}

void EditOpenVPN::revalidateInput() {
    if (!form) return;
    for (auto *widget: std::as_const(checkedWidgets)) form->revalidate(widget);
}

void EditOpenVPN::bindValidation(Validation::Form &form) {
    using namespace Validation;
    using namespace Configs::Checks;
    using Check = std::function<QString(const Configs::openvpn &)>;
    this->form = &form;
    form.bind(ui->mtu, Rules::Int(0, 65535), this);

    // The Advanced dialog holds the mode, the file paths and the fingerprints, already applied to the profile.
    const auto rule = [this, &form](const QList<QWidget *> &participants, const Check &check, const QString &label = {}) {
        form.addRule(participants, [this, check] { return check(*withInput()); }, this, Severity::Error, label);
        for (auto *widget: participants) {
            if (!checkedWidgets.contains(widget)) checkedWidgets << widget;
        }
    };
    // The wrap rows are labelled just Type, Key and Direction inside their group.
    const auto wrapType = tr("Control Channel Wrap Type"), wrapKey = tr("Control Channel Wrap Key");
    rule({ui->certificate}, OpenVPNTlsVerification);
    rule({ui->client_certificate, ui->client_key}, OpenVPNClientPair);
    rule({ui->certificate}, [](const Configs::openvpn &o) { return VpnMaterialConflict(o.tls->certificate, o.tls->certificate_path); });
    rule({ui->client_certificate}, [](const Configs::openvpn &o) {
        return VpnMaterialConflict(o.tls->client_certificate, o.tls->client_certificate_path);
    });
    rule({ui->client_key}, [](const Configs::openvpn &o) { return VpnMaterialConflict(o.tls->client_key, o.tls->client_key_path); });
    rule({ui->control_wrap_key}, [](const Configs::openvpn &o) {
        return VpnMaterialConflict(o.tls->control_wrap->key, o.tls->control_wrap->key_path);
    }, wrapKey);
    rule({ui->control_wrap_type, ui->control_wrap_key}, OpenVPNControlWrapKey, wrapType);
    rule({ui->control_wrap_direction, ui->control_wrap_type}, OpenVPNControlWrapDirection, tr("Control Channel Wrap Direction"));

    const QList<QPair<QWidget *, QString>> modeBound = {
        {ui->username, "username"},
        {ui->password, "password"},
        {ui->static_challenge, "static_challenge"},
        {ui->static_challenge_echo, "static_challenge_echo"},
        {ui->certificate, "tls.certificate"},
        {ui->client_certificate, "tls.client_certificate"},
        {ui->client_key, "tls.client_key"},
    };
    for (const auto &bound: modeBound) {
        rule({bound.first}, [key = bound.second](const Configs::openvpn &o) { return OpenVPNModeConflict(o, key); });
    }
    rule({ui->control_wrap_type}, [](const Configs::openvpn &o) { return OpenVPNModeConflict(o, "tls.control_wrap"); }, wrapType);

    rule({ui->username, ui->otp_profile}, [](const Configs::openvpn &o) { return OtpPlaceholderUnbound(o.username, o.otp_profile_id); });
    rule({ui->password, ui->otp_profile}, [](const Configs::openvpn &o) { return OtpPlaceholderUnbound(o.password, o.otp_profile_id); });
}

bool EditOpenVPN::serverPortOptional() {
    return ent != nullptr && !ent->OpenVPN()->servers.isEmpty();
}

bool EditOpenVPN::serverAddressOptional() {
    return serverPortOptional();
}
