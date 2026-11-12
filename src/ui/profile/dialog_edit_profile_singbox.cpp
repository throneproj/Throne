#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/common/utils.h"
#include "include/database/DatabaseManager.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"
#include "include/ui/widget/MuxSettingsWidget.h"

#include <QInputDialog>

#include <climits>

void DialogEditProfile::setupSingboxStream() {
    network_title_base = ui->network_box->title();
    ui->utlsFingerprint->addItems(Configs::tlsFingerprints);

    connect(ui->network, &QComboBox::currentTextChanged, this, [this] { relayout(); });
    connect(ui->security, &QComboBox::currentTextChanged, this, [this] { relayout(); });
    connect(ui->fragment, &QComboBox::currentIndexChanged, this, [this] { relayout(); });
    connect(ui->multiplex, &QComboBox::currentIndexChanged, this, [this] { relayout(); });
    connect(ui->multiplex_custom, &QPushButton::clicked, this, [this] {
        MuxSettingsWidget::openDialog(this, CACHE.mux, [this](const Configs::MuxSettings &settings) { CACHE.mux = settings; });
    });
    bindSingboxStream();
}

QString DialogEditProfile::ownTlsSpoof() {
    if (ent == nullptr || !ent->outbound->HasTLS() || ent->outbound->LimitedTLS()) return {};
    const auto tls = ent->outbound->GetTLS();
    return Configs::Checks::OwnSpoofOn(tls->spoof, tls->getSpoofState()) ? tls->spoof.trimmed() : QString();
}

bool DialogEditProfile::singboxTlsFragmentOn() {
    if (ent == nullptr || ent->outbound->LimitedTLS()) return false;
    const auto &settings = *Configs::dataManager->settingsRepo;
    const int state = ui->fragment->currentIndex();
    // Build emits the TLS-level fragment (and its delay) only for the non-custom implementation.
    return (state == 1 || (state == 0 && settings.fragment_default_on)) && settings.fragment_implementation != "custom";
}

bool DialogEditProfile::singboxUtlsActive() {
    if (ent == nullptr || ent->outbound->LimitedTLS()) return false;
    if (!ui->reality_pbk->text().trimmed().isEmpty()) return true;
    if (!ent->outbound->GetTLS()->utls->supported) return false;
    auto fingerprint = ui->utlsFingerprint->currentText().trimmed();
    if (fingerprint.isEmpty()) fingerprint = Configs::dataManager->settingsRepo->utlsFingerprint.trimmed();
    return !fingerprint.isEmpty() && fingerprint != Configs::tlsUnsafeFingerprint;
}

void DialogEditProfile::bindSingboxStream() {
    using Validation::Severity;
    namespace Checks = Configs::Checks;

    form->addRule({ui->network}, [this] { return Checks::TransportType(ui->network->currentText()); });
    form->bind(ui->headers, Checks::HeaderPairs);
    form->bind(ui->method, Validation::Rules::HttpToken()).warning();
    form->bind(ui->host, [this](const QString &text) { return Checks::TransportHost(text, ui->network->currentText()); }).warning();
    form->bind(ui->ws_early_data_length, Validation::Rules::Int(0, INT_MAX));
    form->addRule({ui->network, ui->security}, [this] {
        if (ui->network->currentText() != "quic" || !ent->outbound->HasTLS()) return QString();
        return Checks::QuicTransport(ui->security->currentText() == "tls", false, false);
    });
    const auto quicOwnUtls = [this] {
        const auto fingerprint = ui->utlsFingerprint->currentText().trimmed();
        return ui->utlsFingerprint->isEnabled() && !fingerprint.isEmpty() && fingerprint != Configs::tlsUnsafeFingerprint;
    };
    const auto quicReality = [this] { return ui->reality_pbk->isEnabled() && !ui->reality_pbk->text().trimmed().isEmpty(); };
    form->addRule({ui->network, ui->utlsFingerprint, ui->reality_pbk}, [this, quicOwnUtls, quicReality] {
        if (ui->network->currentText() != "quic" || ui->security->currentText() != "tls") return QString();
        return Checks::QuicTransport(true, quicOwnUtls(), quicReality());
    });
    form->addRule({ui->network, ui->utlsFingerprint, ui->reality_pbk}, [this, quicOwnUtls, quicReality] {
        if (ui->network->currentText() != "quic" || ui->security->currentText() != "tls" || quicReality()) return QString();
        return Checks::QuicTransportUtls(quicOwnUtls());
    }, nullptr, Severity::Warning);

    form->bind(ui->sni, Checks::ServerName).warning();
    form->bind(ui->utlsFingerprint, Checks::SingBoxFingerprint);
    form->bind(ui->cipher_suites, Checks::SingBoxCipherSuites);
    form->bind(ui->reality_pbk, [](const QString &text) { return Checks::RealityPublicKey(text); });
    form->bind(ui->reality_sid, [this](const QString &text) {
        return ui->reality_pbk->text().trimmed().isEmpty() ? QString() : Checks::RealityShortId(text);
    });
    form->addRule({ui->reality_sid}, [this] {
        if (ui->reality_sid->text().trimmed().isEmpty() || !ui->reality_pbk->text().trimmed().isEmpty()) return QString();
        return tr("The short ID has no effect without a Reality public key, so Reality stays off.");
    }, nullptr, Severity::Warning);
    form->bind(ui->tls_frag_fall_delay, Validation::Rules::Duration()).when([this] { return singboxTlsFragmentOn(); });
    form->addRule({ui->certificate_edit}, [this] { return Checks::PemCertificates(CACHE.certificate); });

    // Rules over values the Advanced dialog keeps in the profile.
    form->addRule({ui->reality_pbk}, [this] {
        if (ui->reality_pbk->text().trimmed().isEmpty()) return QString();
        const auto tls = ent->outbound->GetTLS();
        return Checks::RealityConflict(tls->ech->enabled, tls->disable_sni, ui->insecure->isChecked(), !ownTlsSpoof().isEmpty());
    });
    form->addRule({ui->certificate_edit}, [this] {
        const auto tls = ent->outbound->GetTLS();
        return Checks::PinsWithCertificate(!tls->certificate_sha256.isEmpty() || !tls->certificate_public_key_sha256.isEmpty(),
                                           !CACHE.certificate.isEmpty() || !tls->certificate_path.isEmpty());
    });
    form->addRule({ui->sni}, [this] {
        // With Reality the Reality rule names the spoof conflict.
        if (ui->reality_pbk->isEnabled() && !ui->reality_pbk->text().trimmed().isEmpty()) return QString();
        const auto sni = ui->sni->text().trimmed();
        return Checks::SpoofServerName(ownTlsSpoof(), sni.isEmpty() ? ui->address->text().remove(' ') : sni,
                                       ent->outbound->GetTLS()->disable_sni);
    });
    form->addRule({ui->utlsFingerprint, ui->fragment, ui->tls_rec_frag}, [this] {
        return Checks::SpoofIgnored(!ownTlsSpoof().isEmpty(), singboxUtlsActive(), singboxTlsFragmentOn(), ui->tls_rec_frag->isChecked());
    }, nullptr, Severity::Warning);
}

