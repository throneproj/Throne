#include "include/ui/profile/dialog_edit_profile.h"

#include "include/ui/profile/edit_http.h"
#include "include/ui/profile/edit_shadowsocks.h"
#include "include/ui/profile/edit_chain.h"
#include "include/ui/profile/edit_vmess.h"
#include "include/ui/profile/edit_vless.h"
#include "include/ui/profile/edit_anytls.h"
#include "include/ui/profile/edit_wireguard.h"
#include "include/ui/profile/edit_tailscale.h"
#include "include/ui/profile/edit_ssh.h"
#include "include/ui/profile/edit_custom.h"
#include "include/ui/profile/edit_extra_core.h"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/global/Utils.hpp"

#include <QInputDialog>

#include "include/configs/common/TLS.h"
#include "include/configs/common/utils.h"
#include "include/configs/common/xrayStreamSetting.h"
#include "include/database/ProfilesRepo.h"



#include "include/ui/profile/edit_advanced.h"
#include "include/ui/profile/edit_hysteria.h"
#include "include/ui/profile/edit_socks.h"
#include "include/ui/profile/edit_trojan.h"
#include "include/ui/profile/edit_tuic.h"
#include "include/ui/profile/edit_xrayvless.h"

#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);
#define LOAD_TYPE(a) ui->type->addItem(Configs::dataManager->profilesRepo->NewProfile(a)->outbound->DisplayType(), a);

void DialogEditProfile::toggleSingboxWidgets(bool show) {
    ui->stream_box->setVisible(show);
    ui->right_all_w->setVisible(show);
}

void DialogEditProfile::toggleXrayWidgets(bool show) {
    ui->xray_settings_box->setVisible(show);
    ui->xray_widget->setVisible(show);
}

