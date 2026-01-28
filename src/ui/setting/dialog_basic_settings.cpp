#include "include/ui/setting/dialog_basic_settings.h"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/Icon.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/global/HTTPRequestHelper.hpp"
#include "include/global/DeviceDetailsHelper.hpp"

#include <QStyleFactory>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <qfontdatabase.h>



#include "include/ui/mainwindow.h"

DialogBasicSettings::DialogBasicSettings(QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogBasicSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);

    // Common
    ui->inbound_socks_port_l->setText(ui->inbound_socks_port_l->text().replace("Socks", "Mixed (SOCKS+HTTP)"));
    ui->log_level->addItems(QString("trace debug info warn error fatal panic").split(" "));
    ui->xray_loglevel->addItems(Configs::Xray::XrayLogLevels);
    ui->mux_protocol->addItems({"h2mux", "smux", "yamux"});
    ui->disable_stats->setChecked(Configs::dataManager->settingsRepo->disable_traffic_stats);
    ui->proxy_scheme->setCurrentText(Configs::dataManager->settingsRepo->proxy_scheme);

    D_LOAD_STRING(inbound_address)
    D_LOAD_COMBO_STRING(log_level)
    CACHE.custom_inbound = Configs::dataManager->settingsRepo->custom_inbound;
    D_LOAD_INT(inbound_socks_port)
    ui->random_listen_port->setChecked(Configs::dataManager->settingsRepo->random_inbound_port);
    D_LOAD_INT(test_concurrent)
    D_LOAD_STRING(test_latency_url)
    D_LOAD_BOOL(disable_tray)
    ui->url_timeout->setText(Int2String(Configs::dataManager->settingsRepo->url_test_timeout_ms));
    ui->speedtest_mode->setCurrentIndex(Configs::dataManager->settingsRepo->speed_test_mode);
    ui->test_timeout->setText(Int2String(Configs::dataManager->settingsRepo->speed_test_timeout_ms));
    ui->simple_down_url->setText(Configs::dataManager->settingsRepo->simple_dl_url);
    ui->allow_beta->setChecked(Configs::dataManager->settingsRepo->allow_beta_update);

    connect(ui->custom_inbound_edit, &QPushButton::clicked, this, [=,this] {
        C_EDIT_JSON_ALLOW_EMPTY(custom_inbound)
    });
    connect(ui->disable_tray, &QCheckBox::stateChanged, this, [=,this](const bool &) {
        CACHE.updateDisableTray = true;
    });
    connect(ui->random_listen_port, &QCheckBox::stateChanged, this, [=,this](const bool &state)
    {
        if (state)
        {
            ui->inbound_socks_port->setDisabled(true);
        } else
        {
            ui->inbound_socks_port->setDisabled(false);
        }
    });

#ifndef Q_OS_WIN
    ui->proxy_scheme_l->hide();
    ui->proxy_scheme->hide();
    ui->windows_no_admin->hide();
#endif

    // Style
    ui->connection_statistics->setChecked(Configs::dataManager->settingsRepo->enable_stats);
    ui->show_sys_dns->setChecked(Configs::dataManager->settingsRepo->show_system_dns);
    connect(ui->show_sys_dns, &QCheckBox::stateChanged, this, [=]
    {
        CACHE.updateSystemDns = true;
    });
#ifndef Q_OS_WIN
    ui->show_sys_dns->hide();
