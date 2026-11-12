#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/common/utils.h"
#include "include/configs/outbounds/xrayVless.h"
#include "include/database/DatabaseManager.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/UiMode.h"
#include "include/ui/utils/ValidationForm.h"
#include "include/ui/widget/MuxSettingsWidget.h"
#include "include/ui/widget/ResolverSourceWidget.h"
#include "include/ui/widget/json/JsonEditorDialog.h"

#include <QSignalBlocker>

#include <climits>

namespace {
constexpr int kXrayPanelNetworkMinWidth = 760;
}

// A stored value the combo does not offer is shown, and flagged by its rule, instead of being rewritten on save.
void DialogEditProfile::keepXrayComboValue(QComboBox *combo, const QString &value) {
    if (!value.isEmpty() && combo->findText(value) < 0) combo->addItem(value);
}

void DialogEditProfile::setupXrayStream() {
    ui->xray_network->addItems(Configs::XrayNetworks);
    ui->xray_security->addItems({"", "tls", "reality"});
    ui->xray_fp->addItems(Configs::tlsFingerprints);
    ui->xray_ed_length->setValidator(new QIntValidator(0, INT_MAX, this));
    setupXrayXHTTPControls();
    setupXrayXDriveControls();
    setupXrayKCPControls();

    connect(ui->xray_network, &QComboBox::currentTextChanged, this, [this](const QString &network) {
        loadXrayNetwork(network);
        relayout();
    });
    connect(ui->xray_security, &QComboBox::currentTextChanged, this, [this] { relayout(); });
    connect(ui->xray_mode, &QComboBox::currentTextChanged, this, [this] { relayout(); });
    connect(ui->xray_xpadding_obfs_mode, &QCheckBox::toggled, this, [this] { relayout(); });
    connect(ui->xray_mux, &QComboBox::currentIndexChanged, this, [this] { relayout(); });
    connect(ui->xray_mux_custom, &QPushButton::clicked, this, [this] {
        XrayMuxSettingsWidget::openDialog(this, CACHE.xrayMux, [this](const Configs::XrayMuxSettings &settings) { CACHE.xrayMux = settings; });
    });
    bindXrayStream();
}

void DialogEditProfile::bindXrayStream() {
    using Validation::Severity;
    namespace Checks = Configs::Checks;
    const auto reality = [this] { return ui->xray_security->currentText() == "reality"; };
    const auto effectiveSni = [this] {
        const auto sni = ui->xray_sni->text().trimmed();
        return sni.isEmpty() ? ui->address->text().remove(' ') : sni;
    };

    form->addRule({ui->xray_network}, [this] { return Checks::XrayNetwork(ui->xray_network->currentText()); });
    form->addRule({ui->xray_security}, [this] { return Checks::XraySecurity(ui->xray_security->currentText()); });
    form->addRule({ui->xray_security, ui->xray_network}, [this] {
        return Checks::XrayRealityNetwork(ui->xray_security->currentText(), ui->xray_network->currentText());
    });
    form->addRule({ui->xray_mux, ui->xray_network}, [this] {
        return Checks::XrayXDriveMux(ui->xray_mux->currentIndex(), ui->xray_network->currentText());
    });

    form->bind(ui->xray_sni, Checks::ServerName).warning();
    form->addRule({ui->xray_fp}, [this, reality] { return Checks::XrayFingerprint(ui->xray_fp->currentText(), reality()); });
    form->bind(ui->xray_cipher_suites, Checks::XrayCipherSuites).warning();
    form->addRule({ui->xray_cipher_suites, ui->xray_fp}, [this] {
        if (ui->xray_security->currentText() != "tls") return QString();
        auto fingerprint = ui->xray_fp->currentText().trimmed();
        if (fingerprint.isEmpty()) fingerprint = Configs::dataManager->settingsRepo->utlsFingerprint;
        return Checks::XrayCipherSuitesNeedUnsafe(ui->xray_cipher_suites->text(), fingerprint);
    }, nullptr, Severity::Warning);
    form->bind(ui->xray_pinned_peer_cert_sha256, Checks::XrayPinnedSha256);
    form->bind(ui->xray_ech_config_list, [effectiveSni](const QString &text) { return Checks::XrayEchConfigList(text, effectiveSni()); }).warning();
    form->bind(ui->xray_reality_pbk, [](const QString &text) { return Checks::RealityPublicKey(text, true); }).when(reality);
    form->bind(ui->xray_reality_sid, Checks::RealityShortId);
    form->bind(ui->xray_reality_spiderx, Checks::XraySpiderX);

    form->bind(ui->xray_host, [](const QString &text) { return Checks::HostHeader(text, false); }).warning();
    form->bind(ui->xray_headers, Checks::HeaderPairs);
    form->addRule({ui->xray_headers}, [this] {
        const auto network = ui->xray_network->currentText();
        if (network != "xhttp" && network != "httpupgrade") return QString();
        return Checks::XrayHeadersHost(Configs::parseHeaderPairs(ui->xray_headers->text()));
    });
    form->bind(ui->xray_ed_length, Validation::Rules::Int(0, INT_MAX));
    bindXrayXHTTP();
    bindXrayXDrive();
    bindXrayKCP();
}