DialogEditProfile::DialogEditProfile(const QString &_type, int profileOrGroupId, QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogEditProfile) {
    // setup UI
    ui->setupUi(this);
    ui->dialog_layout->setAlignment(ui->left, Qt::AlignTop);

    // Xray init
    ui->xray_security->addItems({"", "tls", "reality"});
    ui->xray_network->addItems(Configs::XrayNetworks);
    ui->xray_fp->addItems(Configs::tlsFingerprints);
    ui->xray_mode->addItems(Configs::XrayXHTTPModes);
    toggleXrayWidgets(false);

    // network changed
    network_title_base = ui->network_box->title();
    connect(ui->network, &QComboBox::currentTextChanged, this, [=,this](const QString &txt) {
        ui->network_box->setTitle(network_title_base.arg(txt));
        if (txt == "grpc") {
            ui->headers->setVisible(false);
            ui->headers_l->setVisible(false);
            ui->method->setVisible(false);
            ui->method_l->setVisible(false);
            ui->path->setVisible(false);
            ui->path_l->setVisible(false);
            ui->host->setVisible(false);
            ui->host_l->setVisible(false);
        } else if (txt == "ws" || txt == "httpupgrade") {
            ui->headers->setVisible(true);
            ui->headers_l->setVisible(true);
            ui->method->setVisible(false);
            ui->method_l->setVisible(false);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
            ui->host->setVisible(true);
            ui->host_l->setVisible(true);
        } else if (txt == "http") {
            ui->headers->setVisible(true);
            ui->headers_l->setVisible(true);
            ui->method->setVisible(true);
            ui->method_l->setVisible(true);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
            ui->host->setVisible(true);
            ui->host_l->setVisible(true);
        } else {
            ui->headers->setVisible(false);
            ui->headers_l->setVisible(false);
            ui->method->setVisible(false);
            ui->method_l->setVisible(false);
            ui->path->setVisible(false);
            ui->path_l->setVisible(false);
            ui->host->setVisible(false);
            ui->host_l->setVisible(false);
        }
        if (txt == "grpc") {
            ui->service_name->setVisible(true);
            ui->service_name_l->setVisible(true);
        } else {
            ui->service_name->setVisible(false);
            ui->service_name_l->setVisible(false);
        }
        if (txt == "ws") {
            ui->ws_early_data_length->setVisible(true);
            ui->ws_early_data_length_l->setVisible(true);
            ui->ws_early_data_name->setVisible(true);
            ui->ws_early_data_name_l->setVisible(true);
        } else {
            ui->ws_early_data_length->setVisible(false);
            ui->ws_early_data_length_l->setVisible(false);
            ui->ws_early_data_name->setVisible(false);
            ui->ws_early_data_name_l->setVisible(false);
        }
        if (!ui->utlsFingerprint->count()) ui->utlsFingerprint->addItems(Configs::tlsFingerprints);
        int networkBoxVisible = 0;
        for (auto label: ui->network_box->findChildren<QLabel *>()) {
            if (!label->isHidden()) networkBoxVisible++;
        }
        ui->network_box->setVisible(networkBoxVisible);
        ADJUST_SIZE
    });
    ui->network->removeItem(0);

    // security changed
    connect(ui->security, &QComboBox::currentTextChanged, this, [=,this](const QString &txt) {
        if (txt == "tls") {
            ui->security_box->setVisible(true);
            ui->tls_camouflage_box->setVisible(true);
        } else {
            ui->security_box->setVisible(false);
            ui->tls_camouflage_box->setVisible(false);
        }
        ADJUST_SIZE
    });
    emit ui->security->currentTextChanged(ui->security->currentText());

    // for fragment
    connect(ui->tls_frag, &QCheckBox::stateChanged, this, [=,this](bool state)
    {
        ui->tls_frag_fall_delay->setEnabled(state);
    });

    // mux setting changed
    connect(ui->multiplex, &QComboBox::currentTextChanged, this, [=,this](const QString &txt) {
        if (txt == "Off") {
            ui->brutal_enable->setCheckState(Qt::CheckState::Unchecked);
            ui->brutal_box->setEnabled(false);
        } else {
            ui->brutal_box->setEnabled(true);
        }
    });

    // Advanced options
    connect(ui->advanced_button, &QPushButton::clicked, this, [=,this]() {
        auto advancedWidget = new EditAdvanced(this, ent);
        advancedWidget->show();
    });

    // Xray
    ui->xray_network_box->hide();
    connect(ui->xray_network, &QComboBox::currentTextChanged, this, [=,this](const QString &txt) {
       if (txt == "raw") {
           ui->xray_network_box->setVisible(false);
           if (ui->xray_security_box->isHidden()) ui->xray_widget->hide();
           ui->xray_downloadsettings_edit->setVisible(false);
       }
       else {
           ui->xray_widget->show();
           ui->xray_network_box->setVisible(true);
           ui->xray_downloadsettings_edit->setVisible(txt == "xhttp");
       }
        ADJUST_SIZE
    });

    ui->xray_security_box->hide();
    connect(ui->xray_security, &QComboBox::currentTextChanged, this, [=,this](const QString &txt) {
        if (txt.isEmpty()) {
            ui->xray_security_box->setVisible(false);
            if (ui->xray_network_box->isHidden()) ui->xray_widget->hide();
        }
        else if (txt == "tls") {
            ui->xray_widget->show();
            ui->xray_security_box->setVisible(true);
            ui->xray_tls_only->setVisible(true);
            ui->xray_reality_box->setVisible(false);
        } else {
            ui->xray_widget->show();
            ui->xray_security_box->setVisible(true);
            ui->xray_tls_only->setVisible(false);
            ui->xray_reality_box->setVisible(true);
        }
        ADJUST_SIZE
    });

    newEnt = _type != "";
    if (newEnt) {
        this->groupId = profileOrGroupId;
        this->type = _type;

        // load type to combo box
        LOAD_TYPE("socks")
        LOAD_TYPE("http")
        LOAD_TYPE("shadowsocks")
        LOAD_TYPE("trojan")
        LOAD_TYPE("vmess")
        LOAD_TYPE("vless")
        ui->type->addItem("VLESS (Xray)", "xrayvless");
        LOAD_TYPE("hysteria")
        LOAD_TYPE("tuic")
        LOAD_TYPE("anytls")
        LOAD_TYPE("wireguard")
        LOAD_TYPE("tailscale")
        LOAD_TYPE("ssh")
        ui->type->addItem(tr("Custom (%1 outbound)").arg(software_core_name), "outbound");
        ui->type->addItem(tr("Custom (%1 config)").arg(software_core_name), "fullconfig");
        ui->type->addItem(tr("Extra Core"), "extracore");
        LOAD_TYPE("chain")

        // type changed
        connect(ui->type, &QComboBox::currentIndexChanged, this, [=,this](int index) {
            typeSelected(ui->type->itemData(index).toString());
        });
    } else {
        this->ent = Configs::dataManager->profilesRepo->GetProfile(profileOrGroupId);
        if (this->ent == nullptr) return;
        this->type = ent->type;
        ui->type->setVisible(false);
        ui->type_l->setVisible(false);
    }

    typeSelected(this->type);
}

