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

#include "include/configs/proxy/includes.h"
#include "include/configs/proxy/Preset.hpp"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "include/global/GuiUtils.hpp"

#include <QInputDialog>

#include "include/ui/profile/edit_hysteria.h"
#include "include/ui/profile/edit_hysteria2.h"
#include "include/ui/profile/edit_socks.h"
#include "include/ui/profile/edit_trojan.h"
#include "include/ui/profile/edit_tuic.h"

#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);
#define LOAD_TYPE(a) ui->type->addItem(Configs::ProfileManager::NewProxyEntity(a)->outbound->DisplayType(), a);

DialogEditProfile::DialogEditProfile(const QString &_type, int profileOrGroupId, QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogEditProfile) {
    // setup UI
    ui->setupUi(this);
    ui->dialog_layout->setAlignment(ui->left, Qt::AlignTop);

    // network changed
    network_title_base = ui->network_box->title();
    connect(ui->network, &QComboBox::currentTextChanged, this, [=,this](const QString &txt) {
        ui->network_box->setTitle(network_title_base.arg(txt));
        if (txt == "grpc") {
            ui->headers->setVisible(false);
            ui->headers_l->setVisible(false);
            ui->method->setVisible(false);
            ui->method_l->setVisible(false);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
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
        if (!ui->utlsFingerprint->count()) ui->utlsFingerprint->addItems(Preset::SingBox::UtlsFingerPrint);
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
        LOAD_TYPE("hysteria")
        LOAD_TYPE("hysteria2")
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
        this->ent = Configs::profileManager->GetProfile(profileOrGroupId);
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
    } else if (type == "trojan") {
        auto _innerWidget = new EditTrojan(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "hysteria") {
        auto _innerWidget = new EditHysteria(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "hysteria2") {
        auto _innerWidget = new EditHysteria2(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
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
        this->ent = Configs::ProfileManager::NewProxyEntity(type);
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
            ui->utlsFingerprint->setCurrentText(Configs::dataStore->utlsFingerprint);
        } else {
            ui->utlsFingerprint->setCurrentText(tls->utls->fingerPrint);
        }
        ui->tls_frag->setChecked(tls->fragment);
        ui->tls_frag_fall_delay->setEnabled(tls->fragment);
        ui->tls_frag_fall_delay->setText(tls->fragment_fallback_delay);
        ui->tls_rec_frag->setChecked(tls->record_fragment);
        ui->insecure->setChecked(tls->insecure);
        ui->headers->setText(transport->getHeadersString());
        ui->ws_early_data_name->setText(transport->early_data_header_name);
        ui->ws_early_data_length->setText(Int2String(transport->max_early_data));
        ui->reality_pbk->setText(tls->reality->public_key);
        ui->reality_sid->setText(tls->reality->short_id);
        CACHE.certificate = tls->certificate;
    } else {
        ui->right_all_w->setVisible(false);
    }

    if (ent->outbound->HasMux()) {
        auto mux = ent->outbound->GetMux();
        ui->multiplex->setCurrentIndex(mux->getMuxState());
        ui->brutal_enable->setChecked(mux->brutal->enabled);
        ui->brutal_speed->setText(Int2String(mux->brutal->down_mbps));
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
    ui->address->setText(ent->outbound->server);
    ui->port->setText(Int2String(ent->outbound->server_port));
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

bool DialogEditProfile::onEnd() {
    // bean
    if (!innerEditor->onEnd()) {
        return false;
    }

    ent->outbound->name = ui->name->text();
    ent->outbound->server = ui->address->text().remove(' ');
    ent->outbound->server_port = ui->port->text().toInt();

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
        transport->headers = transport->getHeaderPairs(ui->headers->text());
        transport->method = ui->method->text();
        transport->early_data_header_name = ui->ws_early_data_name->text();
        transport->max_early_data = ui->ws_early_data_length->text().toInt();
        tls->reality->public_key = ui->reality_pbk->text();
        tls->reality->short_id = ui->reality_sid->text();
        tls->certificate = CACHE.certificate;
    }
    if (ent->outbound->HasMux()) {
        auto mux = ent->outbound->GetMux();
        mux->saveMuxState(ui->multiplex->currentIndex());
        mux->brutal->enabled = ui->brutal_enable->isChecked();
        mux->brutal->down_mbps = ui->brutal_speed->text().toInt();
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
        auto ok = Configs::profileManager->AddProfile(ent);
        if (!ok) {
            MessageBoxWarning("???", "id exists");
        }
    } else {
        auto changed = ent->Save();
        if (changed && Configs::dataStore->started_id == ent->id) msg << "restart";
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
    auto txt = QInputDialog::getMultiLineText(this, tr("Certificate"), "", CACHE.certificate, &ok);
    if (ok) {
        CACHE.certificate = txt;
        editor_cache_updated_impl();
    }
}
