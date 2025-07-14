#include "include/ui/profile/dialog_edit_profile.h"

#include "include/ui/profile/edit_socks_http.h"
#include "include/ui/profile/edit_shadowsocks.h"
#include "include/ui/profile/edit_chain.h"
#include "include/ui/profile/edit_vmess.h"
#include "include/ui/profile/edit_trojan_vless.h"
#include "include/ui/profile/edit_quic.h"
#include "include/ui/profile/edit_wireguard.h"
#include "include/ui/profile/edit_ssh.h"
#include "include/ui/profile/edit_custom.h"
#include "include/ui/profile/edit_extra_core.h"

#include "include/configs/proxy/includes.h"
#include "include/configs/proxy/Preset.hpp"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "include/global/GuiUtils.hpp"

#include <QInputDialog>
#include <QToolTip>

#define ADJUST_SIZE runOnUiThread([=] { adjustSize(); adjustPosition(mainwindow); }, this);
#define LOAD_TYPE(a) ui->type->addItem(NekoGui::ProfileManager::NewProxyEntity(a)->bean->DisplayType(), a);

DialogEditProfile::DialogEditProfile(const QString &_type, int profileOrGroupId, QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogEditProfile) {
    // setup UI
    ui->setupUi(this);
    ui->dialog_layout->setAlignment(ui->left, Qt::AlignTop);

    // network changed
    network_title_base = ui->network_box->title();
    connect(ui->network, &QComboBox::currentTextChanged, this, [=](const QString &txt) {
        ui->network_box->setTitle(network_title_base.arg(txt));
        if (txt == "tcp") {
            ui->header_type->setVisible(true);
            ui->header_type_l->setVisible(true);
            ui->headers->setVisible(false);
            ui->headers_l->setVisible(false);
            ui->method->setVisible(false);
            ui->method_l->setVisible(false);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
            ui->host->setVisible(true);
            ui->host_l->setVisible(true);
        } else if (txt == "grpc") {
            ui->header_type->setVisible(false);
            ui->header_type_l->setVisible(false);
            ui->headers->setVisible(false);
            ui->headers_l->setVisible(false);
            ui->method->setVisible(false);
            ui->method_l->setVisible(false);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
            ui->host->setVisible(false);
            ui->host_l->setVisible(false);
        } else if (txt == "ws" || txt == "httpupgrade") {
            ui->header_type->setVisible(false);
            ui->header_type_l->setVisible(false);
            ui->headers->setVisible(true);
            ui->headers_l->setVisible(true);
            ui->method->setVisible(false);
            ui->method_l->setVisible(false);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
            ui->host->setVisible(true);
            ui->host_l->setVisible(true);
        } else if (txt == "http") {
            ui->header_type->setVisible(false);
            ui->header_type_l->setVisible(false);
            ui->headers->setVisible(true);
            ui->headers_l->setVisible(true);
            ui->method->setVisible(true);
            ui->method_l->setVisible(true);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
            ui->host->setVisible(true);
            ui->host_l->setVisible(true);
        } else {
            ui->header_type->setVisible(false);
            ui->header_type_l->setVisible(false);
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
    connect(ui->security, &QComboBox::currentTextChanged, this, [=](const QString &txt) {
        if (txt == "tls") {
            ui->security_box->setVisible(true);
            ui->tls_camouflage_box->setVisible(true);
            ui->reality_pbk->setVisible(false);
            ui->reality_pbk_l->setVisible(false);
            ui->reality_sid->setVisible(false);
            ui->reality_sid_l->setVisible(false);
        } else if (txt == "reality") {
            ui->security_box->setVisible(true);
            ui->tls_camouflage_box->setVisible(true);
            ui->reality_pbk->setVisible(true);
            ui->reality_pbk_l->setVisible(true);
            ui->reality_sid->setVisible(true);
            ui->reality_sid_l->setVisible(true);
        } else {
            ui->security_box->setVisible(false);
            ui->tls_camouflage_box->setVisible(false);
        }
        ADJUST_SIZE
    });
    emit ui->security->currentTextChanged(ui->security->currentText());

    // mux setting changed
    connect(ui->multiplex, &QComboBox::currentTextChanged, this, [=](const QString &txt) {
        if (txt == "Off") {
            ui->brutal_enable->setCheckState(Qt::CheckState::Unchecked);
            ui->brutal_box->setEnabled(false);
        } else {
            ui->brutal_box->setEnabled(true);
        }
    });

    // 确定模式和 ent
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
        LOAD_TYPE("wireguard")
        LOAD_TYPE("ssh")
        ui->type->addItem(tr("Custom (%1 outbound)").arg(software_core_name), "internal");
        ui->type->addItem(tr("Custom (%1 config)").arg(software_core_name), "internal-full");
        ui->type->addItem(tr("Extra Core"), "extracore");
        LOAD_TYPE("chain")

        // type changed
        connect(ui->type, &QComboBox::currentIndexChanged, this, [=](int index) {
            typeSelected(ui->type->itemData(index).toString());
        });

        ui->apply_to_group->hide();
    } else {
        this->ent = NekoGui::profileManager->GetProfile(profileOrGroupId);
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

    if (type == "socks" || type == "http") {
        auto _innerWidget = new EditSocksHttp(this);
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
    } else if (type == "trojan" || type == "vless") {
        auto _innerWidget = new EditTrojanVLESS(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
        connect(_innerWidget->flow_, &QComboBox::currentTextChanged, _innerWidget, [=](const QString &txt)
        {
            if (txt == "xtls-rprx-vision")
            {
                ui->multiplex->setDisabled(true);
            } else
            {
                ui->multiplex->setDisabled(false);
            }
        });
    } else if (type == "hysteria" || type == "hysteria2" || type == "tuic") {
        auto _innerWidget = new EditQUIC(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "wireguard") {
        auto _innerWidget = new EditWireguard(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "ssh") {
        auto _innerWidget = new EditSSH(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "internal" || type == "internal-full" || type == "custom") {
        auto _innerWidget = new EditCustom(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
        customType = newEnt ? type : ent->CustomBean()->core;
        _innerWidget->preset_core = customType;
        type = "custom";
        ui->apply_to_group->hide();
    } else if (type == "extracore")
    {
        auto _innerWidget = new EditExtraCore(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
        ui->apply_to_group->hide();
    } else {
        validType = false;
    }

    if (!validType) {
        MessageBoxWarning(newType, "Wrong type");
        return;
    }

    if (newEnt) {
        this->ent = NekoGui::ProfileManager::NewProxyEntity(type);
        this->ent->gid = groupId;
    }

    // hide some widget
    auto showAddressPort = type != "chain" && customType != "internal" && customType != "internal-full" && type != "extracore";
    ui->address->setVisible(showAddressPort);
    ui->address_l->setVisible(showAddressPort);
    ui->port->setVisible(showAddressPort);
    ui->port_l->setVisible(showAddressPort);

    // 右边 stream
    auto stream = GetStreamSettings(ent->bean.get());
    if (stream != nullptr) {
        ui->right_all_w->setVisible(true);
        ui->network->setCurrentText(stream->network);
        ui->security->setCurrentText(stream->security);
        ui->packet_encoding->setCurrentText(stream->packet_encoding);
        ui->path->setText(stream->path);
        ui->host->setText(stream->host);
        ui->method->setText(stream->method);
        ui->sni->setText(stream->sni);
        ui->alpn->setText(stream->alpn);
        if (newEnt) {
            ui->utlsFingerprint->setCurrentText(NekoGui::dataStore->utlsFingerprint);
        } else {
            ui->utlsFingerprint->setCurrentText(stream->utlsFingerprint);
        }
        ui->insecure->setChecked(stream->allow_insecure);
        ui->header_type->setCurrentText(stream->header_type);
        ui->headers->setText(stream->headers);
        ui->ws_early_data_name->setText(stream->ws_early_data_name);
        ui->ws_early_data_length->setText(Int2String(stream->ws_early_data_length));
        ui->reality_pbk->setText(stream->reality_pbk);
        ui->reality_sid->setText(stream->reality_sid);
        ui->multiplex->setCurrentIndex(ent->bean->mux_state);
        ui->brutal_enable->setCheckState(ent->bean->enable_brutal ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
        ui->brutal_speed->setText(Int2String(ent->bean->brutal_speed));
        CACHE.certificate = stream->certificate;
    } else {
        ui->right_all_w->setVisible(false);
    }

    // left: custom
    CACHE.custom_config = ent->bean->custom_config;
    CACHE.custom_outbound = ent->bean->custom_outbound;
    bool show_custom_config = true;
    bool show_custom_outbound = true;
    if (type == "chain") {
        show_custom_outbound = false;
    } else if (type == "custom") {
        if (customType == "internal") {
            show_custom_outbound = false;
        } else if (customType == "internal-full") {
            show_custom_outbound = false;
            show_custom_config = false;
        }
    } else if (type == "extracore")
    {
        show_custom_outbound = false;
        show_custom_config = false;
    }
    ui->custom_box->setVisible(show_custom_outbound);
    ui->custom_global_box->setVisible(show_custom_config);

    // 左边 bean
    auto old = ui->bean->layout()->itemAt(0)->widget();
    ui->bean->layout()->removeWidget(old);
    innerWidget->layout()->setContentsMargins(0, 0, 0, 0);
    ui->bean->layout()->addWidget(innerWidget);
    ui->bean->setTitle(ent->bean->DisplayType());
    delete old;

    // 左边 bean inner editor
    innerEditor->get_edit_dialog = [&]() { return static_cast<QWidget*>(this); };
    innerEditor->get_edit_text_name = [&]() { return ui->name->text(); };
    innerEditor->get_edit_text_serverAddress = [&]() { return ui->address->text(); };
    innerEditor->get_edit_text_serverPort = [&]() { return ui->port->text(); };
    innerEditor->editor_cache_updated = [=] { editor_cache_updated_impl(); };
    innerEditor->onStart(ent);

    // 左边 common
    ui->name->setText(ent->bean->name);
    ui->address->setText(ent->bean->serverAddress);
    ui->port->setText(Int2String(ent->bean->serverPort));
    ui->port->setValidator(QRegExpValidator_Number);

    // 星号
    ADD_ASTERISK(this)

    if (type == "vmess" || type == "vless") {
        ui->packet_encoding->setVisible(true);
        ui->packet_encoding_l->setVisible(true);
    } else {
        ui->packet_encoding->setVisible(false);
        ui->packet_encoding_l->setVisible(false);
    }
    if (type == "vmess" || type == "vless" || type == "trojan") {
        ui->network_l->setVisible(true);
        ui->network->setVisible(true);
        ui->network_box->setVisible(true);
    } else {
        ui->network_l->setVisible(false);
        ui->network->setVisible(false);
        ui->network_box->setVisible(false);
    }
    if (type == "vmess" || type == "vless" || type == "trojan" || type == "http") {
        ui->security->setVisible(true);
        ui->security_l->setVisible(true);
    } else {
        ui->security->setVisible(false);
        ui->security_l->setVisible(false);
    }
    if (type == "vmess" || type == "vless" || type == "trojan" || type == "shadowsocks") {
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

    auto rightNoBox = (ui->stream_box->isHidden() && ui->network_box->isHidden() && ui->security_box->isHidden());
    if (rightNoBox && !ui->right_all_w->isHidden()) {
        ui->right_all_w->setVisible(false);
    }

    editor_cache_updated_impl();
    ADJUST_SIZE

    // 第一次显示
    if (isHidden()) {
        runOnUiThread([=] { show(); }, this);
    }
}

bool DialogEditProfile::onEnd() {
    // bean
    if (!innerEditor->onEnd()) {
        return false;
    }

    // 左边
    ent->bean->name = ui->name->text();
    ent->bean->serverAddress = ui->address->text().remove(' ');
    ent->bean->serverPort = ui->port->text().toInt();

    // 右边 stream
    auto stream = GetStreamSettings(ent->bean.get());
    if (stream != nullptr) {
        stream->network = ui->network->currentText();
        stream->security = ui->security->currentText();
        stream->packet_encoding = ui->packet_encoding->currentText();
        stream->path = ui->path->text();
        stream->host = ui->host->text();
        stream->sni = ui->sni->text();
        stream->alpn = ui->alpn->text();
        stream->utlsFingerprint = ui->utlsFingerprint->currentText();
        stream->allow_insecure = ui->insecure->isChecked();
        stream->headers = ui->headers->text();
        stream->header_type = ui->header_type->currentText();
        stream->method = ui->method->text();
        stream->ws_early_data_name = ui->ws_early_data_name->text();
        stream->ws_early_data_length = ui->ws_early_data_length->text().toInt();
        stream->reality_pbk = ui->reality_pbk->text();
        stream->reality_sid = ui->reality_sid->text();
        ent->bean->mux_state = ui->multiplex->currentIndex();
        ent->bean->enable_brutal = ui->brutal_enable->isChecked();
        ent->bean->brutal_speed = ui->brutal_speed->text().toInt();
        stream->certificate = CACHE.certificate;

        bool validHeaders;
        stream->GetHeaderPairs(&validHeaders);
        if (!validHeaders) {
            MW_show_log("Headers are not valid");
            return false;
        }
    }

    // cached custom
    ent->bean->custom_outbound = CACHE.custom_outbound;
    ent->bean->custom_config = CACHE.custom_config;

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
        auto ok = NekoGui::profileManager->AddProfile(ent);
        if (!ok) {
            MessageBoxWarning("???", "id exists");
        }
    } else {
        auto changed = ent->Save();
        if (changed && NekoGui::dataStore->started_id == ent->id) msg << "restart";
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
    if (CACHE.custom_outbound.isEmpty()) {
        ui->custom_outbound_edit->setText(tr("Not set"));
    } else {
        ui->custom_outbound_edit->setText(tr("Already set"));
    }
    if (CACHE.custom_config.isEmpty()) {
        ui->custom_config_edit->setText(tr("Not set"));
    } else {
        ui->custom_config_edit->setText(tr("Already set"));
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

void DialogEditProfile::on_custom_outbound_edit_clicked() {
    C_EDIT_JSON_ALLOW_EMPTY(custom_outbound)
    editor_cache_updated_impl();
}

void DialogEditProfile::on_custom_config_edit_clicked() {
    C_EDIT_JSON_ALLOW_EMPTY(custom_config)
    editor_cache_updated_impl();
}

void DialogEditProfile::on_certificate_edit_clicked() {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, tr("Certificate"), "", CACHE.certificate, &ok);
    if (ok) {
        CACHE.certificate = txt;
        editor_cache_updated_impl();
    }
}

void DialogEditProfile::on_apply_to_group_clicked() {
    if (apply_to_group_ui.empty()) {
        apply_to_group_ui[ui->multiplex] = new FloatCheckBox(ui->multiplex, this);
        apply_to_group_ui[ui->sni] = new FloatCheckBox(ui->sni, this);
        apply_to_group_ui[ui->alpn] = new FloatCheckBox(ui->alpn, this);
        apply_to_group_ui[ui->host] = new FloatCheckBox(ui->host, this);
        apply_to_group_ui[ui->path] = new FloatCheckBox(ui->path, this);
        apply_to_group_ui[ui->utlsFingerprint] = new FloatCheckBox(ui->utlsFingerprint, this);
        apply_to_group_ui[ui->insecure] = new FloatCheckBox(ui->insecure, this);
        apply_to_group_ui[ui->certificate_edit] = new FloatCheckBox(ui->certificate_edit, this);
        apply_to_group_ui[ui->custom_config_edit] = new FloatCheckBox(ui->custom_config_edit, this);
        apply_to_group_ui[ui->custom_outbound_edit] = new FloatCheckBox(ui->custom_outbound_edit, this);
        ui->apply_to_group->setText(tr("Confirm"));
    } else {
        auto group = NekoGui::profileManager->GetGroup(ent->gid);
        if (group == nullptr) {
            MessageBoxWarning("failed", "unknown group");
            return;
        }
        // save this
        if (onEnd()) {
            ent->Save();
        } else {
            MessageBoxWarning("failed", "failed to save");
            return;
        }
        // copy keys
        for (const auto &pair: apply_to_group_ui) {
            if (pair.second->isChecked()) {
                do_apply_to_group(group, pair.first);
            }
            delete pair.second;
        }
        apply_to_group_ui.clear();
        ui->apply_to_group->setText(tr("Apply settings to this group"));
    }
}

void DialogEditProfile::do_apply_to_group(const std::shared_ptr<NekoGui::Group> &group, QWidget *key) {
    auto stream = GetStreamSettings(ent->bean.get());

    auto copyStream = [=](void *p) {
        for (const auto &profile: group->GetProfileEnts()) {
            auto newStream = GetStreamSettings(profile->bean.get());
            if (newStream == nullptr) continue;
            if (stream == newStream) continue;
            newStream->_setValue(stream->_name(p), p);
            // qDebug() << newStream->ToJsonBytes();
            profile->Save();
        }
    };

    auto copyBean = [=](void *p) {
        for (const auto &profile: group->GetProfileEnts()) {
            if (profile == ent) continue;
            profile->bean->_setValue(ent->bean->_name(p), p);
            // qDebug() << profile->bean->ToJsonBytes();
            profile->Save();
        }
    };

    if (key == ui->multiplex) {
        copyStream(&ent->bean->mux_state);
    } else if (key == ui->brutal_enable) {
        copyStream(&ent->bean->enable_brutal);
    } else if (key == ui->brutal_speed) {
        copyStream(&ent->bean->brutal_speed);
    } else if (key == ui->sni) {
        copyStream(&stream->sni);
    } else if (key == ui->alpn) {
        copyStream(&stream->alpn);
    } else if (key == ui->host) {
        copyStream(&stream->host);
    } else if (key == ui->path) {
        copyStream(&stream->path);
    } else if (key == ui->utlsFingerprint) {
        copyStream(&stream->utlsFingerprint);
    } else if (key == ui->insecure) {
        copyStream(&stream->allow_insecure);
    } else if (key == ui->certificate_edit) {
        copyStream(&stream->certificate);
    } else if (key == ui->custom_config_edit) {
        copyBean(&ent->bean->custom_config);
    } else if (key == ui->custom_outbound_edit) {
        copyBean(&ent->bean->custom_outbound);
    }
}