#endif
    //
    D_LOAD_BOOL(start_minimal)
    D_LOAD_INT(max_log_line)
    //
    ui->language->setCurrentIndex(Configs::dataManager->settingsRepo->language);
    connect(ui->language, &QComboBox::currentIndexChanged, this, [=,this](int index) {
        CACHE.needRestart = true;
    });
    connect(ui->font, &QComboBox::currentTextChanged, this, [=,this](const QString &fontName) {
        auto font = qApp->font();
        font.setFamily(fontName);
        qApp->setFont(font);
        Configs::dataManager->settingsRepo->font = fontName;
        Configs::dataManager->settingsRepo->Save();
        adjustSize();
    });
    for (int i=7;i<=26;i++) {
        ui->font_size->addItem(Int2String(i));
    }
    ui->font_size->setCurrentText(Int2String(qApp->font().pointSize()));
    connect(ui->font_size, &QComboBox::currentTextChanged, this, [=,this](const QString &sizeStr) {
        auto font = qApp->font();
        font.setPointSize(sizeStr.toInt());
        qApp->setFont(font);
        Configs::dataManager->settingsRepo->font_size = sizeStr.toInt();
        Configs::dataManager->settingsRepo->Save();
        adjustSize();
    });
    //
    ui->theme->addItems(QStyleFactory::keys());
    ui->theme->addItem("QDarkStyle");
    ui->enable_custom_icon->setChecked(Configs::dataManager->settingsRepo->use_custom_icons);
    connect(ui->select_custom_icon, &QPushButton::clicked, this, [=, this] {
        auto n = QMessageBox::information(this, "Custom Icon Manual", tr(Configs::Information::CustomIconManual.toStdString().c_str()), QMessageBox::Open | QMessageBox::Cancel);
        if (n == QMessageBox::Open) {
            auto fileNames = QFileDialog::getOpenFileNames(this,
                tr("Select png icons"), QDir::homePath(), tr("Image Files (*.png)"));
            // process files
            QString errors;
            for (const auto& fileName : fileNames) {
                CACHE.updateTrayIcon = true;
                QFileInfo fileInfo(fileName);
                if (auto pixMap = QPixmap(fileName); pixMap.isNull()) errors += "Failed to load " + fileName + "\n";
                else if (pixMap.width() != pixMap.height()) errors += "Image does not have equal width and height: " + fileName + "\n";
                else if (!Configs::Information::iconNames.contains(fileInfo.fileName())) errors += "Icon name is not valid: " + fileInfo.fileName() + "\n";
                else {
                    QFile::remove(QDir("icons").filePath(fileInfo.fileName()));
                    if (!QFile::copy(fileName, QDir("icons").filePath(fileInfo.fileName()))) errors += "Failed to copy " + fileName + "\n";
                }
            }
            if (!errors.isEmpty()) {
                QMessageBox::warning(this, "Select custom image error", errors);
            }
        }
    });
    //
    bool ok;
    auto themeId = Configs::dataManager->settingsRepo->theme.toInt(&ok);
    if (ok) {
        ui->theme->setCurrentIndex(themeId);
    } else {
        ui->theme->setCurrentText(Configs::dataManager->settingsRepo->theme);
    }
    //
    connect(ui->theme, &QComboBox::currentIndexChanged, this, [=,this](int index) {
        themeManager->ApplyTheme(ui->theme->currentText());
        Configs::dataManager->settingsRepo->theme = ui->theme->currentText();
        Configs::dataManager->settingsRepo->Save();
    });

    // Subscription

    ui->user_agent->setText(Configs::dataManager->settingsRepo->user_agent);
    ui->user_agent->setPlaceholderText(Configs::dataManager->settingsRepo->GetUserAgent(true));
    D_LOAD_BOOL(net_use_proxy)
    D_LOAD_BOOL(sub_clear)
    D_LOAD_BOOL(net_insecure)
    D_LOAD_BOOL(sub_send_hwid)
    D_LOAD_STRING(sub_custom_hwid_params)
    D_LOAD_INT_ENABLE(sub_auto_update, sub_auto_update_enable)
    auto details = GetDeviceDetails();
	ui->sub_send_hwid->setToolTip(
        ui->sub_send_hwid->toolTip()
            .arg(details.hwid.isEmpty() ? "N/A" : details.hwid,
                details.os.isEmpty() ? "N/A" : details.os,
                details.osVersion.isEmpty() ? "N/A" : details.osVersion,
                details.model.isEmpty() ? "N/A" : details.model));

    // Mux
    D_LOAD_INT(mux_concurrency)
    D_LOAD_COMBO_STRING(mux_protocol)
    D_LOAD_BOOL(mux_padding)
    D_LOAD_BOOL(mux_default_on)

    // Xray
    ui->xray_loglevel->setCurrentText(Configs::dataManager->settingsRepo->xray_log_level);
    ui->xray_mux_concurrency->setText(Int2String(Configs::dataManager->settingsRepo->xray_mux_concurrency));
    ui->xray_default_mux->setChecked(Configs::dataManager->settingsRepo->xray_mux_default_on);

    // NTP
    ui->ntp_enable->setChecked(Configs::dataManager->settingsRepo->enable_ntp);
    ui->ntp_server->setEnabled(Configs::dataManager->settingsRepo->enable_ntp);
    ui->ntp_port->setEnabled(Configs::dataManager->settingsRepo->enable_ntp);
    ui->ntp_interval->setEnabled(Configs::dataManager->settingsRepo->enable_ntp);
    ui->ntp_server->setText(Configs::dataManager->settingsRepo->ntp_server_address);
    ui->ntp_port->setText(Int2String(Configs::dataManager->settingsRepo->ntp_server_port));
    ui->ntp_interval->setCurrentText(Configs::dataManager->settingsRepo->ntp_interval);
    connect(ui->ntp_enable, &QCheckBox::stateChanged, this, [=,this](const bool &state) {
        ui->ntp_server->setEnabled(state);
        ui->ntp_port->setEnabled(state);
        ui->ntp_interval->setEnabled(state);
    });

    // Security

    ui->utlsFingerprint->addItems(Configs::tlsFingerprints);
    ui->disable_priv_req->setChecked(Configs::dataManager->settingsRepo->disable_privilege_req);
    ui->windows_no_admin->setChecked(Configs::dataManager->settingsRepo->disable_run_admin);
    ui->mozilla_cert->setChecked(Configs::dataManager->settingsRepo->use_mozilla_certs);

    D_LOAD_BOOL(skip_cert)
    ui->utlsFingerprint->setCurrentText(Configs::dataManager->settingsRepo->utlsFingerprint);
}