DialogEditProfile::~DialogEditProfile() {
    delete ui;
}

void DialogEditProfile::typeSelected(const QString &newType) {
    QString customType;
    type = newType;
    bool validType = true;

    if (type == "http") {
        auto _innerWidget = new EditHttp(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "socks") {
        auto _innerWidget = new EditSocks(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "shadowsocks") {
        auto _innerWidget = new EditShadowSocks(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "chain") {
        auto _innerWidget = new EditChain(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "vmess") {
        auto _innerWidget = new EditVMess(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if ( type == "vless") {
        auto _innerWidget = new EditVless(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
        connect(_innerWidget->_flow, &QComboBox::currentTextChanged, _innerWidget, [=,this](const QString &txt)
        {
            if (txt == "xtls-rprx-vision")
            {
                ui->multiplex->setDisabled(true);
            } else
            {
                ui->multiplex->setDisabled(false);
            }
        });
    } else if (type == "xrayvless") {
        auto _innerWidget = new EditXrayVless(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "trojan") {
        auto _innerWidget = new EditTrojan(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "hysteria") {
        auto _innerWidget = new EditHysteria(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
        connect(_innerWidget->_protocol_version, &QComboBox::currentTextChanged, _innerWidget, [=,this](const QString &txt)
        {
            _innerWidget->editHysteriaLayout(txt);
            ADJUST_SIZE
        });
    } else if (type == "tuic") {
        auto _innerWidget = new EditTuic(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "anytls") {
        auto _innerWidget = new EditAnyTLS(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "wireguard") {
        auto _innerWidget = new EditWireguard(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "tailscale") {
        auto _innerWidget = new EditTailScale(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "ssh") {
        auto _innerWidget = new EditSSH(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "outbound" || type == "fullconfig" || type == "custom") {
        auto _innerWidget = new EditCustom(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
        customType = newEnt ? type : ent->Custom()->type;
        _innerWidget->preset_core = customType;
        type = "custom";
    } else if (type == "extracore")
    {
        auto _innerWidget = new EditExtraCore(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else {
        validType = false;
    }

    if (!validType) {
        MessageBoxWarning(newType, "Wrong type");
        return;
    }

    if (newEnt) {
        this->ent = Configs::dataManager->profilesRepo->NewProfile(type);
        this->ent->gid = groupId;
    }

    // hide some widget
    auto showAddressPort = type != "chain" && customType != "outbound" && customType != "fullconfig" && type != "extracore" && type != "tailscale";
    ui->address->setVisible(showAddressPort);
    ui->address_l->setVisible(showAddressPort);
    ui->port->setVisible(showAddressPort);
    ui->port_l->setVisible(showAddressPort);

    if (ent->outbound->HasTLS() || ent->outbound->HasTransport()) {
        ui->right_all_w->setVisible(true);
        auto tls = ent->outbound->GetTLS();
        auto transport = ent->outbound->GetTransport();
        if (ent->outbound->MustTLS()) {
            ui->security->setCurrentText("tls");
            ui->security->setEnabled(false);
        } else {
            ui->security->setCurrentText(tls->enabled ? "tls" : "");
            ui->security->setEnabled(true);
        }
        ui->network->setCurrentText(transport->type);
        ui->path->setText(transport->path);
        ui->host->setText(transport->host);
        ui->method->setText(transport->method);
        ui->sni->setText(tls->server_name);
        ui->alpn->setText(tls->alpn.join(","));
        if (newEnt) {
            ui->utlsFingerprint->setCurrentText(Configs::dataManager->settingsRepo->utlsFingerprint);
        } else {
            ui->utlsFingerprint->setCurrentText(tls->utls->fingerPrint);
        }
        ui->tls_frag->setChecked(tls->fragment);
        ui->tls_frag_fall_delay->setEnabled(tls->fragment);
        ui->tls_frag_fall_delay->setText(tls->fragment_fallback_delay);
        ui->tls_rec_frag->setChecked(tls->record_fragment);
        ui->insecure->setChecked(tls->insecure);
        ui->headers->setText(Configs::getHeadersString(transport->headers));
        ui->service_name->setText(transport->service_name);
        ui->ws_early_data_name->setText(transport->early_data_header_name);
        ui->ws_early_data_length->setText(Int2String(transport->max_early_data));
        ui->reality_pbk->setText(tls->reality->public_key);
        ui->reality_sid->setText(tls->reality->short_id);
        CACHE.certificate = tls->certificate;
    } else {
        ui->right_all_w->setVisible(false);
    }

    if (ent->outbound->IsXray()) {
        auto xrayStream = ent->outbound->GetXrayStream();
        auto xrayMux = ent->outbound->GetXrayMultiplex();

        ui->xray_network->setCurrentText(xrayStream->network);
        ui->xray_security->setCurrentText(xrayStream->security);
        ui->xray_mux->setCurrentIndex(xrayMux->getMuxState());

        ui->xray_sni->setText(xrayStream->security == "tls" ? xrayStream->TLS->serverName : xrayStream->reality->serverName);
        ui->xray_fp->setCurrentText(xrayStream->security == "tls" ? xrayStream->TLS->fingerprint : xrayStream->reality->fingerprint);
        ui->xray_alpn->setText(xrayStream->TLS->alpn.join(","));
        ui->xray_insecure->setChecked(xrayStream->TLS->allowInsecure);
        ui->xray_reality_pbk->setText(xrayStream->reality->password);
        ui->xray_reality_sid->setText(xrayStream->reality->shortId);
        ui->xray_reality_spiderx->setText(xrayStream->reality->spiderX);

        ui->xray_host->setText(xrayStream->xhttp->host);
        ui->xray_path->setText(xrayStream->xhttp->path);
        ui->xray_mode->setCurrentText(xrayStream->xhttp->mode);
        ui->xray_headers->setText(Configs::getHeadersString(xrayStream->xhttp->headers));
        ui->xray_xpaddingbytes->setText(xrayStream->xhttp->xPaddingBytes);
        ui->xray_no_grpc->setChecked(xrayStream->xhttp->noGRPCHeader);
        ui->xray_scMaxEachPostBytes->setText(xrayStream->xhttp->scMaxEachPostBytes);
        ui->xray_scMinPostsIntervalMs->setText(xrayStream->xhttp->scMinPostsIntervalMs);
        ui->xray_max_concurrency->setText(xrayStream->xhttp->maxConcurrency);
        ui->xray_max_connections->setText(xrayStream->xhttp->maxConnections);
        ui->xray_hMaxRequestTimes->setText(xrayStream->xhttp->hMaxRequestTimes);
        ui->xray_hMaxReusableSecs->setText(xrayStream->xhttp->hMaxReusableSecs);
        ui->xray_max_reuse_times->setText(xrayStream->xhttp->cMaxReuseTimes);
        ui->xray_keep_alive_period->setText(Int2String(xrayStream->xhttp->hKeepAlivePeriod));
        CACHE.XrayDownloadSettings = xrayStream->xhttp->downloadSettings;
        ui->xray_downloadsettings_edit->setText(xrayStream->xhttp->downloadSettings.isEmpty() ? "Not Set" : "Already Set");

        toggleXrayWidgets(true);
        toggleSingboxWidgets(false);
    } else {
        toggleXrayWidgets(false);
        toggleSingboxWidgets(true);
    }

    if (ent->outbound->HasMux()) {
        auto mux = ent->outbound->GetMux();
        ui->multiplex->setCurrentIndex(mux->getMuxState());
        ui->brutal_enable->setChecked(mux->brutal->enabled);
        ui->brutal_d_speed->setText(Int2String(mux->brutal->down_mbps));
        ui->brutal_u_speed->setText(Int2String(mux->brutal->up_mbps));
    }

    // 左边 bean
    auto old = ui->bean->layout()->itemAt(0)->widget();
    ui->bean->layout()->removeWidget(old);
    innerWidget->layout()->setContentsMargins(0, 0, 0, 0);
    ui->bean->layout()->addWidget(innerWidget);
    ui->bean->setTitle(ent->outbound->DisplayType());
    delete old;

    // 左边 bean inner editor
    innerEditor->get_edit_dialog = [&]() { return static_cast<QWidget*>(this); };
    innerEditor->get_edit_text_name = [&]() { return ui->name->text(); };
    innerEditor->get_edit_text_serverAddress = [&]() { return ui->address->text(); };
    innerEditor->get_edit_text_serverPort = [&]() { return ui->port->text(); };
    innerEditor->editor_cache_updated = [=,this] { editor_cache_updated_impl(); };
    innerEditor->onStart(ent);

    // 左边 common
    ui->name->setText(ent->outbound->name);
    ui->address->setText(ent->outbound->GetAddress());
    ui->port->setText(ent->outbound->GetPort());
    ui->port->setValidator(QRegExpValidator_Number);

    // 星号
    ADD_ASTERISK(this)
    if (ent->outbound->HasTransport()) {
        ui->network_l->setVisible(true);
        ui->network->setVisible(true);
        if (ui->network->currentText() == "tcp") {
            ui->network_box->setVisible(false);
        } else {
            ui->network_box->setVisible(true);
        }
    } else {
        ui->network_l->setVisible(false);
        ui->network->setVisible(false);
        ui->network_box->setVisible(false);
    }
    if (ent->outbound->HasTLS()) {
        ui->security->setVisible(true);
        ui->security_l->setVisible(true);
    } else {
        ui->security->setVisible(false);
        ui->security_l->setVisible(false);
    }
    if (ent->outbound->HasMux()) {
        ui->multiplex->setVisible(true);
        ui->multiplex_l->setVisible(true);
        ui->brutal_box->setVisible(true);
    } else {
        ui->multiplex->setVisible(false);
        ui->multiplex_l->setVisible(false);
        ui->brutal_box->setVisible(false);
    }
    int streamBoxVisible = 0;
    for (auto label: ui->stream_box->findChildren<QLabel *>()) {
        if (!label->isHidden() && label->parent() == ui->stream_box) streamBoxVisible++;
    }
    ui->stream_box->setVisible(streamBoxVisible);

    auto rightNoBox = (ui->security_box->isHidden() && ui->network_box->isHidden() && ui->tls_camouflage_box->isHidden());
    if (rightNoBox && !ent->outbound->HasTLS() && !ent->outbound->HasTransport() && !ui->right_all_w->isHidden()) {
        ui->right_all_w->setVisible(false);
    }

    editor_cache_updated_impl();
    ADJUST_SIZE

    // 第一次显示
    if (isHidden()) {
        runOnThread([=,this] { show(); }, this);
    }
}

bool DialogEditProfile::validateHeaders() {
    return !ui->headers->text().contains("|");
}

bool DialogEditProfile::onEnd() {
    // bean
    if (!innerEditor->onEnd()) {
        return false;
    }

    if (!validateHeaders()) return false;

    ent->outbound->name = ui->name->text();
    ent->outbound->SetAddress(ui->address->text().remove(' '));
    ent->outbound->SetPort(ui->port->text().toInt());

    if (ent->outbound->HasTLS() || ent->outbound->HasTransport()) {
        auto tls = ent->outbound->GetTLS();
        auto transport = ent->outbound->GetTransport();
        transport->type = ui->network->currentText();
        tls->enabled = ui->security->currentText() == "tls";
        transport->path = ui->path->text();
        transport->host = ui->host->text();
        tls->server_name = ui->sni->text();
        tls->alpn = SplitAndTrim(ui->alpn->text(), ",");
        tls->utls->fingerPrint = ui->utlsFingerprint->currentText();
        tls->utls->enabled = !tls->utls->fingerPrint.isEmpty();
        tls->fragment = ui->tls_frag->isChecked();
        tls->fragment_fallback_delay = ui->tls_frag_fall_delay->text();
        tls->record_fragment = ui->tls_rec_frag->isChecked();
        tls->insecure = ui->insecure->isChecked();
        transport->headers = Configs::parseHeaderPairs(ui->headers->text());
        transport->method = ui->method->text();
        transport->service_name = ui->service_name->text();
        transport->early_data_header_name = ui->ws_early_data_name->text();
        transport->max_early_data = ui->ws_early_data_length->text().toInt();
        tls->reality->public_key = ui->reality_pbk->text();
        tls->reality->short_id = ui->reality_sid->text();
        tls->reality->enabled = !tls->reality->public_key.isEmpty();
        tls->certificate = CACHE.certificate;
    }
    if (ent->outbound->HasMux()) {
        auto mux = ent->outbound->GetMux();
        mux->saveMuxState(ui->multiplex->currentIndex());
        mux->brutal->enabled = ui->brutal_enable->isChecked();
        mux->brutal->down_mbps = ui->brutal_d_speed->text().toInt();
        mux->brutal->up_mbps = ui->brutal_u_speed->text().toInt();
    }
    if (ent->outbound->IsXray()) {
        auto xrayStream = ent->outbound->GetXrayStream();
        auto xrayMux = ent->outbound->GetXrayMultiplex();

        xrayStream->network = ui->xray_network->currentText();
        xrayStream->security = ui->xray_security->currentText();
        xrayMux->saveMuxState(ui->xray_mux->currentIndex());

        auto sni = ui->xray_sni->text();
        if (xrayStream->security == "tls") xrayStream->TLS->serverName = sni;
        else if (xrayStream->security == "reality") xrayStream->reality->serverName = sni;

        auto fp = ui->xray_fp->currentText();
        if (xrayStream->security == "tls") xrayStream->TLS->fingerprint = fp;
        else if (xrayStream->security == "reality") xrayStream->reality->fingerprint = fp;

        xrayStream->TLS->alpn = ui->xray_alpn->text().split(",");
        xrayStream->TLS->allowInsecure = ui->xray_insecure->isChecked();
        xrayStream->reality->password = ui->xray_reality_pbk->text();
        xrayStream->reality->shortId = ui->xray_reality_sid->text();
        xrayStream->reality->spiderX = ui->xray_reality_spiderx->text();

        xrayStream->xhttp->host = ui->xray_host->text();
        xrayStream->xhttp->path = ui->xray_path->text();
        xrayStream->xhttp->mode = ui->xray_mode->currentText();
        xrayStream->xhttp->headers = Configs::parseHeaderPairs(ui->xray_headers->text());
        xrayStream->xhttp->xPaddingBytes = ui->xray_xpaddingbytes->text();
        xrayStream->xhttp->noGRPCHeader = ui->xray_no_grpc->isChecked();
        xrayStream->xhttp->scMaxEachPostBytes = ui->xray_scMaxEachPostBytes->text();
        xrayStream->xhttp->scMinPostsIntervalMs = ui->xray_scMinPostsIntervalMs->text();
        xrayStream->xhttp->maxConcurrency = ui->xray_max_concurrency->text();
        xrayStream->xhttp->maxConnections = ui->xray_max_connections->text();
        xrayStream->xhttp->hMaxRequestTimes = ui->xray_hMaxRequestTimes->text();
        xrayStream->xhttp->hMaxReusableSecs = ui->xray_hMaxReusableSecs->text();
        xrayStream->xhttp->cMaxReuseTimes = ui->xray_max_reuse_times->text();
        xrayStream->xhttp->hKeepAlivePeriod = ui->xray_keep_alive_period->text().toLongLong();
        xrayStream->xhttp->downloadSettings = CACHE.XrayDownloadSettings;
    }

    return true;
}

void DialogEditProfile::accept() {
    // save to ent
    if (!onEnd()) {
        return;
    }

    // finish
    QStringList msg = {"accept"};

    if (newEnt) {
        auto ok = Configs::dataManager->profilesRepo->AddProfile(ent);
        if (!ok) {
            MessageBoxWarning("???", "id exists");
        }
    } else {
        auto changed = Configs::dataManager->profilesRepo->Save(ent);
        if (changed && Configs::dataManager->settingsRepo->started_id == ent->id) msg << "restart";
    }

    MW_dialog_message(Dialog_DialogEditProfile, msg.join(","));
    QDialog::accept();
}

// cached editor (dialog)

void DialogEditProfile::editor_cache_updated_impl() {
    if (CACHE.certificate.isEmpty()) {
        ui->certificate_edit->setText(tr("Not set"));
    } else {
        ui->certificate_edit->setText(tr("Already set"));
    }
    if (ent->outbound->IsXray()) {
        ui->xray_downloadsettings_edit->setText(CACHE.XrayDownloadSettings.isEmpty() ? "Not Set" : "Already Set");
    }
    // CACHE macro
    for (auto a: innerEditor->get_editor_cached()) {
        if (a.second.isEmpty()) {
            a.first->setText(tr("Not set"));
        } else {
            a.first->setText(tr("Already set"));
        }
    }
}

void DialogEditProfile::on_certificate_edit_clicked() {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, tr("Certificate"), "", CACHE.certificate.join("\n"), &ok);
    if (ok) {
        CACHE.certificate = txt.split("\n", Qt::SkipEmptyParts);
        editor_cache_updated_impl();
    }
}

void DialogEditProfile::on_xray_downloadsettings_edit_clicked() {
    auto editor = new JsonEditor(QString2QJsonObject(CACHE.XrayDownloadSettings), this);
    auto result = editor->OpenEditor();
    if (!result.isEmpty()) CACHE.XrayDownloadSettings = QJsonObject2QString(result, true);
    else CACHE.XrayDownloadSettings.clear();
    editor->deleteLater();

    editor_cache_updated_impl();
}