void DialogEditProfile::loadSingboxStream() {
    const auto outbound = ent->outbound;
    if (outbound->HasTLS() || outbound->HasTransport()) {
        const auto transport = outbound->GetTransport();
        // Shown (and flagged) as stored, rather than as tcp and silently rewritten on save.
        if (!transport->type.isEmpty() && ui->network->findText(transport->type) < 0) ui->network->addItem(transport->type);
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
        ui->cipher_suites->setText(tls->cipher_suites.join(':'));
        ui->reality_pbk->setText(tls->reality->public_key);
        ui->reality_sid->setText(tls->reality->short_id);
    }
    if (outbound->HasMux()) {
        const auto mux = outbound->GetMux();
        ui->multiplex->setCurrentIndex(mux->getMuxState());
        offerCustomMux(ui->multiplex, Configs::kMuxCustom);
        CACHE.mux = mux->CustomSettings();
    }
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
        tls->cipher_suites = Configs::splitCipherSuites(ui->cipher_suites->text());
        tls->reality->public_key = ui->reality_pbk->text().trimmed();
        tls->reality->short_id = ui->reality_sid->text().trimmed();
        tls->reality->enabled = !tls->reality->public_key.isEmpty();
    }
    if (outbound->HasMux()) {
        auto mux = outbound->GetMux();
        const int state = ui->multiplex->currentIndex();
        mux->saveMuxState(state);
        if (state == Configs::kMuxCustom) mux->SetCustomSettings(CACHE.mux);
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
    ui->multiplex_custom->setVisible(hasMux && ui->multiplex->currentIndex() == Configs::kMuxCustom);
    ui->security->setEnabled(!outbound->MustTLS());
    const bool muxEditable = !innerEditor->blocksMultiplex();
    ui->multiplex->setEnabled(muxEditable);
    ui->multiplex_custom->setEnabled(muxEditable);
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
    // QUIC is TLS 1.3 only, where Go does not let the suites be chosen.
    ui->cipher_suites_l->setEnabled(!limited && !quic);
    ui->cipher_suites->setEnabled(!limited && !quic);
    ui->fragment_l->setEnabled(fragment);
    ui->fragment->setEnabled(fragment);
    ui->tls_frag_fall_delay->setEnabled(!limited && ui->fragment->currentIndex() != Configs::kTriStateOff);
}

void DialogEditProfile::on_certificate_edit_clicked() {
    bool ok;
    const auto txt = QInputDialog::getMultiLineText(this, tr("Certificate"), "", CACHE.certificate.join("\n"), &ok);
    if (!ok) return;
    CACHE.certificate = txt.split("\n", Qt::SkipEmptyParts);
    editor_cache_updated_impl();
    form->revalidate(ui->certificate_edit);
}