DialogBasicSettings::~DialogBasicSettings() {
    delete ui;
}

void DialogBasicSettings::accept() {
    // Common
    bool needChoosePort = false;

    D_SAVE_STRING(inbound_address)
    D_SAVE_COMBO_STRING(log_level)
    Configs::dataManager->settingsRepo->custom_inbound = CACHE.custom_inbound;
    D_SAVE_INT(inbound_socks_port)
    if (!Configs::dataManager->settingsRepo->random_inbound_port && ui->random_listen_port->isChecked())
    {
        needChoosePort = true;
    }
    Configs::dataManager->settingsRepo->random_inbound_port = ui->random_listen_port->isChecked();
    D_SAVE_INT(test_concurrent)
    D_SAVE_STRING(test_latency_url)
    D_SAVE_BOOL(disable_tray)
    Configs::dataManager->settingsRepo->proxy_scheme = ui->proxy_scheme->currentText().toLower();
    Configs::dataManager->settingsRepo->speed_test_mode = ui->speedtest_mode->currentIndex();
    Configs::dataManager->settingsRepo->simple_dl_url = ui->simple_down_url->text();
    Configs::dataManager->settingsRepo->url_test_timeout_ms = ui->url_timeout->text().toInt();
    Configs::dataManager->settingsRepo->speed_test_timeout_ms = ui->test_timeout->text().toInt();
    Configs::dataManager->settingsRepo->allow_beta_update = ui->allow_beta->isChecked();

    // Style

    Configs::dataManager->settingsRepo->enable_stats = ui->connection_statistics->isChecked();
    Configs::dataManager->settingsRepo->language = ui->language->currentIndex();
    auto oldUseCustomIcon = Configs::dataManager->settingsRepo->use_custom_icons;
    Configs::dataManager->settingsRepo->use_custom_icons = ui->enable_custom_icon->isChecked();
    if (oldUseCustomIcon != Configs::dataManager->settingsRepo->use_custom_icons) CACHE.updateTrayIcon = true;
    D_SAVE_BOOL(start_minimal)
    D_SAVE_INT(max_log_line)
    Configs::dataManager->settingsRepo->show_system_dns = ui->show_sys_dns->isChecked();

    if (Configs::dataManager->settingsRepo->max_log_line <= 0) {
        Configs::dataManager->settingsRepo->max_log_line = 200;
    }

    // Subscription

    if (ui->sub_auto_update_enable->isChecked()) {
        TM_auto_update_subsctiption_Reset_Minute(ui->sub_auto_update->text().toInt());
    } else {
        TM_auto_update_subsctiption_Reset_Minute(0);
    }

    Configs::dataManager->settingsRepo->user_agent = ui->user_agent->text();
    D_SAVE_BOOL(net_use_proxy)
    D_SAVE_BOOL(sub_clear)
    D_SAVE_BOOL(net_insecure)
    D_SAVE_BOOL(sub_send_hwid)
    D_SAVE_STRING(sub_custom_hwid_params)
    D_SAVE_INT_ENABLE(sub_auto_update, sub_auto_update_enable)

    // Core
    Configs::dataManager->settingsRepo->disable_traffic_stats = ui->disable_stats->isChecked();

    // Xray
    Configs::dataManager->settingsRepo->xray_log_level = ui->xray_loglevel->currentText();
    Configs::dataManager->settingsRepo->xray_mux_concurrency = ui->xray_mux_concurrency->text().toInt();
    Configs::dataManager->settingsRepo->xray_mux_default_on = ui->xray_default_mux->isChecked();

    // Mux
    D_SAVE_INT(mux_concurrency)
    D_SAVE_COMBO_STRING(mux_protocol)
    D_SAVE_BOOL(mux_padding)
    D_SAVE_BOOL(mux_default_on)

    // NTP
    Configs::dataManager->settingsRepo->enable_ntp = ui->ntp_enable->isChecked();
    Configs::dataManager->settingsRepo->ntp_server_address = ui->ntp_server->text();
    Configs::dataManager->settingsRepo->ntp_server_port = ui->ntp_port->text().toInt();
    Configs::dataManager->settingsRepo->ntp_interval = ui->ntp_interval->currentText();

    // Security

    D_SAVE_BOOL(skip_cert)
    Configs::dataManager->settingsRepo->utlsFingerprint = ui->utlsFingerprint->currentText();
    Configs::dataManager->settingsRepo->disable_privilege_req = ui->disable_priv_req->isChecked();
    Configs::dataManager->settingsRepo->disable_run_admin = ui->windows_no_admin->isChecked();
    Configs::dataManager->settingsRepo->use_mozilla_certs = ui->mozilla_cert->isChecked();

    QStringList str{"UpdateConfigs::dataManager->settingsRepo"};
    if (CACHE.needRestart) str << "NeedRestart";
    if (CACHE.updateDisableTray) str << "UpdateDisableTray";
    if (CACHE.updateSystemDns) str << "UpdateSystemDns";
    if (CACHE.updateTrayIcon) str << "UpdateTrayIcon";
    if (needChoosePort) str << "NeedChoosePort";
    MW_dialog_message(Dialog_DialogBasicSettings, str.join(","));
    QDialog::accept();
}

