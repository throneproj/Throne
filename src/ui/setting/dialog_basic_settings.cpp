#include "include/ui/setting/dialog_basic_settings.h"

#include "include/ui/widget/json/JsonEditorDialog.h"
#include "include/ui/widget/json/SchemaStore.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/Icon.hpp"
#include "include/ui/utils/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/global/DeviceDetailsHelper.hpp"
#include "include/global/ExitLookup.hpp"
#include "include/global/Validation.hpp"
#include "include/database/entities/Group.h"
#include "include/engine/services/AssetUpdates.h"
#include "include/engine/services/BackupArchive.h"
#include "include/ui/utils/ValidationForm.h"

#include <QStyleFactory>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QTimer>
#include <QTextBlock>
#include <QTextCursor>
#include <qfontdatabase.h>
#include <QJsonObject>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>
#include <QCheckBox>
#include <QScreen>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSslSocket>
#include <QStandardItemModel>

#include <algorithm>
#include <limits>

#include "include/sys/UrlScheme.hpp"
#include "include/ui/mainwindow.h"
#include "include/ui/setting/DiagnosticsTab.h"

namespace {
    constexpr int kBasicSettingsMinAutoUpdate = 30;
    constexpr int kBasicSettingsMaxAutoUpdate = 525600;

    // The test runners read a bare integer; anything else, "3s" included, becomes 0.
    QString basicSettingsMillisError(const QString &text, int example, bool zeroIsDefault) {
        if (const auto ms = Validation::ToInt(text); ms && *ms >= (zeroIsDefault ? 0 : 1) && *ms <= std::numeric_limits<int>::max()) return {};
        if (zeroIsDefault)
            return DialogBasicSettings::tr("Enter a whole number of milliseconds, for example %1 (without a unit), or 0 for the default.").arg(example);
        return DialogBasicSettings::tr("Enter a whole number of milliseconds, for example %1 (without a unit).").arg(example);
    }

    // GroupUpdater sends only values below 1000 characters without line breaks, and silently falls back otherwise.
    QString basicSettingsHeaderValueError(const QString &text) {
        if (text.size() < 1000 && !text.contains(QLatin1Char('\n')) && !text.contains(QLatin1Char('\r'))) return {};
        return DialogBasicSettings::tr("Must be shorter than 1000 characters, on a single line.");
    }

    // The keys GroupUpdater's applyCustomHwidParams reads; it skips anything else without a word.
    QString basicSettingsHwidParamsError(const QString &text) {
        static const QStringList keys = {QStringLiteral("hwid"), QStringLiteral("os"), QStringLiteral("osversion"), QStringLiteral("model")};
        for (const auto &item : Validation::Items(text, Validation::Split::Comma)) {
            const auto eq = item.indexOf(QLatin1Char('='));
            const auto value = eq > 0 ? item.mid(eq + 1).trimmed() : QString();
            if (eq > 0 && keys.contains(item.left(eq).trimmed().toLower()) && !value.isEmpty() && basicSettingsHeaderValueError(value).isEmpty())
                continue;
            return DialogBasicSettings::tr("Unknown or malformed parameter \"%1\". Use hwid=…, os=…, osVersion=…, model=… separated by commas.")
                .arg(item.size() > 40 ? item.left(39) + QChar(0x2026) : item);
        }
        return {};
    }

    // Same per-line wording as Validation::Form::bindLines, which takes only a QPlainTextEdit.
    QString basicSettingsRegexLinesError(const QString &text) {
        const auto lines = text.split(QLatin1Char('\n'));
        QString first;
        int bad = 0;
        for (qsizetype i = 0; i < lines.size(); ++i) {
            const auto line = lines[i].trimmed();
            const auto problem = Validation::Regex(line);
            if (problem.isEmpty()) continue;
            if (bad++ == 0) {
                first = QCoreApplication::translate("Validation", "Line %1 (\"%2\"): %3")
                            .arg(QString::number(i + 1), line.size() > 40 ? line.left(39) + QChar(0x2026) : line, problem);
            }
        }
        if (bad > 1) first += QLatin1Char(' ') + QCoreApplication::translate("Validation", "%n more line(s) have problems.", nullptr, bad - 1);
        return first;
    }

    // Underlines the lines that do not compile; extra selections stay off the undo stack.
    void basicSettingsMarkRegexLines(QTextEdit *edit) {
        QList<QTextEdit::ExtraSelection> selections;
        QTextCharFormat format;
        format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        format.setUnderlineColor(themeManager()->tokens.danger.isValid() ? themeManager()->tokens.danger : QColor(Qt::red));
        for (QTextBlock block = edit->document()->begin(); block.isValid(); block = block.next()) {
            if (Validation::Regex(block.text()).isEmpty()) continue;
            QTextEdit::ExtraSelection selection;
            selection.cursor = QTextCursor(block);
            selection.cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            selection.format = format;
            selections.append(selection);
        }
        edit->setExtraSelections(selections);
    }

    // BackupArchive answers in English with these texts; the dialog shows their translations.
    const char *const kBasicSettingsBackupErrors[] = {
        QT_TRANSLATE_NOOP("DialogBasicSettings", "Failed to create database snapshot: %1"),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "Failed to read database snapshot."),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "Cannot write to: %1"),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "Cannot open backup file: %1"),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "Not a valid Throne backup file."),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "Unsupported backup format version: %1.\nThis backup may have been created with a newer version of the application."),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "The backup file is damaged or incomplete."),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "This backup file does not contain any restorable data."),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "Failed to create temporary file for restore."),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "The backup's database is damaged: %1"),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "Failed to restore database: %1"),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "There is nothing to back up"),
        QT_TRANSLATE_NOOP("DialogBasicSettings", "A subscription update is running; try again when it finishes."),
    };

    QString basicSettingsBackupError(const QString &error) {
        for (const char *source : kBasicSettingsBackupErrors) {
            const auto text = QString::fromUtf8(source);
            const auto at = text.indexOf(QLatin1String("%1"));
            if (at < 0) {
                if (error == text) return DialogBasicSettings::tr(source);
                continue;
            }
            const auto prefix = text.left(at);
            const auto suffix = text.mid(at + 2);
            if (error.size() >= prefix.size() + suffix.size() && error.startsWith(prefix) && error.endsWith(suffix))
                return DialogBasicSettings::tr(source).arg(error.mid(prefix.size(), error.size() - prefix.size() - suffix.size()));
        }
        return error;
    }
}

