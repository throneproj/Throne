#include "include/ui/profile/edit_advanced.h"

#include <QInputDialog>

EditAdvanced::EditAdvanced(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent)
    : QDialog(parent)
    , ui(new Ui::EditAdvanced)
{
    ui->setupUi(this);
    ent = _ent;
    auto dialFieldsObj = ent->outbound->dialFields;
    ui->reuse_addr->setChecked(dialFieldsObj->reuse_addr);
    ui->tcp_fast_open->setChecked(dialFieldsObj->tcp_fast_open);
    ui->udp_fragment->setChecked(dialFieldsObj->udp_fragment);
    ui->tcp_multipath->setChecked(dialFieldsObj->tcp_multi_path);
    ui->connect_timeout->setText(dialFieldsObj->connect_timeout);

    if (ent->outbound->HasTLS()) {
        auto tlsObj = ent->outbound->GetTLS();
        ui->disable_sni->setChecked(tlsObj->disable_sni);
        ui->min_version->setText(tlsObj->min_version);
        ui->max_version->setText(tlsObj->max_version);
        ui->enable_ech->setChecked(tlsObj->ech->enabled);

        // TODO enable when sing-box version is >= 1.13
        ui->cert_sha256->setEnabled(false);
        ui->client_cert->setEnabled(false);
        ui->client_key->setEnabled(false);
        if (!tlsObj->ech->config.isEmpty()) {
            ui->ech_config->setText("Already set");
            CACHE.echConfig = tlsObj->ech->config;
        }
        if (!tlsObj->certificate_public_key_sha256.isEmpty()) {
            ui->cert_sha256->setText("Already set");
            CACHE.certSha256 = tlsObj->certificate_public_key_sha256;
        }
        if (!tlsObj->client_certificate.isEmpty()) {
            ui->client_cert->setText("Already set");
            CACHE.clientCert = tlsObj->client_certificate;
        }
        if (!tlsObj->client_key.isEmpty()) {
            ui->client_key->setText("Already set");
            CACHE.clientKey = tlsObj->client_key;
        }
    } else {
        ui->tls_box->hide();
        adjustSize();
    }
}

EditAdvanced::~EditAdvanced()
{
    delete ui;
}

void EditAdvanced::accept() {
    auto dialFieldsObj = ent->outbound->dialFields;
    dialFieldsObj->reuse_addr = ui->reuse_addr->isChecked();
    dialFieldsObj->tcp_fast_open = ui->tcp_fast_open->isChecked();
    dialFieldsObj->udp_fragment = ui->udp_fragment->isChecked();
    dialFieldsObj->tcp_multi_path = ui->tcp_multipath->isChecked();
    dialFieldsObj->connect_timeout = ui->connect_timeout->text();

    if (ent->outbound->HasTLS()) {
        auto tlsObj = ent->outbound->GetTLS();
        tlsObj->disable_sni = ui->disable_sni->isChecked();
        tlsObj->min_version = ui->min_version->text();
        tlsObj->max_version = ui->max_version->text();
        tlsObj->ech->enabled = ui->enable_ech->isChecked();
        tlsObj->ech->config = CACHE.echConfig;
        tlsObj->client_certificate = CACHE.clientCert;
        tlsObj->client_key = CACHE.clientKey;
        tlsObj->certificate_public_key_sha256 = CACHE.certSha256;
    }
    QDialog::accept();
}

void EditAdvanced::on_ech_config_clicked() {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, tr("ECH Config"), "", CACHE.echConfig.join("\n"), &ok);
    if (ok) {
        CACHE.echConfig = txt.split("\n", Qt::SkipEmptyParts);
        if (!CACHE.echConfig.isEmpty()) {
            ui->ech_config->setText("Already set");
        } else {
            ui->ech_config->setText("Not Set");
        }
    }
}

void EditAdvanced::on_client_cert_clicked() {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, tr("Client Certificate"), "", CACHE.clientCert.join("\n"), &ok);
    if (ok) {
        CACHE.clientCert = txt.split("\n", Qt::SkipEmptyParts);
        if (!CACHE.echConfig.isEmpty()) {
            ui->client_cert->setText("Already set");
        } else {
            ui->client_cert->setText("Not Set");
        }
    }
}

void EditAdvanced::on_client_key_clicked() {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, tr("Client Key"), "", CACHE.clientKey.join("\n"), &ok);
    if (ok) {
        CACHE.clientKey = txt.split("\n", Qt::SkipEmptyParts);
        if (!CACHE.echConfig.isEmpty()) {
            ui->client_key->setText("Already set");
        } else {
            ui->client_key->setText("Not Set");
        }
    }
}

void EditAdvanced::on_cert_sha256_clicked() {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, tr("Certificate sha256"), "", CACHE.certSha256.join("\n"), &ok);
    if (ok) {
        CACHE.certSha256 = txt.split("\n", Qt::SkipEmptyParts);
        if (!CACHE.echConfig.isEmpty()) {
            ui->cert_sha256->setText("Already set");
        } else {
            ui->cert_sha256->setText("Not Set");
        }
    }
}