void DialogEditProfile::loadXrayStream() {
    if (!ent->outbound->HasXrayStream()) return;
    const auto stream = ent->outbound->GetXrayStream();

    loadXrayXHTTP(*stream->xhttp);
    CACHE.XrayDownloadSettings = stream->xhttp->downloadSettings;
    CACHE.XrayFinalmask = stream->finalmask;
    Configs::UpgradeXrayFinalmask(CACHE.XrayFinalmask);
    CACHE.XrayXDriveTemplate = stream->xdrive->templateSpec;
    loadXrayXDrive(*stream->xdrive);
    loadXrayKCP(*stream->kcp);
    xdnsResolvers->load(stream->xdnsResolvers);
    xdnsResolvers->setProfileId(ent->id);
    {
        const QSignalBlocker blocker(ui->xray_network);
        const auto network = Configs::NormalizeXrayNetwork(stream->network);
        keepXrayComboValue(ui->xray_network, network);
        selectComboText(ui->xray_network, network);
    }
    loadXrayNetwork(ui->xray_network->currentText());
    if (const auto security = stream->security.toLower(); security != "none") keepXrayComboValue(ui->xray_security, security);
    selectComboText(ui->xray_security, stream->security.toLower());
    const auto mux = ent->outbound->GetXrayMultiplex();
    ui->xray_mux->setCurrentIndex(mux->getMuxState());
    offerCustomMux(ui->xray_mux, Configs::kXrayMuxCustom);
    CACHE.xrayMux = mux->CustomSettings();

    const bool tls = stream->security == "tls";
    ui->xray_sni->setText(tls ? stream->TLS->serverName : stream->reality->serverName);
    // Keep a fingerprint the list does not offer, or saving would silently reset it to Xray's default.
    const auto fingerprint = tls ? stream->TLS->fingerprint : stream->reality->fingerprint;
    if (ui->xray_fp->findText(fingerprint) < 0) ui->xray_fp->addItem(fingerprint);
    selectComboText(ui->xray_fp, fingerprint);
    ui->xray_alpn->setText(stream->TLS->alpn.join(","));
    ui->xray_cipher_suites->setText(stream->TLS->cipherSuites);
    ui->xray_pinned_peer_cert_sha256->setText(stream->TLS->pinnedPeerCertSha256);
    ui->xray_verify_peer_cert_by_name->setText(stream->TLS->verifyPeerCertByName);
    ui->xray_ech_config_list->setText(stream->TLS->echConfigList);
    ui->xray_reality_pbk->setText(stream->reality->password);
    ui->xray_reality_sid->setText(stream->reality->shortId);
    ui->xray_reality_spiderx->setText(stream->reality->spiderX);
}

