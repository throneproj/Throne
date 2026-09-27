#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/common/utils.h"
#include "include/database/DatabaseManager.h"
#include "include/global/Utils.hpp"

#include <QInputDialog>

namespace {
// Index of "Off" in the Keep Default / On / Off combos.
constexpr int kTriStateOff = 2;
}

void DialogEditProfile::setupSingboxStream() {
    network_title_base = ui->network_box->title();
    ui->utlsFingerprint->addItems(Configs::tlsFingerprints);

    connect(ui->network, &QComboBox::currentTextChanged, this, [this] { relayout(); });
    connect(ui->security, &QComboBox::currentTextChanged, this, [this] { relayout(); });
    connect(ui->fragment, &QComboBox::currentIndexChanged, this, [this] { updateControls(); });
    connect(ui->multiplex, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index == kTriStateOff) ui->brutal_enable->setChecked(false);
        updateControls();
    });
}

void DialogEditProfile::loadSingboxStream() {
    const auto outbound = ent->outbound;
    if (outbound->HasTLS() || outbound->HasTransport()) {
        const auto transport = outbound->GetTransport();
        selectComboText(ui->network, transport->type);
        ui->headers->setText(Configs::getHeadersString(transport->headers));
        ui->method->setText(transport->method);
        ui->path->setText(transport->path);
        ui->host->setText(transport->host);
        ui->ws_early_data_length->setText(Int2String(transport->max_early_data));
        ui->ws_early_data_name->setText(transport->early_data_header_name);
        ui->service_name->setText(transport->service_name);

        const auto tls = outbound->GetTLS();
        ui->security->setCurrentText((outbound->MustTLS() || tls->enabled) ? "tls" : "");
        ui->insecure->setChecked(tls->insecure);
        CACHE.certificate = tls->certificate;
        ui->sni->setText(tls->server_name);
        ui->alpn->setText(tls->alpn.join(","));
        ui->fragment->setCurrentIndex(tls->getFragmentState());
        ui->tls_frag_fall_delay->setText(tls->fragment_fallback_delay);
        ui->tls_rec_frag->setChecked(tls->record_fragment);
        ui->tls_tricks->setCurrentIndex(tls->getTlsTricksState());
        if (!tls->utls->supported || outbound->LimitedTLS()) {
            ui->utlsFingerprint->setCurrentText("");
        } else if (newEnt) {
            ui->utlsFingerprint->setCurrentText(Configs::dataManager->settingsRepo->utlsFingerprint);
        } else {
            ui->utlsFingerprint->setCurrentText(tls->utls->fingerPrint);
        }
        ui->reality_pbk->setText(tls->reality->public_key);
        ui->reality_sid->setText(tls->reality->short_id);
    }
    if (outbound->HasMux()) {
        const auto mux = outbound->GetMux();
        // Before brutal_enable: selecting Off unchecks it.
        ui->multiplex->setCurrentIndex(mux->getMuxState());
        ui->brutal_enable->setChecked(mux->brutal->enabled);
        ui->brutal_d_speed->setText(Int2String(mux->brutal->down_mbps));
        ui->brutal_u_speed->setText(Int2String(mux->brutal->up_mbps));
    }
}

bool DialogEditProfile::validateSingboxStream() {
    // "|" separates headers in exported links.
    if (!ent->outbound->HasTransport() || !ui->headers->text().contains('|')) return true;
    MessageBoxWarning(software_name, tr("Headers cannot contain \"|\"."));
    return false;
}

void DialogEditProfile::saveSingboxStream() {
    const auto outbound = ent->outbound;
    if (outbound->HasTLS() || outbound->HasTransport()) {
        auto transport = outbound->GetTransport();
        transport->type = ui->network->currentText().trimmed();
        transport->headers = Configs::parseHeaderPairs(ui->headers->text());
        transport->method = ui->method->text().trimmed();
        transport->path = ui->path->text().trimmed();
        transport->host = ui->host->text().trimmed();
        transport->max_early_data = ui->ws_early_data_length->text().trimmed().toInt();
        transport->early_data_header_name = ui->ws_early_data_name->text().trimmed();
        transport->service_name = ui->service_name->text().trimmed();

        auto tls = outbound->GetTLS();
        tls->enabled = ui->security->currentText() == "tls";
        tls->insecure = ui->insecure->isChecked();
        tls->certificate = CACHE.certificate;
        tls->server_name = ui->sni->text().trimmed();
        tls->alpn = SplitAndTrim(ui->alpn->text(), ",", false);
        tls->saveFragmentState(ui->fragment->currentIndex());
        tls->fragment_fallback_delay = ui->tls_frag_fall_delay->text().trimmed();
        tls->record_fragment = ui->tls_rec_frag->isChecked();
        tls->saveTlsTricksState(ui->tls_tricks->currentIndex());
        tls->utls->fingerPrint = ui->utlsFingerprint->currentText().trimmed();
        tls->utls->enabled = !tls->utls->fingerPrint.isEmpty();
        tls->reality->public_key = ui->reality_pbk->text().trimmed();
        tls->reality->short_id = ui->reality_sid->text().trimmed();
        tls->reality->enabled = !tls->reality->public_key.isEmpty();
    }
    if (outbound->HasMux()) {
        auto mux = outbound->GetMux();
        mux->saveMuxState(ui->multiplex->currentIndex());
        mux->brutal->enabled = ui->brutal_enable->isChecked();
        mux->brutal->down_mbps = ui->brutal_d_speed->text().trimmed().toInt();
        mux->brutal->up_mbps = ui->brutal_u_speed->text().trimmed().toInt();
    }
}

