#include "include/ui/profile/edit_trusttunnel.h"

#include <QComboBox>
#include <QLineEdit>

#include "include/configs/common/utils.h"
#include "include/ui/utils/ValidationForm.h"

EditTrustTunnel::EditTrustTunnel(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::EditTrustTunnel) {

    ui->setupUi(this);
    connect(ui->quic, &QCheckBox::toggled, this, [this] {
        if (editor_state_changed) editor_state_changed();
    });
}

EditTrustTunnel::~EditTrustTunnel() {
    delete ui;
}

void EditTrustTunnel::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = ent->TrustTunnel();

    ui->username->setText(outbound->username);
    ui->password->setText(outbound->password);
    ui->custom_sni->setText(outbound->custom_sni);
    ui->client_random->setText(outbound->client_random);
    ui->health_check->setChecked(outbound->health_check);
    ui->quic->setChecked(outbound->quic);
    // Older builds saved translated item text, e.g. "CUBIC"; unknown values fall back to bbr, as in sing-trusttunnel.
    const auto congestion = outbound->congestion_control.trimmed().replace(' ', '_');
    ui->congestion_control->setCurrentIndex(qMax(0, ui->congestion_control->findText(congestion, Qt::MatchFixedString)));
}

bool EditTrustTunnel::onEnd() {
    auto outbound = ent->TrustTunnel();

    outbound->username = ui->username->text().trimmed();
    outbound->password = ui->password->text();
    outbound->custom_sni = ui->custom_sni->text().trimmed();
    outbound->client_random = ui->client_random->text().trimmed();
    outbound->health_check = ui->health_check->isChecked();
    outbound->quic = ui->quic->isChecked();
    outbound->congestion_control = ui->congestion_control->currentText().trimmed();
    return true;
}

bool EditTrustTunnel::usesQuic() {
    return ui->quic->isChecked();
}

void EditTrustTunnel::bindValidation(Validation::Form &form) {
    using namespace Validation;
    form.bind(ui->username, Rules::Required(), this);
    form.bind(ui->password, Rules::Required(), this).raw();
    form.bind(ui->custom_sni, Configs::Checks::ServerName, this);
    form.bind(ui->client_random, Configs::Checks::TrustTunnelClientRandom, this);
    form.addRule({ui->client_random, ui->quic}, [this] {
        return ui->client_random->text().trimmed().isEmpty() ? QString() : tlsConflict(true, false);
    }, this);
    form.addRule({ui->custom_sni, ui->quic}, [this] {
        return ui->custom_sni->text().trimmed().isEmpty() ? QString() : tlsConflict(false, true);
    }, this);
}

QString EditTrustTunnel::tlsConflict(bool clientRandom, bool customSni) const {
    const auto tls = ent->TrustTunnel()->tls;
    bool reality = tls->reality->enabled;
    QString fingerprint = tls->utls->enabled ? tls->utls->fingerPrint : QString();
    // The outer TLS rows are saved only after this editor, so read their current input.
    if (auto *dialog = get_edit_dialog ? get_edit_dialog() : nullptr) {
        if (const auto *pbk = dialog->findChild<QLineEdit *>("reality_pbk")) reality = pbk->isEnabled() && !pbk->text().trimmed().isEmpty();
        if (const auto *utls = dialog->findChild<QComboBox *>("utlsFingerprint")) fingerprint = utls->currentText().trimmed();
    }
    return Configs::Checks::TrustTunnelTlsConflict(clientRandom, customSni, ui->quic->isChecked(), tls->ech->enabled, reality,
                                                  tls->disable_sni, fingerprint);
}