void DialogBasicSettings::on_core_settings_clicked() {
    auto w = new QDialog(this);
    w->setWindowTitle(software_core_name + " Core Options");
    auto layout = new QGridLayout;
    w->setLayout(layout);
    //
    auto line = -1;
    MyLineEdit *core_box_clash_api;
    MyLineEdit *core_box_clash_api_secret;
    MyLineEdit *core_box_clash_listen_addr;
    //
    auto core_box_clash_listen_addr_l = new QLabel("Clash Api Listen Address");
    core_box_clash_listen_addr = new MyLineEdit;
    core_box_clash_listen_addr->setText(Configs::dataManager->settingsRepo->core_box_clash_listen_addr);
    layout->addWidget(core_box_clash_listen_addr_l, ++line, 0);
    layout->addWidget(core_box_clash_listen_addr, line, 1);
    //
    auto core_box_clash_api_l = new QLabel("Clash API Listen Port");
    core_box_clash_api = new MyLineEdit;
    core_box_clash_api->setText(Configs::dataManager->settingsRepo->core_box_clash_api > 0 ? Int2String(Configs::dataManager->settingsRepo->core_box_clash_api) : "");
    layout->addWidget(core_box_clash_api_l, ++line, 0);
    layout->addWidget(core_box_clash_api, line, 1);
    //
    auto core_box_clash_api_secret_l = new QLabel("Clash API Secret");
    core_box_clash_api_secret = new MyLineEdit;
    core_box_clash_api_secret->setText(Configs::dataManager->settingsRepo->core_box_clash_api_secret);
    layout->addWidget(core_box_clash_api_secret_l, ++line, 0);
    layout->addWidget(core_box_clash_api_secret, line, 1);
    //
    auto box = new QDialogButtonBox;
    box->setOrientation(Qt::Horizontal);
    box->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    connect(box, &QDialogButtonBox::accepted, w, [=,this] {
        Configs::dataManager->settingsRepo->core_box_clash_api = core_box_clash_api->text().toInt();
        Configs::dataManager->settingsRepo->core_box_clash_listen_addr = core_box_clash_listen_addr->text();
        Configs::dataManager->settingsRepo->core_box_clash_api_secret = core_box_clash_api_secret->text();
        MW_dialog_message(Dialog_DialogBasicSettings, "UpdateConfigs::dataManager->settingsRepo");
        w->accept();
    });
    connect(box, &QDialogButtonBox::rejected, w, &QDialog::reject);
    layout->addWidget(box, ++line, 1);
    //
    ADD_ASTERISK(w)
    w->exec();
    w->deleteLater();
}