void DialogEditProfile::updateSingboxRows() {
    const auto outbound = ent->outbound;
    const bool hasTransport = outbound->HasTransport();
    const bool hasTLS = outbound->HasTLS();
    const bool hasMux = outbound->HasMux();

    setRowVisible(ui->network_l, ui->network, hasTransport);
    setRowVisible(ui->security_l, ui->security, hasTLS);
    setRowVisible(ui->multiplex_l, ui->multiplex, hasMux);
    ui->security->setEnabled(!outbound->MustTLS());
    ui->multiplex->setEnabled(!innerEditor->blocksMultiplex());
    ui->brutal_box->setVisible(hasMux);
    ui->brutal_box->setEnabled(ui->multiplex->currentIndex() != kTriStateOff);
    ui->stream_box->setVisible(hasTransport || hasTLS || hasMux);

    const auto network = ui->network->currentText();
    const bool ws = network == "ws";
    const bool http = network == "http";
    const bool grpc = network == "grpc";
    const bool httpLike = ws || http || network == "httpupgrade";
    setRowVisible(ui->headers_l, ui->headers, httpLike);
    setRowVisible(ui->method_l, ui->method, http);
    setRowVisible(ui->path_l, ui->path, httpLike);
    setRowVisible(ui->host_l, ui->host, httpLike);
    setRowVisible(ui->ws_early_data_length_l, ui->ws_early_data_length, ws);
    setRowVisible(ui->ws_early_data_name_l, ui->ws_early_data_name, ws);
    setRowVisible(ui->service_name_l, ui->service_name, grpc);
    const bool networkBox = hasTransport && (httpLike || grpc);
    ui->network_box->setTitle(network_title_base.arg(network));
    ui->network_box->setVisible(networkBox);

    const bool tls = hasTLS && ui->security->currentText() == "tls";
    ui->security_box->setVisible(tls);
    ui->tls_camouflage_box->setVisible(tls);
    ui->right_all_w->setVisible(networkBox || tls);
}

void DialogEditProfile::updateTlsControlsEnabled() {
    const auto outbound = ent->outbound;
    const bool limited = outbound->LimitedTLS();
    // QUIC dials through qtls, which cannot use a uTLS or Reality config.
    const bool quic = innerEditor->usesQuic();
    const bool utls = !limited && !quic && outbound->GetTLS()->utls->supported;
    const bool reality = !limited && !quic && ent->type != "masque";
    // Limited-TLS outbounds only get the dialer-level ("custom") fragment, which has no fallback delay.
    const bool fragment = !limited || Configs::dataManager->settingsRepo->fragment_implementation == "custom";

    for (QWidget *w: std::initializer_list<QWidget *>{ui->alpn_l, ui->alpn, ui->insecure, ui->tls_rec_frag,
                                                     ui->tls_tricks_l, ui->tls_tricks, ui->tls_frag_fall_delay_l}) {
        w->setEnabled(!limited);
    }
    for (QWidget *w: std::initializer_list<QWidget *>{ui->reality_pbk_l, ui->reality_pbk, ui->reality_sid_l, ui->reality_sid}) {
        w->setEnabled(reality);
    }
    ui->utlsFingerprint_l->setEnabled(utls);
    ui->utlsFingerprint->setEnabled(utls);
    ui->fragment_l->setEnabled(fragment);
    ui->fragment->setEnabled(fragment);
    ui->tls_frag_fall_delay->setEnabled(!limited && ui->fragment->currentIndex() != kTriStateOff);
}

void DialogEditProfile::on_certificate_edit_clicked() {
    bool ok;
    const auto txt = QInputDialog::getMultiLineText(this, tr("Certificate"), "", CACHE.certificate.join("\n"), &ok);
    if (!ok) return;
    CACHE.certificate = txt.split("\n", Qt::SkipEmptyParts);
    editor_cache_updated_impl();
}