void DialogEditProfile::loadXrayNetwork(const QString &network) {
    if (ent == nullptr || !ent->outbound->HasXrayStream()) return;
    const auto stream = ent->outbound->GetXrayStream();

    if (network == "xhttp") {
        ui->xray_host->setText(stream->xhttp->host);
        ui->xray_path->setText(stream->xhttp->path);
        selectComboText(ui->xray_mode, stream->xhttp->mode);
        ui->xray_headers->setText(Configs::getHeadersString(stream->xhttp->headers));
    } else if (network == "grpc") {
        ui->xray_host->setText(stream->grpc->authority);
        ui->xray_path->setText(stream->grpc->serviceName);
        ui->xray_multi_mode->setChecked(stream->grpc->multiMode);
    } else if (network == "ws") {
        ui->xray_host->setText(stream->ws->host);
        ui->xray_path->setText(stream->ws->path);
        ui->xray_ed_length->setText(QString::number(stream->ws->ed));
        ui->xray_headers->setText(Configs::getHeadersString(stream->ws->headers));
    } else if (network == "httpupgrade") {
        ui->xray_host->setText(stream->httpupgrade->host);
        ui->xray_path->setText(stream->httpupgrade->path);
        ui->xray_ed_length->setText(QString::number(stream->httpupgrade->ed));
        ui->xray_headers->setText(Configs::getHeadersString(stream->httpupgrade->headers));
    }
}

void DialogEditProfile::saveXrayStream() {
    if (!ent->outbound->HasXrayStream()) return;
    auto stream = ent->outbound->GetXrayStream();

    stream->network = ui->xray_network->currentText().trimmed();
    stream->security = ui->xray_security->currentText().trimmed();
    stream->finalmask = CACHE.XrayFinalmask;
    stream->xdnsResolvers = xdnsResolvers->save();
    const auto mux = ent->outbound->GetXrayMultiplex();
    const int muxState = ui->xray_mux->currentIndex();
    mux->saveMuxState(muxState);
    if (muxState == Configs::kXrayMuxCustom) mux->SetCustomSettings(CACHE.xrayMux);

    const auto sni = ui->xray_sni->text().trimmed();
    const auto fingerprint = ui->xray_fp->currentText().trimmed();
    if (stream->security == "tls") {
        stream->TLS->serverName = sni;
        stream->TLS->fingerprint = fingerprint;
    } else if (stream->security == "reality") {
        stream->reality->serverName = sni;
        stream->reality->fingerprint = fingerprint;
    }
    stream->TLS->alpn = SplitAndTrim(ui->xray_alpn->text(), ",", false);
    stream->TLS->pinnedPeerCertSha256 = ui->xray_pinned_peer_cert_sha256->text().trimmed();
    stream->TLS->verifyPeerCertByName = ui->xray_verify_peer_cert_by_name->text().trimmed();
    stream->TLS->echConfigList = ui->xray_ech_config_list->text().trimmed();
    stream->TLS->cipherSuites = Configs::splitCipherSuites(ui->xray_cipher_suites->text()).join(':');
    stream->reality->password = ui->xray_reality_pbk->text().trimmed();
    stream->reality->shortId = ui->xray_reality_sid->text().trimmed();
    stream->reality->spiderX = ui->xray_reality_spiderx->text().trimmed();

    const auto host = ui->xray_host->text().trimmed();
    const auto path = ui->xray_path->text().trimmed();
    if (stream->network == "xhttp") {
        stream->xhttp->host = host;
        stream->xhttp->path = path;
        stream->xhttp->mode = ui->xray_mode->currentText().trimmed();
        stream->xhttp->headers = Configs::parseHeaderPairs(ui->xray_headers->text());
        saveXrayXHTTP(*stream->xhttp);
    } else if (stream->network == "grpc") {
        stream->grpc->authority = host;
        stream->grpc->serviceName = path;
        stream->grpc->multiMode = ui->xray_multi_mode->isChecked();
    } else if (stream->network == "ws") {
        stream->ws->host = host;
        stream->ws->path = path;
        stream->ws->ed = ui->xray_ed_length->text().trimmed().toInt();
        stream->ws->headers = Configs::parseHeaderPairs(ui->xray_headers->text());
    } else if (stream->network == "httpupgrade") {
        stream->httpupgrade->host = host;
        stream->httpupgrade->path = path;
        stream->httpupgrade->ed = ui->xray_ed_length->text().trimmed().toInt();
        stream->httpupgrade->headers = Configs::parseHeaderPairs(ui->xray_headers->text());
    } else if (stream->network == "xdrive") {
        saveXrayXDrive(*stream->xdrive);
    } else if (stream->network == "kcp") {
        saveXrayKCP(*stream->kcp);
    }
}