DialogBasicSettings::DialogBasicSettings(QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogBasicSettings) {
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    ADD_ASTERISK(this);

    ui->inbound_socks_port_l->setText(ui->inbound_socks_port_l->text().replace("Socks", "Mixed (SOCKS+HTTP)"));
    ui->log_level->addItems(QString("trace debug info warn error fatal panic").split(" "));
    ui->xray_loglevel->addItems(Configs::Xray::XrayLogLevels);
    ui->disable_stats->setChecked(Configs::dataManager->settingsRepo->disable_traffic_stats);
    ui->proxy_scheme->setCurrentText(Configs::dataManager->settingsRepo->proxy_scheme);

    D_LOAD_STRING(inbound_address)
    CACHE.custom_inbound = Configs::dataManager->settingsRepo->custom_inbound;
    D_LOAD_INT(inbound_socks_port)
    ui->random_listen_port->setChecked(Configs::dataManager->settingsRepo->random_inbound_port);
    D_LOAD_INT(test_concurrent)
    D_LOAD_STRING(test_latency_url)
    D_LOAD_STRING(direct_test_url)
    D_LOAD_BOOL(exit_country_lookup)
    ui->exit_lookup_provider->addItem(tr("Automatic"), Configs_network::ExitLookupAutoId);
    for (const auto &provider : Configs_network::ExitLookupProviders()) ui->exit_lookup_provider->addItem(provider.label, provider.id);
    ui->exit_lookup_provider->setCurrentIndex(
        std::max(0, ui->exit_lookup_provider->findData(Configs::dataManager->settingsRepo->exit_lookup_provider)));
    ui->label_exit_lookup_provider->setEnabled(ui->exit_country_lookup->isChecked());
    ui->exit_lookup_provider->setEnabled(ui->exit_country_lookup->isChecked());
    connect(ui->exit_country_lookup, &QCheckBox::toggled, this, [this](bool enabled) {
        ui->label_exit_lookup_provider->setEnabled(enabled);
        ui->exit_lookup_provider->setEnabled(enabled);
    });
    D_LOAD_BOOL(disable_tray)
    ui->reset_proxy_on_disable_sp->setChecked(Configs::dataManager->settingsRepo->reset_proxy_on_disable_sp);
    ui->url_timeout->setText(Int2String(Configs::dataManager->settingsRepo->url_test_timeout_ms));
    ui->speedtest_mode->setCurrentIndex(Configs::dataManager->settingsRepo->speed_test_mode);
    ui->test_timeout->setText(Int2String(Configs::dataManager->settingsRepo->speed_test_timeout_ms));
    ui->simple_down_url->setText(Configs::dataManager->settingsRepo->simple_dl_url);
    ui->allow_beta->setChecked(Configs::dataManager->settingsRepo->allow_beta_update);
    ui->disable_mixed_inbound->setChecked(Configs::dataManager->settingsRepo->disable_mixed_inbound);
    D_LOAD_BOOL(inbound_auth)
    D_LOAD_STRING(inbound_user)
    D_LOAD_STRING(inbound_pass)

    ui->url_scheme_auto_register->setChecked(Configs::dataManager->settingsRepo->url_scheme_auto_register);
    ui->file_assoc_auto_register->setChecked(Configs::dataManager->settingsRepo->file_assoc_auto_register);
    const auto connectAssociation = [this](Association a, QCheckBox *autoRegister, QPushButton *install, QPushButton *uninstall, const QString &failure) {
        connect(install, &QPushButton::clicked, this, [=,this] {
            const bool ok = UrlScheme_Install(a);
            refreshUrlSchemeStatus();
            if (!ok) QMessageBox::warning(this, ui->url_scheme_box->title(), failure);
        });
        connect(uninstall, &QPushButton::clicked, this, [=,this] {
            UrlScheme_Uninstall(a);
            autoRegister->setChecked(false);
            refreshUrlSchemeStatus();
        });
    };
    connectAssociation(Association::Links, ui->url_scheme_auto_register, ui->url_scheme_install, ui->url_scheme_uninstall,
                       tr("Could not register the handler for throne:// links."));
    connectAssociation(Association::ConfigFiles, ui->file_assoc_auto_register, ui->file_assoc_install, ui->file_assoc_uninstall,
                       tr("Could not register the config file associations."));
#ifdef Q_OS_MACOS
    // LaunchServices registers the scheme from the bundle's Info.plist, so there is nothing of ours to add or take back.
    ui->url_scheme_install->hide();
    ui->url_scheme_uninstall->hide();
#endif
    if (!UrlScheme_IsSupported(Association::ConfigFiles)) {
        ui->file_assoc_auto_register->hide();
        ui->file_assoc_status->hide();
        ui->file_assoc_install->hide();
        ui->file_assoc_uninstall->hide();
    }
    ui->url_scheme_box->setEnabled(UrlScheme_IsSupported(Association::Links));
    refreshUrlSchemeStatus();

    connect(ui->custom_inbound_edit, &QPushButton::clicked, this, [=,this] {
        C_EDIT_JSON_ALLOW_EMPTY(custom_inbound, JsonEdit::SingBox::Config)
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

    ui->max_log_line->setText(QString::number(Configs::dataManager->settingsRepo->max_log_line));
    ui->log_level->setCurrentText(Configs::dataManager->settingsRepo->log_level);
    ui->xray_loglevel->setCurrentText(Configs::dataManager->settingsRepo->xray_log_level);
    ui->enable_log_include->setChecked(Configs::dataManager->settingsRepo->log_enable_include);
    ui->enable_log_exclude->setChecked(Configs::dataManager->settingsRepo->log_enable_exclude);
    ui->log_include_keyword->setText(Configs::dataManager->settingsRepo->log_include_keyword.join("\n"));
    ui->log_exclude_keyword->setText(Configs::dataManager->settingsRepo->log_exclude_keyword.join("\n"));
    ui->log_include_regex->setText(Configs::dataManager->settingsRepo->log_include_regex.join("\n"));
    ui->log_exclude_regex->setText(Configs::dataManager->settingsRepo->log_exclude_regex.join("\n"));
    applyRegexHighlighting();

    ui->connection_statistics->setChecked(Configs::dataManager->settingsRepo->enable_stats);
    ui->disable_traffic_aggregation->setChecked(Configs::dataManager->settingsRepo->disable_traffic_aggregation);
    D_LOAD_BOOL(start_minimal)
    ui->skip_delete_confirm->setChecked(Configs::dataManager->settingsRepo->skip_delete_confirmation);
    D_LOAD_BOOL(show_config_security)
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
    ui->log_font_family->setCurrentFont(QFont(ResolveLogFontFamily(Configs::dataManager->settingsRepo->log_font_family)));
    CACHE.shownLogFontFamily = ui->log_font_family->currentFont().family();
    for (int i=7;i<=26;i++) {
        ui->log_font_size->addItem(Int2String(i));
    }
    const int logFontSize = Configs::dataManager->settingsRepo->log_font_size;
    ui->log_font_size->setCurrentText(Int2String(logFontSize > 0 ? logFontSize : qApp->font().pointSize()));
    CACHE.shownLogFontSize = ui->log_font_size->currentText().toInt();
    ui->theme->addItems(QStyleFactory::keys());
    ui->theme->addItem("QDarkStyle");
    // Custom stylesheet themes, not QStyleFactory keys.
    ui->theme->addItems({"FlatGray", "LightBlue", "SoftPink", "BlackSoft"});
    ui->enable_custom_icon->setChecked(Configs::dataManager->settingsRepo->use_custom_icons);
    ui->follow_status_in_taskbar->setChecked(Configs::dataManager->settingsRepo->follow_status_in_taskbar);
    ui->follow_status_in_taskbar->setEnabled(ui->enable_custom_icon->isChecked());
    connect(ui->enable_custom_icon, &QCheckBox::toggled, this, [this](bool enabled) {
        ui->follow_status_in_taskbar->setEnabled(enabled);
    });
    connect(ui->select_custom_icon, &QPushButton::clicked, this, [=, this] {
        auto n = QMessageBox::information(this, "Custom Icon Manual", tr(Configs::Information::CustomIconManual.toStdString().c_str()), QMessageBox::Open | QMessageBox::Cancel);
        if (n == QMessageBox::Open) {
            auto fileNames = QFileDialog::getOpenFileNames(this,
                tr("Select png icons"), QDir::homePath(), tr("Image Files (*.png)"));
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
    bool ok;
    auto themeId = Configs::dataManager->settingsRepo->theme.toInt(&ok);
    if (ok) {
        ui->theme->setCurrentIndex(themeId);
    } else {
        ui->theme->setCurrentText(Configs::dataManager->settingsRepo->theme);
    }
    connect(ui->theme, &QComboBox::currentIndexChanged, this, [=,this](int index) {
        themeManager()->ApplyTheme(ui->theme->currentText());
        Configs::dataManager->settingsRepo->theme = ui->theme->currentText().trimmed();
        Configs::dataManager->settingsRepo->Save();
        refreshUrlSchemeStatus();
    });

    ui->user_agent->setText(Configs::dataManager->settingsRepo->user_agent);
    ui->user_agent->setPlaceholderText(Configs::dataManager->settingsRepo->GetUserAgent(true));
    ui->sub_tls_version->setCurrentIndex(std::clamp(Configs::dataManager->settingsRepo->sub_tls_version, 0, ui->sub_tls_version->count() - 1));
    ui->sub_http_version->setCurrentIndex(std::clamp(Configs::dataManager->settingsRepo->sub_http_version, 0, ui->sub_http_version->count() - 1));
    if (auto *model = qobject_cast<QStandardItemModel *>(ui->sub_tls_version->model()); model && !QSslSocket::isProtocolSupported(QSsl::TlsV1_3)) {
        model->item(static_cast<int>(Configs::subTlsVersion::tls13))->setEnabled(false);
    }
    D_LOAD_BOOL(net_use_proxy)
    D_LOAD_BOOL(allow_stopping_active_profile)
    D_LOAD_BOOL(sub_clear)
    D_LOAD_BOOL(sub_show_change_popup)
    D_LOAD_BOOL(net_insecure)
    D_LOAD_BOOL(sub_send_hwid)
    D_LOAD_STRING(sub_custom_hwid_params)
    D_LOAD_INT_ENABLE(sub_auto_update, sub_auto_update_enable)
    D_LOAD_BOOL(sub_respect_server_interval)
    D_LOAD_INT_ENABLE(route_auto_update, route_auto_update_enable)
    auto details = GetDeviceDetails();
	ui->sub_send_hwid->setToolTip(
        ui->sub_send_hwid->toolTip()
            .arg(details.hwid.isEmpty() ? "N/A" : details.hwid,
                details.os.isEmpty() ? "N/A" : details.os,
                details.osVersion.isEmpty() ? "N/A" : details.osVersion,
                details.model.isEmpty() ? "N/A" : details.model));

    ui->dns_in_port->setValidator(new QIntValidator(1, 65535, ui->dns_in_port));
    ui->dns_in_port->setText(Int2String(Configs::dataManager->settingsRepo->core_dns_in_port));

    ui->core_box_clash_listen_addr->setText(Configs::dataManager->settingsRepo->core_box_clash_listen_addr);
    ui->core_box_clash_listen_addr->setPlaceholderText(tr("127.0.0.1 (default)"));
    ui->core_box_clash_api->setValidator(new QIntValidator(1, 65535, ui->core_box_clash_api));
    ui->core_box_clash_api->setText(Configs::dataManager->settingsRepo->core_box_clash_api > 0
                                        ? Int2String(Configs::dataManager->settingsRepo->core_box_clash_api)
                                        : "");
    ui->core_box_clash_api_secret->setText(Configs::dataManager->settingsRepo->core_box_clash_api_secret);

    ui->core_box_api_port->setValidator(new QIntValidator(1, 65535, ui->core_box_api_port));
    ui->core_box_api_port->setText(Configs::dataManager->settingsRepo->core_box_api_port > 0
                                       ? Int2String(Configs::dataManager->settingsRepo->core_box_api_port)
                                       : "");
    ui->core_box_api_secret->setText(Configs::dataManager->settingsRepo->core_box_api_secret);
    connect(ui->core_box_api_regen, &QPushButton::clicked, this, [this] {
        ui->core_box_api_secret->setText(QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-'));
    });

    ui->vless_xray_pref->addItems(Configs::Xray::XrayVlessPreferenceString);
    ui->vless_xray_pref->setCurrentIndex(Configs::dataManager->settingsRepo->xray_vless_preference);
    auto populateGeoAssetCombo = [](QComboBox *combo, bool geoip, const QString &current, const QStringList &history) {
        for (const auto &p: Configs::Xray::XrayGeoAssetProviders) {
            combo->addItem(geoip ? p.geoip : p.geosite);
            combo->setItemData(combo->count() - 1, p.name, Qt::ToolTipRole);
        }
        for (const auto &h: history) {
            if (combo->findText(h) < 0) combo->addItem(h);
        }
        const auto &fallback = Configs::Xray::XrayGeoAssetProviders.first();
        combo->lineEdit()->setPlaceholderText(geoip ? fallback.geoip : fallback.geosite);
        combo->setCurrentText(current);
    };
    populateGeoAssetCombo(ui->xray_geoip_url, true,
                          Configs::dataManager->settingsRepo->xray_geoip_url,
                          Configs::dataManager->settingsRepo->xray_geoip_url_history);
    populateGeoAssetCombo(ui->xray_geosite_url, false,
                          Configs::dataManager->settingsRepo->xray_geosite_url,
                          Configs::dataManager->settingsRepo->xray_geosite_url_history);

    ui->ntp_enable->setChecked(Configs::dataManager->settingsRepo->enable_ntp);
    ui->ntp_server->setEnabled(Configs::dataManager->settingsRepo->enable_ntp);
    ui->ntp_port->setEnabled(Configs::dataManager->settingsRepo->enable_ntp);
    ui->ntp_interval->setEnabled(Configs::dataManager->settingsRepo->enable_ntp);
    ui->ntp_outbound->setEnabled(Configs::dataManager->settingsRepo->enable_ntp);
    ui->ntp_server->setText(Configs::dataManager->settingsRepo->ntp_server_address);
    ui->ntp_port->setText(Int2String(Configs::dataManager->settingsRepo->ntp_server_port));
    ui->ntp_interval->setCurrentText(Configs::dataManager->settingsRepo->ntp_interval);
    ui->ntp_outbound->setCurrentText(Configs::dataManager->settingsRepo->ntp_outbound);
    connect(ui->ntp_enable, &QCheckBox::stateChanged, this, [=,this](const bool &state) {
        ui->ntp_server->setEnabled(state);
        ui->ntp_port->setEnabled(state);
        ui->ntp_interval->setEnabled(state);
        ui->ntp_outbound->setEnabled(state);
    });

    ui->disable_priv_req->setChecked(Configs::dataManager->settingsRepo->disable_privilege_req);
    ui->windows_no_admin->setChecked(Configs::dataManager->settingsRepo->disable_run_admin);
    ui->mozilla_cert->setChecked(Configs::dataManager->settingsRepo->use_mozilla_certs);
    D_LOAD_BOOL(kill_switch)

    D_LOAD_BOOL(skip_cert)

    ui->tabWidget->addTab(new DiagnosticsTab(ui->tabWidget), tr("Diagnostics"));
    bindValidation();

    // The .ui geometry underruns real font metrics/translations and Qt then overlaps the rows (#1671).
    QSize want = sizeHint();
    if (const QScreen *scr = parent ? parent->screen() : screen()) {
        const QRect avail = scr->availableGeometry();
        want = want.boundedTo(QSize(avail.width() - 24, avail.height() - 72));
    }
    resize(want);
}

DialogBasicSettings::~DialogBasicSettings() {
    delete ui;
}

void DialogBasicSettings::bindValidation() {
    using namespace Validation;
    const QStringList http = {QStringLiteral("http"), QStringLiteral("https")};
    const auto refreshOn = [this](QAbstractButton *toggle) { connect(toggle, &QAbstractButton::toggled, form, [this] { form->refresh(); }); };

    form->bind(ui->inbound_address, Rules::Ip());
    form->bind(ui->inbound_socks_port, Rules::Port());
    // Only the URL test treats 0 as its default; the download/upload test would time out at once.
    form->bind(ui->test_timeout, [](const QString &text) { return basicSettingsMillisError(text, 5000, false); }).label(tr("Speed test timeout"));
    form->bind(ui->simple_down_url, Rules::Url(http, Empty::Required)).label(ui->label_16->text())
        .when([this] { return ui->speedtest_mode->currentIndex() == 3; });
    connect(ui->speedtest_mode, &QComboBox::currentIndexChanged, form, [this] { form->refresh(); });
    form->bind(ui->test_latency_url, Rules::Url(http, Empty::Required)).label(ui->label_13->text());
    form->bind(ui->direct_test_url, Rules::Url(http)).label(ui->label_direct_test_url->text());
    form->bind(ui->test_concurrent, Rules::Int(0, 499, Empty::Required)).label(ui->label_14->text());
    form->bind(ui->url_timeout, [](const QString &text) { return basicSettingsMillisError(text, 3000, true); }).label(tr("URL test timeout"));

    struct PortUse {
        QLineEdit *edit;
        QString name;
        std::function<bool()> used;
    };
    const QList<PortUse> ports = {
        {ui->inbound_socks_port, tr("Mixed inbound"),
         [this] { return !ui->disable_mixed_inbound->isChecked() && !ui->random_listen_port->isChecked(); }},
        {ui->dns_in_port, tr("DNS server"), [] { return true; }},
        {ui->core_box_clash_api, tr("Clash API"), [this] { return !ui->core_box_clash_api->text().trimmed().isEmpty(); }},
        {ui->core_box_api_port, tr("sing-box API"), [this] { return !ui->core_box_api_port->text().trimmed().isEmpty(); }},
    };
    for (qsizetype i = 0; i < ports.size(); ++i) {
        for (qsizetype j = i + 1; j < ports.size(); ++j) {
            form->addRule({ports[i].edit, ports[j].edit}, [a = ports[i], b = ports[j]] {
                if (!a.used() || !b.used()) return QString();
                const auto port = ToInt(a.edit->text());
                if (!port || *port <= 0 || port != ToInt(b.edit->text())) return QString();
                return tr("Port %1 is already used by: %2. Choose a different port.").arg(QString::number(*port), b.name);
            }, nullptr, Severity::Error, ports[i].name);
        }
    }
    refreshOn(ui->disable_mixed_inbound);
    refreshOn(ui->random_listen_port);

    form->bind(ui->max_log_line, Rules::Int(1, std::numeric_limits<int>::max(), Empty::Required)).label(ui->label_17->text());
    for (QTextEdit *edit : {static_cast<QTextEdit *>(ui->log_include_regex), static_cast<QTextEdit *>(ui->log_exclude_regex)})
        form->addRule({edit}, [edit] {
            basicSettingsMarkRegexLines(edit);
            return basicSettingsRegexLinesError(edit->toPlainText());
        });

    form->bind(ui->user_agent, [](const QString &text) { return basicSettingsHeaderValueError(text); });
    form->bind(ui->sub_custom_hwid_params, [](const QString &text) { return basicSettingsHwidParamsError(text); });
    form->bind(ui->sub_auto_update, Rules::Int(kBasicSettingsMinAutoUpdate, kBasicSettingsMaxAutoUpdate, Empty::Required))
        .when([this] { return ui->sub_auto_update_enable->isChecked(); });
    form->bind(ui->route_auto_update, Rules::Int(kBasicSettingsMinAutoUpdate, kBasicSettingsMaxAutoUpdate, Empty::Required))
        .when([this] { return ui->route_auto_update_enable->isChecked(); });
    refreshOn(ui->sub_auto_update_enable);
    refreshOn(ui->route_auto_update_enable);

    form->bind(ui->dns_in_port, Rules::Port());
    const auto clashLabel = [this](const QLabel *label) { return QStringLiteral("%1 %2").arg(ui->clash_api_box->title(), label->text()); };
    form->bind(ui->core_box_clash_listen_addr, Rules::Host(Empty::Allowed)).label(clashLabel(ui->core_box_clash_listen_addr_l));
    form->bind(ui->core_box_clash_api, Rules::Port(Empty::Allowed)).label(clashLabel(ui->core_box_clash_api_l));
    form->bind(ui->core_box_api_port, Rules::Port(Empty::Allowed))
        .label(QStringLiteral("%1 %2").arg(ui->api_service_box->title(), ui->core_box_api_port_l->text()));

    form->bind(ui->ntp_server, Rules::Host(Empty::Allowed)).label(tr("NTP server"));
    // The core turns port 0 into 123.
    form->bind(ui->ntp_port, [](const QString &text) { return ToInt(text) == 0 ? QString() : Port(text, Empty::Allowed); }).label(tr("NTP port"));

    form->bind(ui->xray_geoip_url, Rules::Url(http)).label(ui->label_xray_geoip_url->text());
    form->bind(ui->xray_geosite_url, Rules::Url(http)).label(ui->label_xray_geosite_url->text());

    form->markAll();
}

void DialogBasicSettings::refreshUrlSchemeStatus() {
    const auto &tk = themeManager()->tokens;
    const auto show = [&](QLabel *status, Association a) {
        if (!UrlScheme_IsSupported(a)) {
            status->setText(tr("Not available for this installation"));
            status->setStyleSheet(QStringLiteral("color: %1;").arg(tk.muted.name()));
            return;
        }
        const bool installed = UrlScheme_IsCurrent(a);
        status->setText(installed ? tr("Installed") : tr("Not installed"));
        status->setStyleSheet(QStringLiteral("color: %1;").arg((installed ? tk.success : tk.muted).name()));
    };
    show(ui->url_scheme_status, Association::Links);
    show(ui->file_assoc_status, Association::ConfigFiles);
}

void DialogBasicSettings::applyRegexHighlighting() {
    basicSettingsMarkRegexLines(ui->log_include_regex);
    basicSettingsMarkRegexLines(ui->log_exclude_regex);
}

void DialogBasicSettings::accept() {
    if (!form->validateAll()) return;
    bool needChoosePort = false;

    Configs::dataManager->settingsRepo->inbound_address = ui->inbound_address->text().trimmed();
    Configs::dataManager->settingsRepo->custom_inbound = CACHE.custom_inbound;
    D_SAVE_INT(inbound_socks_port)
    if (!Configs::dataManager->settingsRepo->random_inbound_port && ui->random_listen_port->isChecked())
    {
        needChoosePort = true;
    }
    Configs::dataManager->settingsRepo->random_inbound_port = ui->random_listen_port->isChecked();
    D_SAVE_INT(test_concurrent)
    Configs::dataManager->settingsRepo->test_latency_url = ui->test_latency_url->text().trimmed();
    Configs::dataManager->settingsRepo->direct_test_url = ui->direct_test_url->text().trimmed();
    D_SAVE_BOOL(exit_country_lookup)
    Configs::dataManager->settingsRepo->exit_lookup_provider = ui->exit_lookup_provider->currentData().toString();
    D_SAVE_BOOL(disable_tray)
    Configs::dataManager->settingsRepo->proxy_scheme = ui->proxy_scheme->currentText().toLower();
    Configs::dataManager->settingsRepo->speed_test_mode = ui->speedtest_mode->currentIndex();
    Configs::dataManager->settingsRepo->simple_dl_url = ui->simple_down_url->text().trimmed();
    Configs::dataManager->settingsRepo->url_test_timeout_ms = ui->url_timeout->text().trimmed().toInt();
    Configs::dataManager->settingsRepo->speed_test_timeout_ms = ui->test_timeout->text().trimmed().toInt();
    Configs::dataManager->settingsRepo->allow_beta_update = ui->allow_beta->isChecked();
    Configs::dataManager->settingsRepo->disable_mixed_inbound = ui->disable_mixed_inbound->isChecked();
    Configs::dataManager->settingsRepo->reset_proxy_on_disable_sp = ui->reset_proxy_on_disable_sp->isChecked();
    D_SAVE_BOOL(inbound_auth)
    D_SAVE_STRING(inbound_user)
    D_SAVE_STRING(inbound_pass)

    const bool registrationTurnedOn =
        (!Configs::dataManager->settingsRepo->url_scheme_auto_register && ui->url_scheme_auto_register->isChecked()) ||
        (!Configs::dataManager->settingsRepo->file_assoc_auto_register && ui->file_assoc_auto_register->isChecked());
    Configs::dataManager->settingsRepo->url_scheme_auto_register = ui->url_scheme_auto_register->isChecked();
    Configs::dataManager->settingsRepo->file_assoc_auto_register = ui->file_assoc_auto_register->isChecked();
    if (registrationTurnedOn) UrlScheme_RegisterIfNeeded();

    auto oldMaxLogLines = Configs::dataManager->settingsRepo->max_log_line;
    Configs::dataManager->settingsRepo->max_log_line = ui->max_log_line->text().trimmed().toInt();
    if (oldMaxLogLines != Configs::dataManager->settingsRepo->max_log_line) CACHE.updateMaxLogLines = true;
    Configs::dataManager->settingsRepo->log_level = ui->log_level->currentText().trimmed();
    Configs::dataManager->settingsRepo->xray_log_level = ui->xray_loglevel->currentText().trimmed();
    Configs::dataManager->settingsRepo->log_enable_include = ui->enable_log_include->isChecked();
    Configs::dataManager->settingsRepo->log_enable_exclude = ui->enable_log_exclude->isChecked();
    Configs::dataManager->settingsRepo->log_include_keyword = SplitAndTrim(ui->log_include_keyword->toPlainText(), "\n", false);
    Configs::dataManager->settingsRepo->log_exclude_keyword = SplitAndTrim(ui->log_exclude_keyword->toPlainText(), "\n", false);

    Configs::dataManager->settingsRepo->log_include_regex = SplitAndTrim(ui->log_include_regex->toPlainText(), "\n", false);
    Configs::dataManager->settingsRepo->log_exclude_regex = SplitAndTrim(ui->log_exclude_regex->toPlainText(), "\n", false);

    Configs::dataManager->settingsRepo->enable_stats = ui->connection_statistics->isChecked();
    Configs::dataManager->settingsRepo->disable_traffic_aggregation = ui->disable_traffic_aggregation->isChecked();
    Configs::dataManager->settingsRepo->language = ui->language->currentIndex();
    auto oldUseCustomIcon = Configs::dataManager->settingsRepo->use_custom_icons;
    Configs::dataManager->settingsRepo->use_custom_icons = ui->enable_custom_icon->isChecked();
    if (oldUseCustomIcon != Configs::dataManager->settingsRepo->use_custom_icons) CACHE.updateTrayIcon = true;
    auto oldFollowStatusInTaskbar = Configs::dataManager->settingsRepo->follow_status_in_taskbar;
    Configs::dataManager->settingsRepo->follow_status_in_taskbar = ui->follow_status_in_taskbar->isChecked();
    if (oldFollowStatusInTaskbar != Configs::dataManager->settingsRepo->follow_status_in_taskbar) CACHE.updateTrayIcon = true;
    D_SAVE_BOOL(start_minimal)
    Configs::dataManager->settingsRepo->skip_delete_confirmation = ui->skip_delete_confirm->isChecked();
    bool profileListDisplayChanged =
        Configs::dataManager->settingsRepo->show_config_security != ui->show_config_security->isChecked();
    D_SAVE_BOOL(show_config_security)

    QString logFontFamily = ui->log_font_family->currentFont().family();
    if (Configs::dataManager->settingsRepo->log_font_family.isEmpty() && logFontFamily == CACHE.shownLogFontFamily) logFontFamily.clear();
    int logFontSize = ui->log_font_size->currentText().toInt();
    if (Configs::dataManager->settingsRepo->log_font_size <= 0 && logFontSize == CACHE.shownLogFontSize) logFontSize = 0;
    const bool logFontChanged = logFontFamily != Configs::dataManager->settingsRepo->log_font_family ||
                                logFontSize != Configs::dataManager->settingsRepo->log_font_size;
    Configs::dataManager->settingsRepo->log_font_family = logFontFamily;
    Configs::dataManager->settingsRepo->log_font_size = logFontSize;

    // The PeriodicRunner reads these intervals live; no timer needs restarting.

    Configs::dataManager->settingsRepo->user_agent = ui->user_agent->text().trimmed();
    Configs::dataManager->settingsRepo->sub_tls_version = ui->sub_tls_version->currentIndex();
    Configs::dataManager->settingsRepo->sub_http_version = ui->sub_http_version->currentIndex();
    D_SAVE_BOOL(net_use_proxy)
    D_SAVE_BOOL(allow_stopping_active_profile)
    D_SAVE_BOOL(sub_clear)
    D_SAVE_BOOL(sub_show_change_popup)
    D_SAVE_BOOL(net_insecure)
    D_SAVE_BOOL(sub_send_hwid)
    Configs::dataManager->settingsRepo->sub_custom_hwid_params = ui->sub_custom_hwid_params->text().trimmed();
    D_SAVE_INT_ENABLE(sub_auto_update, sub_auto_update_enable)
    D_SAVE_BOOL(sub_respect_server_interval)
    D_SAVE_INT_ENABLE(route_auto_update, route_auto_update_enable)

    Configs::dataManager->settingsRepo->disable_traffic_stats = ui->disable_stats->isChecked();
    Configs::dataManager->settingsRepo->core_dns_in_port = ui->dns_in_port->text().trimmed().toInt();
    Configs::dataManager->settingsRepo->core_box_clash_listen_addr = ui->core_box_clash_listen_addr->text().trimmed();
    Configs::dataManager->settingsRepo->core_box_clash_api = ui->core_box_clash_api->text().trimmed().toInt();
    Configs::dataManager->settingsRepo->core_box_clash_api_secret = ui->core_box_clash_api_secret->text().trimmed();
    Configs::dataManager->settingsRepo->core_box_api_port = ui->core_box_api_port->text().trimmed().toInt();
    // Blank means "no authentication" to sing-box, so never let the field clear it.
    if (const auto secret = ui->core_box_api_secret->text().trimmed(); !secret.isEmpty())
        Configs::dataManager->settingsRepo->core_box_api_secret = secret;

    Configs::dataManager->settingsRepo->xray_vless_preference = static_cast<Configs::Xray::XrayVlessPreference>(ui->vless_xray_pref->currentIndex());
    // An empty field means the provider shown as its placeholder; the download at profile start has no fallback of its own.
    const auto geoAssetUrl = [](const QComboBox *combo) {
        const auto url = combo->currentText().trimmed();
        return url.isEmpty() ? combo->lineEdit()->placeholderText() : url;
    };
    Configs::dataManager->settingsRepo->xray_geoip_url = geoAssetUrl(ui->xray_geoip_url);
    Configs::dataManager->settingsRepo->xray_geosite_url = geoAssetUrl(ui->xray_geosite_url);

    Configs::dataManager->settingsRepo->enable_ntp = ui->ntp_enable->isChecked();
    Configs::dataManager->settingsRepo->ntp_server_address = ui->ntp_server->text().trimmed();
    Configs::dataManager->settingsRepo->ntp_server_port = ui->ntp_port->text().trimmed().toInt();
    Configs::dataManager->settingsRepo->ntp_interval = ui->ntp_interval->currentText().trimmed();
    Configs::dataManager->settingsRepo->ntp_outbound = ui->ntp_outbound->currentText().trimmed();

    // The Type column marks TLS profiles compromised while this is on.
    profileListDisplayChanged |= Configs::dataManager->settingsRepo->skip_cert != ui->skip_cert->isChecked();
    D_SAVE_BOOL(skip_cert)
    Configs::dataManager->settingsRepo->disable_privilege_req = ui->disable_priv_req->isChecked();
    if (Configs::dataManager->settingsRepo->disable_run_admin != ui->windows_no_admin->isChecked()) CACHE.updateDisableAdmin = true;
    Configs::dataManager->settingsRepo->disable_run_admin = ui->windows_no_admin->isChecked();
    Configs::dataManager->settingsRepo->use_mozilla_certs = ui->mozilla_cert->isChecked();
    const bool killSwitchChanged = Configs::dataManager->settingsRepo->kill_switch != ui->kill_switch->isChecked();
    D_SAVE_BOOL(kill_switch)

    QStringList changes;
    if (CACHE.needRestart) changes << MwArg::NeedRestart;
    if (CACHE.updateDisableTray) changes << MwArg::DisableTray;
    if (CACHE.updateTrayIcon) changes << MwArg::TrayIcon;
    if (CACHE.updateMaxLogLines) changes << MwArg::MaxLogLines;
    if (CACHE.updateDisableAdmin) changes << MwArg::DisableAdmin;
    if (needChoosePort) changes << MwArg::ChoosePort;
    if (profileListDisplayChanged) changes << MwArg::ProfileListDisplay;
    if (killSwitchChanged) changes << MwArg::KillSwitch;
    if (logFontChanged) changes << MwArg::LogFont;
    MW_dialog_message(MwMessage::UpdateSettings, changes);
    QDialog::accept();
}

void DialogBasicSettings::downloadXrayGeoAsset(const QString &url, const QString &fileName) {
    const QString effectiveUrl = url.trimmed();
    if (effectiveUrl.isEmpty()) {
        QMessageBox::warning(this, tr("Download geo asset"),
            tr("Please enter a URL for %1 first.").arg(fileName));
        return;
    }

    Engine::AssetUpdates::GeoRequest request;
    if (fileName == QLatin1String("geoip.dat")) {
        request.files = {Engine::AssetUpdates::GeoFile::GeoIp};
        request.geoipUrl = effectiveUrl;
    } else {
        request.files = {Engine::AssetUpdates::GeoFile::GeoSite};
        request.geositeUrl = effectiveUrl;
    }
    // Don't capture the dialog, it may close first.
    const bool started = Engine::AssetUpdates::UpdateGeoAssets(request, [fileName](const QList<Engine::AssetUpdates::GeoFileResult> &results) {
        const QString err = results.isEmpty() ? QString() : results.first().error;
        if (!results.isEmpty() && err.isEmpty()) {
            QMessageBox::information(GetMainWindow(), QObject::tr("Download geo asset"),
                QObject::tr("%1 was downloaded successfully.").arg(fileName));
        } else {
            MessageBoxWarning(QObject::tr("Download geo asset"),
                QObject::tr("Failed to download %1:\n%2").arg(fileName, err));
        }
    });
    // Refused only while this same file is downloading; the other file may download alongside.
    if (!started) {
        QMessageBox::warning(this, tr("Download geo asset"), tr("A geo asset download is already running."));
        return;
    }
    MW_show_log(tr("Downloading Xray geo asset: %1").arg(fileName));
}

void DialogBasicSettings::rememberGeoAssetUrl(QComboBox *combo, const QString &url, QStringList &history) {
    if (url.isEmpty()) return;
    for (const auto &p: Configs::Xray::XrayGeoAssetProviders) {
        if (url == p.geoip || url == p.geosite) return;
    }

    history.removeAll(url);
    history.prepend(url);
    while (history.size() > 5) history.removeLast();

    // Providers occupy the head of the combo, so everything after them is history that may have aged out.
    for (int i = combo->count() - 1; i >= static_cast<int>(Configs::Xray::XrayGeoAssetProviders.size()); --i) {
        if (!history.contains(combo->itemText(i))) combo->removeItem(i);
    }
    if (combo->findText(url) < 0) combo->addItem(url);
    combo->setCurrentText(url);

    Configs::dataManager->settingsRepo->Save();
}

void DialogBasicSettings::on_xray_geoip_download_clicked() {
    QString url = ui->xray_geoip_url->currentText().trimmed();
    if (url.isEmpty()) url = ui->xray_geoip_url->lineEdit()->placeholderText();
    rememberGeoAssetUrl(ui->xray_geoip_url, url, Configs::dataManager->settingsRepo->xray_geoip_url_history);
    downloadXrayGeoAsset(url, "geoip.dat");
}

void DialogBasicSettings::on_xray_geosite_download_clicked() {
    QString url = ui->xray_geosite_url->currentText().trimmed();
    if (url.isEmpty()) url = ui->xray_geosite_url->lineEdit()->placeholderText();
    rememberGeoAssetUrl(ui->xray_geosite_url, url, Configs::dataManager->settingsRepo->xray_geosite_url_history);
    downloadXrayGeoAsset(url, "geosite.dat");
}

void DialogBasicSettings::on_backup_create_clicked() {
    Configs::BackupParts parts;
    parts.profiles = ui->backup_inc_profiles->isChecked();
    parts.routes = ui->backup_inc_routes->isChecked();
    parts.settings = ui->backup_inc_settings->isChecked();
    parts.otp = ui->backup_inc_otp->isChecked();
    parts.ipLists = ui->backup_inc_ip_lists->isChecked();
    parts.icons = ui->backup_inc_icons->isChecked();

    if (!parts.any()) {
        QMessageBox::warning(this, tr("Create Backup"),
            tr("Select at least one part to include in the backup."));
        return;
    }

    Engine::BackupArchive::Prepare(parts);

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Create Backup"),
        QDir::homePath() + "/Throne-backup.thrbackup",
        tr("Throne Backup (*.thrbackup)")
    );
    if (filePath.isEmpty()) return;

    if (const auto error = Engine::BackupArchive::Build(parts, filePath); !error.isEmpty()) {
        QMessageBox::critical(this, tr("Backup Failed"), basicSettingsBackupError(error));
        return;
    }

    QStringList included;
    if (parts.profiles) included << tr("Profiles");
    if (parts.routes) included << tr("Routing profiles");
    if (parts.settings) included << tr("Settings");
    if (parts.otp) included << tr("OTP profiles");
    if (parts.ipLists) included << tr("IP lists and scans");
    if (parts.icons) included << tr("Custom icons");

    QMessageBox::information(this, tr("Backup Created"),
        tr("Backup created successfully:\n%1\n\nIncluded: %2")
            .arg(filePath, included.join(", ")));
}

void DialogBasicSettings::on_backup_restore_clicked() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Restore Backup"),
        QDir::homePath(),
        tr("Throne Backup (*.thrbackup)")
    );
    if (filePath.isEmpty()) return;

    Engine::BackupArchive::Info info;
    if (const auto error = Engine::BackupArchive::Inspect(filePath, &info); !error.isEmpty()) {
        QMessageBox::critical(this, tr("Restore Failed"), basicSettingsBackupError(error));
        return;
    }
    const Configs::BackupParts avail = info.available;
    const QString createdAt = info.createdAt;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Restore Backup"));
    auto* layout = new QVBoxLayout(&dlg);
    auto* header = new QLabel(
        tr("Backup created on %1.\nSelect which parts to restore:")
            .arg(createdAt.isEmpty() ? tr("unknown date") : createdAt), &dlg);
    header->setWordWrap(true);
    layout->addWidget(header);

    auto* cbProfiles = new QCheckBox(tr("Profiles (groups and proxies)"), &dlg);
    auto* cbRoutes = new QCheckBox(tr("Routing profiles"), &dlg);
    auto* cbSettings = new QCheckBox(tr("Settings"), &dlg);
    auto* cbOtp = new QCheckBox(tr("OTP profiles"), &dlg);
    auto* cbIpLists = new QCheckBox(tr("IP lists and scans"), &dlg);
    auto* cbIcons = new QCheckBox(tr("Custom icons"), &dlg);
    for (auto* cb : {cbProfiles, cbRoutes, cbSettings, cbOtp, cbIpLists, cbIcons}) cb->setChecked(true);
    cbProfiles->setEnabled(avail.profiles);
    cbProfiles->setChecked(avail.profiles);
    cbRoutes->setEnabled(avail.routes);
    cbRoutes->setChecked(avail.routes);
    cbSettings->setEnabled(avail.settings);
    cbSettings->setChecked(avail.settings);
    cbOtp->setEnabled(avail.otp);
    cbOtp->setChecked(avail.otp);
    cbIpLists->setEnabled(avail.ipLists);
    cbIpLists->setChecked(avail.ipLists);
    cbIcons->setEnabled(avail.icons);
    cbIcons->setChecked(avail.icons);
    layout->addWidget(cbProfiles);
    layout->addWidget(cbRoutes);
    layout->addWidget(cbSettings);
    layout->addWidget(cbOtp);
    layout->addWidget(cbIpLists);
    layout->addWidget(cbIcons);

    auto* warn = new QLabel(
        tr("Each selected part replaces the current data. This cannot be undone.\n"
           "Throne will restart to complete the restore."), &dlg);
    warn->setWordWrap(true);
    layout->addWidget(warn);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Restore"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    Configs::BackupParts chosen;
    chosen.profiles = avail.profiles && cbProfiles->isChecked();
    chosen.routes = avail.routes && cbRoutes->isChecked();
    chosen.settings = avail.settings && cbSettings->isChecked();
    chosen.otp = avail.otp && cbOtp->isChecked();
    chosen.ipLists = avail.ipLists && cbIpLists->isChecked();
    chosen.icons = avail.icons && cbIcons->isChecked();

    if (!chosen.any()) {
        QMessageBox::warning(this, tr("Restore Backup"),
            tr("Select at least one part to restore."));
        return;
    }

    // The window keeps today's full settings restore; only the API keeps this host's keys.
    Engine::BackupArchive::ApplyResult result;
    if (const auto error = Engine::BackupArchive::Apply(filePath, chosen, {}, &result); !error.isEmpty()) {
        QMessageBox::critical(this, tr("Restore Failed"), basicSettingsBackupError(error));
        return;
    }
    const int skippedRules = result.skippedRules;

    QString done = tr("Backup restored successfully. Throne will now restart for the changes to take effect.");
    if (skippedRules > 0)
        done += "\n\n" + tr("Skipped %n routing rule(s) that use conditions this version of Throne does not support.", nullptr, skippedRules);
    QMessageBox::information(this, tr("Restore Complete"), done);
    MW_dialog_message(MwMessage::RestartProgram, {});
    QDialog::reject();
}