void DialogEditProfile::updateXrayRows() {
    const bool hasStream = ent->outbound->HasXrayStream();
    ui->xray_settings_box->setVisible(hasStream);

    const auto security = ui->xray_security->currentText();
    const bool tls = security == "tls";
    const bool reality = security == "reality";
    const bool securityBox = hasStream && (tls || reality);
    ui->xray_security_box->setVisible(securityBox);
    ui->xray_tls_only->setVisible(tls);
    ui->xray_reality_box->setVisible(reality);

    const auto network = ui->xray_network->currentText();
    const bool xhttp = network == "xhttp";
    const bool grpc = network == "grpc";
    const bool xdrive = network == "xdrive";
    const bool kcp = network == "kcp";
    const bool networkBox = hasStream && network != "raw";
    ui->xray_network_box->setVisible(networkBox);
    ui->xray_common_box->setVisible(!xdrive && !kcp);
    ui->xray_xhttp_box->setVisible(xhttp);
    ui->xray_xdrive_box->setVisible(xdrive);
    ui->xray_kcp_box->setVisible(kcp);
    setRowVisible(xdnsResolversLabel, xdnsResolvers, kcp && Configs::XrayFinalmaskHasXdns(CACHE.XrayFinalmask));
    setRowVisible(ui->label_xray_finalmask, ui->xray_finalmask_edit, kcp || !UiMode::Simple());
    ui->xray_network_scroll->setMinimumWidth(xhttp || xdrive ? kXrayPanelNetworkMinWidth : 0);
    setRowVisible(ui->xray_ed_label, ui->xray_ed_length, !xhttp && !grpc);
    setRowVisible(ui->xray_headers_l, ui->xray_headers, !grpc);
    ui->xray_multi_mode->setVisible(grpc);
    ui->xray_mux_custom->setVisible(ui->xray_mux->currentIndex() == Configs::kXrayMuxCustom);
    updateXrayXHTTPControls();
    updateXrayXDriveControls();

    ui->xray_widget->setVisible(securityBox || networkBox);
}

void DialogEditProfile::on_xray_downloadsettings_edit_clicked() {
    auto editor = new JsonEdit::JsonEditorDialog(QString2QJsonObject(CACHE.XrayDownloadSettings), this);
    const auto result = editor->OpenEditor();
    CACHE.XrayDownloadSettings = result.isEmpty() ? QString() : QJsonObject2QString(result, true);
    editor->deleteLater();
    editor_cache_updated_impl();
}

void DialogEditProfile::on_xray_finalmask_edit_clicked() {
    const bool xdns = Configs::XrayFinalmaskHasXdns(CACHE.XrayFinalmask);
    auto editor = new JsonEdit::JsonEditorDialog(CACHE.XrayFinalmask, this);
    CACHE.XrayFinalmask = editor->OpenEditor();
    Configs::UpgradeXrayFinalmask(CACHE.XrayFinalmask);
    editor->deleteLater();
    editor_cache_updated_impl();
    form->revalidate(ui->xray_finalmask_edit);
    if (Configs::XrayFinalmaskHasXdns(CACHE.XrayFinalmask) != xdns) relayout();
}
