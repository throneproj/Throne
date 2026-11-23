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
#include "include/database/SettingsSchema.h"
#include "include/database/entities/Group.h"
#include "include/engine/services/AssetUpdates.h"
#include "include/engine/services/BackupArchive.h"
#include "include/engine/services/SettingsService.h"
#include "include/ui/setting/ServiceCheckSelector.h"
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
#include <QCheckBox>
#include <QScreen>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSslSocket>
#include <QStandardItemModel>
#include <QFile>
#include <QFormLayout>
#include <QLineEdit>
#include <QLocale>
#include <QPointer>
#include <QProgressDialog>
#include <QScrollBar>
#include <QSysInfo>
#include <QTreeWidget>
#include <QUrl>

#include <algorithm>
#include <limits>

#include "include/sys/UrlScheme.hpp"
#include "include/ui/mainwindow.h"
#include "include/ui/setting/DiagnosticsTab.h"
#include "include/ui/utils/NativeFileDialog.h"

namespace {
    constexpr int kBasicSettingsMinAutoUpdate = 30;
    constexpr int kBasicSettingsMaxAutoUpdate = 525600;

    Configs::SettingsSchema::IntRange basicSettingsRange(const QString &key, Configs::SettingsSchema::IntRange fallback) {
        const auto *found = Configs::SettingsSchema::Find(key);
        return found != nullptr && found->range ? *found->range : fallback;
    }

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

    Configs::BackupParts basicSettingsCheckedParts(const Ui::DialogBasicSettings *ui) {
        Configs::BackupParts parts;
        parts.profiles = ui->backup_inc_profiles->isChecked();
        parts.routes = ui->backup_inc_routes->isChecked();
        parts.settings = ui->backup_inc_settings->isChecked();
        parts.otp = ui->backup_inc_otp->isChecked();
        parts.ipLists = ui->backup_inc_ip_lists->isChecked();
        parts.icons = ui->backup_inc_icons->isChecked();
        return parts;
    }

    void basicSettingsRestoreCheckedParts(Ui::DialogBasicSettings *ui) {
        Configs::BackupParts parts;
        const QStringList &saved = Configs::dataManager->settingsRepo->backup_parts;
        if (saved.isEmpty() || !Engine::BackupArchive::PartsFromNames(saved, &parts, nullptr) || !parts.any()) return;
        ui->backup_inc_profiles->setChecked(parts.profiles);
        ui->backup_inc_routes->setChecked(parts.routes);
        ui->backup_inc_settings->setChecked(parts.settings);
        ui->backup_inc_otp->setChecked(parts.otp);
        ui->backup_inc_ip_lists->setChecked(parts.ipLists);
        ui->backup_inc_icons->setChecked(parts.icons);
    }

    void basicSettingsRememberCheckedParts(const Configs::BackupParts &parts) {
        auto &settings = *Configs::dataManager->settingsRepo;
        const QStringList names = Engine::BackupArchive::PartNames(parts);
        if (settings.backup_parts == names) return;
        settings.backup_parts = names;
        settings.Save();
    }

    // Text a WebDAV server or another device wrote, for plain-text display only.
    QString basicSettingsRemoteText(const QString &text) {
        return text.simplified().left(200);
    }

    QString basicSettingsPlatformName(const QString &platform) {
        if (platform == QLatin1String("winnt")) return QStringLiteral("Windows");
        if (platform == QLatin1String("linux")) return QStringLiteral("Linux");
        if (platform == QLatin1String("darwin")) return QStringLiteral("macOS");
        if (platform == QLatin1String("android")) return QStringLiteral("Android");
        return basicSettingsRemoteText(platform);
    }

    QString basicSettingsPartsText(const QStringList &parts) {
        QStringList shown;
        for (const auto &part : parts) {
            QString label;
            if (part == QLatin1String("profiles")) label = DialogBasicSettings::tr("Profiles");
            else if (part == QLatin1String("routes")) label = DialogBasicSettings::tr("Routing profiles");
            else if (part == QLatin1String("settings")) label = DialogBasicSettings::tr("Settings");
            else if (part == QLatin1String("otp")) label = DialogBasicSettings::tr("OTP entries");
            else if (part == QLatin1String("ip_lists")) label = DialogBasicSettings::tr("IP lists and scans");
            else if (part == QLatin1String("icons")) label = DialogBasicSettings::tr("Custom icons");
            if (!label.isEmpty() && !shown.contains(label)) shown << label;
        }
        return shown.join(QStringLiteral(", "));
    }

    // Window-modal busy indicator without a cancel button; the caller closes it when the worker reports back.
    QPointer<QProgressDialog> basicSettingsWebDavProgress(QWidget *parent, const QString &text) {
        auto *progress = new QProgressDialog(text, QString(), 0, 0, parent);
        progress->setWindowTitle(QStringLiteral("WebDAV"));
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->setAutoClose(false);
        progress->setAutoReset(false);
        progress->setAttribute(Qt::WA_DeleteOnClose);
        progress->show();
        return progress;
    }

    // Non-blocking, and plain text because it may carry what a server sent.
    void basicSettingsWebDavMessage(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text) {
        auto *box = new QMessageBox(icon, title, text, QMessageBox::Ok, parent);
        box->setTextFormat(Qt::PlainText);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    }
}

struct DialogBasicSettings::WebDavChooser {
    QPointer<QDialog> dialog;
    QTreeWidget *tree = nullptr;
    QLabel *note = nullptr;
    QPushButton *restore = nullptr;
    QPushButton *remove = nullptr;
    QList<Engine::WebDavBackup::Entry> entries;
};

DialogBasicSettings::DialogBasicSettings(QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogBasicSettings) {
    snapshot_ = *Configs::dataManager->settingsRepo;
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    ADD_ASTERISK(this);

    ui->inbound_socks_port_l->setText(ui->inbound_socks_port_l->text().replace("Socks", "Mixed (SOCKS+HTTP)"));
    ui->log_level->addItems(QString("trace debug info warn error fatal panic").split(" "));
    ui->xray_loglevel->addItems(Configs::Xray::XrayLogLevels);
    ui->disable_stats->setChecked(snapshot_.disable_traffic_stats);
    ui->proxy_scheme->setCurrentText(snapshot_.proxy_scheme);
    basicSettingsRestoreCheckedParts(ui);

    S_LOAD_STRING(inbound_address)
    CACHE.custom_inbound = snapshot_.custom_inbound;
    S_LOAD_INT(inbound_socks_port)
    ui->random_listen_port->setChecked(snapshot_.random_inbound_port);
    S_LOAD_INT(test_concurrent)
    S_LOAD_STRING(test_latency_url)
    S_LOAD_STRING(direct_test_url)
    S_LOAD_BOOL(exit_country_lookup)
    ui->exit_lookup_provider->addItem(tr("Automatic"), Configs_network::ExitLookupAutoId);
    for (const auto &provider : Configs_network::ExitLookupProviders()) ui->exit_lookup_provider->addItem(provider.label, provider.id);
    ui->exit_lookup_provider->setCurrentIndex(
        std::max(0, ui->exit_lookup_provider->findData(snapshot_.exit_lookup_provider)));
    ui->label_exit_lookup_provider->setEnabled(ui->exit_country_lookup->isChecked());
    ui->exit_lookup_provider->setEnabled(ui->exit_country_lookup->isChecked());
    connect(ui->exit_country_lookup, &QCheckBox::toggled, this, [this](bool enabled) {
        ui->label_exit_lookup_provider->setEnabled(enabled);
        ui->exit_lookup_provider->setEnabled(enabled);
    });
    serviceChecks_ = new ServiceCheckSelector(snapshot_.service_check_disabled, ui->service_check_box);
    ui->serviceCheckLayout->addWidget(serviceChecks_);
    S_LOAD_BOOL(disable_tray)
    ui->reset_proxy_on_disable_sp->setChecked(snapshot_.reset_proxy_on_disable_sp);
    ui->url_timeout->setText(Int2String(snapshot_.url_test_timeout_ms));
    const int speedTestMode = snapshot_.speed_test_mode;
    ui->speedtest_mode->setCurrentIndex(speedTestMode >= 0 && speedTestMode < ui->speedtest_mode->count() ? speedTestMode : 0);
    ui->test_timeout->setText(Int2String(snapshot_.speed_test_timeout_ms));
    ui->simple_down_url->setText(snapshot_.simple_dl_url);
    ui->allow_beta->setChecked(snapshot_.allow_beta_update);
    ui->disable_mixed_inbound->setChecked(snapshot_.disable_mixed_inbound);
    S_LOAD_BOOL(inbound_auth)
    S_LOAD_STRING(inbound_user)
    S_LOAD_STRING(inbound_pass)

    ui->url_scheme_auto_register->setChecked(snapshot_.url_scheme_auto_register);
    ui->file_assoc_auto_register->setChecked(snapshot_.file_assoc_auto_register);
    const auto connectAssociation = [this](Association a, bool Configs::SettingsValues::*autoRegisterKey, QCheckBox *autoRegister,
                                           QPushButton *install, QPushButton *uninstall, const QString &failure) {
        connect(install, &QPushButton::clicked, this, [=,this] {
            const bool ok = UrlScheme_Install(a);
            refreshUrlSchemeStatus();
            if (!ok) QMessageBox::warning(this, ui->url_scheme_box->title(), failure);
        });
        connect(uninstall, &QPushButton::clicked, this, [=,this] {
            UrlScheme_Uninstall(a);
            // Uninstall saved auto registration off, so ticking the box again before OK must count as turning it on.
            snapshot_.*autoRegisterKey = false;
            autoRegister->setChecked(false);
            refreshUrlSchemeStatus();
        });
    };
    connectAssociation(Association::Links, &Configs::SettingsValues::url_scheme_auto_register, ui->url_scheme_auto_register,
                       ui->url_scheme_install, ui->url_scheme_uninstall, tr("Could not register the handler for throne:// links."));
    connectAssociation(Association::ConfigFiles, &Configs::SettingsValues::file_assoc_auto_register, ui->file_assoc_auto_register,
                       ui->file_assoc_install, ui->file_assoc_uninstall, tr("Could not register the config file associations."));
    for (auto *widget : UnavailableWidgets(ui)) widget->hide();
    ui->url_scheme_box->setEnabled(UrlScheme_IsSupported(Association::Links));
    refreshUrlSchemeStatus();

    connect(ui->custom_inbound_edit, &QPushButton::clicked, this, [=,this] {
        C_EDIT_JSON_ALLOW_EMPTY(custom_inbound, JsonEdit::SingBox::Config)
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

    ui->max_log_line->setText(QString::number(snapshot_.max_log_line));
    ui->log_level->setCurrentText(snapshot_.log_level);
    ui->xray_loglevel->setCurrentText(snapshot_.xray_log_level);
    ui->enable_log_include->setChecked(snapshot_.log_enable_include);
    ui->enable_log_exclude->setChecked(snapshot_.log_enable_exclude);
    ui->log_include_keyword->setText(snapshot_.log_include_keyword.join("\n"));
    ui->log_exclude_keyword->setText(snapshot_.log_exclude_keyword.join("\n"));
    ui->log_include_regex->setText(snapshot_.log_include_regex.join("\n"));
    ui->log_exclude_regex->setText(snapshot_.log_exclude_regex.join("\n"));
    applyRegexHighlighting();

    ui->connection_statistics->setChecked(snapshot_.enable_stats);
    ui->disable_traffic_aggregation->setChecked(snapshot_.disable_traffic_aggregation);
    S_LOAD_BOOL(start_minimal)
    ui->skip_delete_confirm->setChecked(snapshot_.skip_delete_confirmation);
    S_LOAD_BOOL(show_config_security)
    ui->language->setCurrentIndex(snapshot_.language);
    CACHE.theme = snapshot_.theme;
    CACHE.font = snapshot_.font;
    CACHE.font_size = snapshot_.font_size;
    CACHE.openTheme = themeManager()->current_theme;
    CACHE.openFont = qApp->font();
    connect(ui->font, &QComboBox::currentTextChanged, this, [=,this](const QString &fontName) {
        auto font = qApp->font();
        font.setFamily(fontName);
        qApp->setFont(font);
        CACHE.font = fontName;
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
        CACHE.font_size = sizeStr.toInt();
        adjustSize();
    });
    ui->log_font_family->setCurrentFont(QFont(ResolveLogFontFamily(snapshot_.log_font_family)));
    CACHE.shownLogFontFamily = ui->log_font_family->currentFont().family();
    for (int i=7;i<=26;i++) {
        ui->log_font_size->addItem(Int2String(i));
    }
    const int logFontSize = snapshot_.log_font_size;
    ui->log_font_size->setCurrentText(Int2String(logFontSize > 0 ? logFontSize : qApp->font().pointSize()));
    CACHE.shownLogFontSize = ui->log_font_size->currentText().toInt();
    ui->theme->addItems(QStyleFactory::keys());
    ui->theme->addItem("QDarkStyle");
    // Custom stylesheet themes, not QStyleFactory keys.
    ui->theme->addItems({"FlatGray", "LightBlue", "SoftPink", "BlackSoft"});
    ui->enable_custom_icon->setChecked(snapshot_.use_custom_icons);
    ui->follow_status_in_taskbar->setChecked(snapshot_.follow_status_in_taskbar);
    ui->follow_status_in_taskbar->setEnabled(ui->enable_custom_icon->isChecked());
    connect(ui->enable_custom_icon, &QCheckBox::toggled, this, [this](bool enabled) {
        ui->follow_status_in_taskbar->setEnabled(enabled);
    });
    connect(ui->select_custom_icon, &QPushButton::clicked, this, [=, this] {
        auto n = QMessageBox::information(this, "Custom Icon Manual", tr(Configs::Information::CustomIconManual.toStdString().c_str()), QMessageBox::Open | QMessageBox::Cancel);
        if (n == QMessageBox::Open) {
            auto fileNames = NativeFileDialog::getOpenFileNames(this,
                tr("Select png icons"), QDir::homePath(), tr("Image Files (*.png)"));
            QString errors;
            for (const auto& fileName : fileNames) {
                QFileInfo fileInfo(fileName);
                if (auto pixMap = QPixmap(fileName); pixMap.isNull()) errors += "Failed to load " + fileName + "\n";
                else if (pixMap.width() != pixMap.height()) errors += "Image does not have equal width and height: " + fileName + "\n";
                else if (!Configs::Information::iconNames.contains(fileInfo.fileName())) errors += "Icon name is not valid: " + fileInfo.fileName() + "\n";
                else {
                    QFile::remove(QDir("icons").filePath(fileInfo.fileName()));
                    if (!QFile::copy(fileName, QDir("icons").filePath(fileInfo.fileName()))) errors += "Failed to copy " + fileName + "\n";
                }
            }
            if (!fileNames.isEmpty()) GetMainWindow()->refreshTrayIcon();
            if (!errors.isEmpty()) {
                QMessageBox::warning(this, "Select custom image error", errors);
            }
        }
    });
    bool ok;
    auto themeId = snapshot_.theme.toInt(&ok);
    if (ok) {
        ui->theme->setCurrentIndex(themeId);
    } else {
        ui->theme->setCurrentText(snapshot_.theme);
    }
    connect(ui->theme, &QComboBox::currentIndexChanged, this, [=,this](int index) {
        themeManager()->ApplyTheme(ui->theme->currentText());
        CACHE.theme = ui->theme->currentText().trimmed();
        refreshUrlSchemeStatus();
    });

    ui->user_agent->setText(snapshot_.user_agent);
    ui->user_agent->setPlaceholderText(Configs::dataManager->settingsRepo->GetUserAgent(true));
    ui->sub_tls_version->setCurrentIndex(std::clamp(snapshot_.sub_tls_version, 0, ui->sub_tls_version->count() - 1));
    ui->sub_http_version->setCurrentIndex(std::clamp(snapshot_.sub_http_version, 0, ui->sub_http_version->count() - 1));
    if (auto *model = qobject_cast<QStandardItemModel *>(ui->sub_tls_version->model()); model && !QSslSocket::isProtocolSupported(QSsl::TlsV1_3)) {
        model->item(static_cast<int>(Configs::subTlsVersion::tls13))->setEnabled(false);
    }
    S_LOAD_BOOL(net_use_proxy)
    S_LOAD_BOOL(allow_stopping_active_profile)
    S_LOAD_BOOL(sub_clear)
    S_LOAD_BOOL(sub_show_change_popup)
    S_LOAD_BOOL(net_insecure)
    S_LOAD_BOOL(sub_send_hwid)
    S_LOAD_STRING(sub_custom_hwid_params)
    S_LOAD_INT_ENABLE(sub_auto_update, sub_auto_update_enable)
    S_LOAD_BOOL(sub_respect_server_interval)
    auto details = GetDeviceDetails();
	ui->sub_send_hwid->setToolTip(
        ui->sub_send_hwid->toolTip()
            .arg(details.hwid.isEmpty() ? "N/A" : details.hwid,
                details.os.isEmpty() ? "N/A" : details.os,
                details.osVersion.isEmpty() ? "N/A" : details.osVersion,
                details.model.isEmpty() ? "N/A" : details.model));

    const auto dnsInPortRange = basicSettingsRange(QStringLiteral("core_dns_in_port"), {1, 65535});
    ui->dns_in_port->setValidator(new QIntValidator(dnsInPortRange.min, dnsInPortRange.max, ui->dns_in_port));
    ui->dns_in_port->setText(Int2String(snapshot_.core_dns_in_port));
    ui->dns_in_lan->setChecked(snapshot_.core_dns_in_lan);

    ui->vless_xray_pref->addItems(Configs::Xray::XrayVlessPreferenceString);
    ui->vless_xray_pref->setCurrentIndex(snapshot_.xray_vless_preference);
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
    populateGeoAssetCombo(ui->xray_geoip_url, true, snapshot_.xray_geoip_url, snapshot_.xray_geoip_url_history);
    populateGeoAssetCombo(ui->xray_geosite_url, false, snapshot_.xray_geosite_url, snapshot_.xray_geosite_url_history);

    ui->ntp_enable->setChecked(snapshot_.enable_ntp);
    ui->ntp_server->setEnabled(snapshot_.enable_ntp);
    ui->ntp_port->setEnabled(snapshot_.enable_ntp);
    ui->ntp_interval->setEnabled(snapshot_.enable_ntp);
    ui->ntp_outbound->setEnabled(snapshot_.enable_ntp);
    ui->ntp_server->setText(snapshot_.ntp_server_address);
    ui->ntp_port->setText(Int2String(snapshot_.ntp_server_port));
    ui->ntp_interval->setCurrentText(snapshot_.ntp_interval);
    ui->ntp_outbound->setCurrentText(snapshot_.ntp_outbound);
    connect(ui->ntp_enable, &QCheckBox::stateChanged, this, [=,this](const bool &state) {
        ui->ntp_server->setEnabled(state);
        ui->ntp_port->setEnabled(state);
        ui->ntp_interval->setEnabled(state);
        ui->ntp_outbound->setEnabled(state);
    });

    ui->disable_priv_req->setChecked(snapshot_.disable_privilege_req);
    ui->windows_no_admin->setChecked(snapshot_.disable_run_admin);
    ui->mozilla_cert->setChecked(snapshot_.use_mozilla_certs);
    S_LOAD_BOOL(xray_use_system_ca)
    S_LOAD_BOOL(kill_switch)

    S_LOAD_BOOL(skip_cert)

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
    const auto socksPort = basicSettingsRange(QStringLiteral("inbound_socks_port"), {1, 65535});
    form->bind(ui->inbound_socks_port, Rules::AllOf({Rules::Port(), Rules::Int(socksPort.min, socksPort.max, Empty::Required)}));
    // Only the URL test treats 0 as its default; the download/upload test would time out at once.
    form->bind(ui->test_timeout, [](const QString &text) { return basicSettingsMillisError(text, 5000, false); }).label(tr("Speed test timeout"));
    form->bind(ui->simple_down_url, Rules::Url(http, Empty::Required)).label(ui->label_16->text())
        .when([this] { return ui->speedtest_mode->currentIndex() == 3; });
    connect(ui->speedtest_mode, &QComboBox::currentIndexChanged, form, [this] { form->refresh(); });
    form->bind(ui->test_latency_url, Rules::Url(http, Empty::Required)).label(ui->label_13->text());
    form->bind(ui->direct_test_url, Rules::Url(http)).label(ui->label_direct_test_url->text());
    const auto concurrency = basicSettingsRange(QStringLiteral("test_concurrent"), {0, 499});
    form->bind(ui->test_concurrent, Rules::Int(concurrency.min, concurrency.max, Empty::Required)).label(ui->label_14->text());
    // 0 means the default whatever the schema's minimum, so only its maximum applies.
    const int urlTimeoutMax = basicSettingsRange(QStringLiteral("url_test_timeout_ms"), {0, std::numeric_limits<int>::max()}).max;
    form->bind(ui->url_timeout, [urlTimeoutMax](const QString &text) {
        if (auto problem = basicSettingsMillisError(text, 3000, true); !problem.isEmpty()) return problem;
        return Int(text, 0, urlTimeoutMax);
    }).label(tr("URL test timeout"));

    struct PortUse {
        QLineEdit *edit;
        QString name;
        std::function<bool()> used;
    };
    const QList<PortUse> ports = {
        {ui->inbound_socks_port, tr("Mixed inbound"),
         [this] { return !ui->disable_mixed_inbound->isChecked() && !ui->random_listen_port->isChecked(); }},
        {ui->dns_in_port, tr("DNS server"), [] { return true; }},
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
    // These are edited in Integration Settings, so only their saved ports can clash.
    const auto savedPorts = [] {
        const auto &settings = Configs::dataManager->settingsRepo;
        QList<QPair<int, QString>> taken;
        if (settings->core_box_clash_api > 0) taken.append({settings->core_box_clash_api, tr("Clash API")});
        if (settings->core_box_api_port > 0) taken.append({settings->core_box_api_port, tr("sing-box API")});
        if (settings->remote_api_enable) taken.append({settings->remote_api_port, tr("Throne API")});
        return taken;
    };
    for (const auto &use : ports) {
        form->addRule({use.edit}, [use, savedPorts] {
            if (!use.used()) return QString();
            const auto port = ToInt(use.edit->text());
            if (!port || *port <= 0) return QString();
            for (const auto &[taken, name] : savedPorts()) {
                if (*port == taken) return tr("Port %1 is already used by: %2. Choose a different port.").arg(QString::number(*port), name);
            }
            return QString();
        }, nullptr, Severity::Error, use.name);
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
    refreshOn(ui->sub_auto_update_enable);

    const auto dnsInPort = basicSettingsRange(QStringLiteral("core_dns_in_port"), {1, 65535});
    form->bind(ui->dns_in_port, Rules::AllOf({Rules::Port(), Rules::Int(dnsInPort.min, dnsInPort.max, Empty::Required)}));

    form->bind(ui->ntp_server, Rules::Host(Empty::Allowed)).label(tr("NTP server"));
    // The core turns port 0 into 123.
    form->bind(ui->ntp_port, [](const QString &text) { return ToInt(text) == 0 ? QString() : Port(text, Empty::Allowed); }).label(tr("NTP port"));

    form->bind(ui->xray_geoip_url, Rules::Url(http)).label(ui->label_xray_geoip_url->text());
    form->bind(ui->xray_geosite_url, Rules::Url(http)).label(ui->label_xray_geosite_url->text());

    form->markAll();
}

QList<QWidget *> DialogBasicSettings::UnavailableWidgets(const Ui::DialogBasicSettings *ui) {
    QList<QWidget *> widgets;
#ifdef Q_OS_MACOS
    // LaunchServices registers the scheme from the bundle's Info.plist, so there is nothing of ours to add or take back.
    widgets << ui->url_scheme_install << ui->url_scheme_uninstall;
#endif
    if (!UrlScheme_IsSupported(Association::ConfigFiles)) {
        widgets << ui->file_assoc_auto_register << ui->file_assoc_status << ui->file_assoc_install << ui->file_assoc_uninstall;
    }
#ifndef Q_OS_WIN
    widgets << ui->proxy_scheme_l << ui->proxy_scheme << ui->windows_no_admin;
#endif
    return widgets;
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
    Configs::SettingsValues proposed = snapshot_;

    proposed.inbound_address = ui->inbound_address->text().trimmed();
    proposed.custom_inbound = CACHE.custom_inbound;
    S_SAVE_INT(inbound_socks_port)
    proposed.random_inbound_port = ui->random_listen_port->isChecked();
    S_SAVE_INT(test_concurrent)
    proposed.test_latency_url = ui->test_latency_url->text().trimmed();
    proposed.direct_test_url = ui->direct_test_url->text().trimmed();
    S_SAVE_BOOL(exit_country_lookup)
    proposed.exit_lookup_provider = ui->exit_lookup_provider->currentData().toString();
    proposed.service_check_disabled = serviceChecks_->disabledIds();
    S_SAVE_BOOL(disable_tray)
    proposed.proxy_scheme = ui->proxy_scheme->currentText().toLower();
    proposed.speed_test_mode = ui->speedtest_mode->currentIndex();
    proposed.simple_dl_url = ui->simple_down_url->text().trimmed();
    proposed.url_test_timeout_ms = ui->url_timeout->text().trimmed().toInt();
    proposed.speed_test_timeout_ms = ui->test_timeout->text().trimmed().toInt();
    proposed.allow_beta_update = ui->allow_beta->isChecked();
    proposed.disable_mixed_inbound = ui->disable_mixed_inbound->isChecked();
    proposed.reset_proxy_on_disable_sp = ui->reset_proxy_on_disable_sp->isChecked();
    S_SAVE_BOOL(inbound_auth)
    S_SAVE_STRING(inbound_user)
    S_SAVE_STRING(inbound_pass)

    proposed.url_scheme_auto_register = ui->url_scheme_auto_register->isChecked();
    proposed.file_assoc_auto_register = ui->file_assoc_auto_register->isChecked();
    const bool registrationTurnedOn = (!snapshot_.url_scheme_auto_register && proposed.url_scheme_auto_register) ||
                                      (!snapshot_.file_assoc_auto_register && proposed.file_assoc_auto_register);

    proposed.max_log_line = ui->max_log_line->text().trimmed().toInt();
    proposed.log_level = ui->log_level->currentText().trimmed();
    proposed.xray_log_level = ui->xray_loglevel->currentText().trimmed();
    proposed.log_enable_include = ui->enable_log_include->isChecked();
    proposed.log_enable_exclude = ui->enable_log_exclude->isChecked();
    proposed.log_include_keyword = SplitAndTrim(ui->log_include_keyword->toPlainText(), "\n", false);
    proposed.log_exclude_keyword = SplitAndTrim(ui->log_exclude_keyword->toPlainText(), "\n", false);

    proposed.log_include_regex = SplitAndTrim(ui->log_include_regex->toPlainText(), "\n", false);
    proposed.log_exclude_regex = SplitAndTrim(ui->log_exclude_regex->toPlainText(), "\n", false);

    proposed.enable_stats = ui->connection_statistics->isChecked();
    proposed.disable_traffic_aggregation = ui->disable_traffic_aggregation->isChecked();
    proposed.language = ui->language->currentIndex();
    proposed.use_custom_icons = ui->enable_custom_icon->isChecked();
    proposed.follow_status_in_taskbar = ui->follow_status_in_taskbar->isChecked();
    S_SAVE_BOOL(start_minimal)
    proposed.skip_delete_confirmation = ui->skip_delete_confirm->isChecked();
    S_SAVE_BOOL(show_config_security)
    proposed.theme = CACHE.theme;
    proposed.font = CACHE.font;
    proposed.font_size = CACHE.font_size;

    QString logFontFamily = ui->log_font_family->currentFont().family();
    if (snapshot_.log_font_family.isEmpty() && logFontFamily == CACHE.shownLogFontFamily) logFontFamily.clear();
    int logFontSize = ui->log_font_size->currentText().toInt();
    if (snapshot_.log_font_size <= 0 && logFontSize == CACHE.shownLogFontSize) logFontSize = 0;
    proposed.log_font_family = logFontFamily;
    proposed.log_font_size = logFontSize;

    // The PeriodicRunner reads these intervals live; no timer needs restarting.

    proposed.user_agent = ui->user_agent->text().trimmed();
    proposed.sub_tls_version = ui->sub_tls_version->currentIndex();
    proposed.sub_http_version = ui->sub_http_version->currentIndex();
    S_SAVE_BOOL(net_use_proxy)
    S_SAVE_BOOL(allow_stopping_active_profile)
    S_SAVE_BOOL(sub_clear)
    S_SAVE_BOOL(sub_show_change_popup)
    S_SAVE_BOOL(net_insecure)
    S_SAVE_BOOL(sub_send_hwid)
    proposed.sub_custom_hwid_params = ui->sub_custom_hwid_params->text().trimmed();
    S_SAVE_INT_ENABLE(sub_auto_update, sub_auto_update_enable)
    S_SAVE_BOOL(sub_respect_server_interval)

    proposed.disable_traffic_stats = ui->disable_stats->isChecked();
    proposed.core_dns_in_port = ui->dns_in_port->text().trimmed().toInt();
    proposed.core_dns_in_lan = ui->dns_in_lan->isChecked();

    proposed.xray_vless_preference = static_cast<Configs::Xray::XrayVlessPreference>(ui->vless_xray_pref->currentIndex());
    // An empty field means the provider shown as its placeholder; the download at profile start has no fallback of its own.
    const auto geoAssetUrl = [](const QComboBox *combo) {
        const auto url = combo->currentText().trimmed();
        return url.isEmpty() ? combo->lineEdit()->placeholderText() : url;
    };
    proposed.xray_geoip_url = geoAssetUrl(ui->xray_geoip_url);
    proposed.xray_geosite_url = geoAssetUrl(ui->xray_geosite_url);

    proposed.enable_ntp = ui->ntp_enable->isChecked();
    proposed.ntp_server_address = ui->ntp_server->text().trimmed();
    proposed.ntp_server_port = ui->ntp_port->text().trimmed().toInt();
    proposed.ntp_interval = ui->ntp_interval->currentText().trimmed();
    proposed.ntp_outbound = ui->ntp_outbound->currentText().trimmed();

    S_SAVE_BOOL(skip_cert)
    proposed.disable_privilege_req = ui->disable_priv_req->isChecked();
    proposed.disable_run_admin = ui->windows_no_admin->isChecked();
    proposed.use_mozilla_certs = ui->mozilla_cert->isChecked();
    S_SAVE_BOOL(xray_use_system_ca)
    S_SAVE_BOOL(kill_switch)

    if (const auto error = Engine::SettingsService::ApplyEdit(snapshot_, proposed, {.interactive = true})) {
        MessageBoxWarning(windowTitle(), SettingsApplyErrorText(*error));
        return;
    }
    if (registrationTurnedOn) UrlScheme_RegisterIfNeeded();
    QDialog::accept();
}

void DialogBasicSettings::done(int r) {
    // Before QDialog::done: a slot on finished may delete the window.
    if (r != QDialog::Accepted) {
        if (themeManager()->current_theme != CACHE.openTheme) themeManager()->ApplyTheme(CACHE.openTheme);
        if (qApp->font() != CACHE.openFont) qApp->setFont(CACHE.openFont);
    }
    QDialog::done(r);
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
    if (webDavBusy) return;
    const Configs::BackupParts parts = basicSettingsCheckedParts(ui);

    if (!parts.any()) {
        QMessageBox::warning(this, tr("Create backup"),
            tr("Select at least one part to include in the backup."));
        return;
    }

    Engine::BackupArchive::Prepare(parts);

    QString filePath = NativeFileDialog::getSaveFileName(
        this,
        tr("Create backup"),
        QDir::homePath() + "/Throne-backup.thrbackup",
        tr("Throne Backup (*.thrbackup)")
    );
    if (filePath.isEmpty()) return;
    basicSettingsRememberCheckedParts(parts);

    if (const auto error = Engine::BackupArchive::Build(parts, filePath); !error.isEmpty()) {
        QMessageBox::critical(this, tr("Backup failed"), basicSettingsBackupError(error));
        return;
    }

    QStringList included;
    if (parts.profiles) included << tr("Profiles");
    if (parts.routes) included << tr("Routing profiles");
    if (parts.settings) included << tr("Settings");
    if (parts.otp) included << tr("OTP entries");
    if (parts.ipLists) included << tr("IP lists and scans");
    if (parts.icons) included << tr("Custom icons");

    QMessageBox::information(this, tr("Backup created"),
        tr("Backup created successfully:\n%1\n\nIncluded: %2")
            .arg(filePath, included.join(", ")));
}

void DialogBasicSettings::on_backup_restore_clicked() {
    if (webDavBusy) return;
    QString filePath = NativeFileDialog::getOpenFileName(
        this,
        tr("Restore backup"),
        QDir::homePath(),
        tr("Throne Backup (*.thrbackup)")
    );
    if (filePath.isEmpty()) return;
    restoreBackupFile(filePath, {});
}

void DialogBasicSettings::restoreBackupFile(const QString &filePath, const QString &fallbackDeviceName) {
    Engine::BackupArchive::Info info;
    if (const auto error = Engine::BackupArchive::Inspect(filePath, &info); !error.isEmpty()) {
        QMessageBox::critical(this, tr("Restore failed"), basicSettingsBackupError(error));
        return;
    }
    const Configs::BackupParts avail = info.available;
    const QString createdAt = info.createdAt;
    const QString createdBy = basicSettingsRemoteText(info.deviceName.isEmpty() ? fallbackDeviceName : info.deviceName);

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Restore Backup"));
    auto* layout = new QVBoxLayout(&dlg);
    if (!createdBy.isEmpty()) {
        auto *creator = new QLabel(tr("Created by: %1").arg(createdBy), &dlg);
        creator->setTextFormat(Qt::PlainText);
        creator->setWordWrap(true);
        layout->addWidget(creator);
    }
    auto* header = new QLabel(
        tr("Backup created on %1.\nSelect which parts to restore:")
            .arg(createdAt.isEmpty() ? tr("unknown date") : createdAt), &dlg);
    header->setTextFormat(Qt::PlainText);
    header->setWordWrap(true);
    layout->addWidget(header);

    auto* cbProfiles = new QCheckBox(tr("Profiles (groups and proxies)"), &dlg);
    auto* cbRoutes = new QCheckBox(tr("Routing profiles"), &dlg);
    auto* cbSettings = new QCheckBox(tr("Settings"), &dlg);
    auto* cbOtp = new QCheckBox(tr("OTP entries"), &dlg);
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
        QMessageBox::warning(this, tr("Restore backup"),
            tr("Select at least one part to restore."));
        return;
    }

    // Apply keeps the device-local settings; the window restores every other one, only the API also keeps this host's keys.
    Engine::BackupArchive::ApplyResult result;
    if (const auto error = Engine::BackupArchive::Apply(filePath, chosen, {}, &result); !error.isEmpty()) {
        QMessageBox::critical(this, tr("Restore failed"), basicSettingsBackupError(error));
        return;
    }
    const int skippedRules = result.skippedRules;

    QString done = tr("Backup restored successfully. Throne will now restart for the changes to take effect.");
    if (skippedRules > 0)
        done += "\n\n" + tr("Skipped %n routing rule(s) that use conditions this version of Throne does not support.", nullptr, skippedRules);
    QMessageBox::information(this, tr("Restore complete"), done);
    MW_dialog_message(MwMessage::RestartProgram, {});
    QDialog::reject();
}

bool DialogBasicSettings::webDavReady(const QString &title) {
    if (webDavBusy) return false;
    if (!Configs::dataManager->settingsRepo->webdav_server.trimmed().isEmpty()) return true;
    QMessageBox::information(this, title, tr("Enter your WebDAV server in WebDAV Settings first."));
    showWebDavSettings();
    return false;
}

void DialogBasicSettings::on_webdav_settings_clicked() {
    if (webDavBusy) return;
    showWebDavSettings();
}

void DialogBasicSettings::showWebDavSettings() {
    const auto &settings = *Configs::dataManager->settingsRepo;
    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("WebDAV Settings"));
    auto *layout = new QVBoxLayout(dlg);
    auto *form = new QFormLayout();
    auto *server = new QLineEdit(settings.webdav_server, dlg);
    server->setPlaceholderText(QStringLiteral("https://cloud.example.com/remote.php/dav/files/me/"));
    server->setMinimumWidth(server->fontMetrics().horizontalAdvance(server->placeholderText()) + 24);
    auto *user = new QLineEdit(settings.webdav_user, dlg);
    auto *pass = new QLineEdit(settings.webdav_pass, dlg);
    pass->setEchoMode(QLineEdit::Password);
    auto *folder = new QLineEdit(settings.webdav_path, dlg);
    folder->setPlaceholderText(QStringLiteral("Throne"));
    auto *device = new QLineEdit(settings.device_name, dlg);
    device->setPlaceholderText(QSysInfo::machineHostName());
    form->addRow(tr("Server URL"), server);
    form->addRow(tr("Username"), user);
    form->addRow(tr("Password"), pass);
    form->addRow(tr("Folder"), folder);
    form->addRow(tr("Device name"), device);
    layout->addLayout(form);
    auto *useProxy = new QCheckBox(tr("Connect through the proxy"), dlg);
    useProxy->setChecked(settings.webdav_use_proxy);
    layout->addWidget(useProxy);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, dlg);
    auto *test = buttons->addButton(tr("Test"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    const auto fields = [server, user, pass, folder, useProxy] {
        Engine::WebDavBackup::Config config;
        config.server = server->text().trimmed();
        config.user = user->text().trimmed();
        config.pass = pass->text();
        config.path = folder->text().trimmed();
        config.useProxy = useProxy->isChecked();
        return config;
    };
    connect(test, &QPushButton::clicked, dlg, [this, dlg, test, fields] {
        if (webDavBusy) return;
        webDavBusy = true;
        test->setEnabled(false);
        const QPointer<DialogBasicSettings> self(this);
        const QPointer<QDialog> target(dlg);
        const QPointer<QPushButton> button(test);
        const auto progress = basicSettingsWebDavProgress(dlg, tr("Testing the connection..."));
        runOnNewThread([self, target, button, progress, config = fields()] {
            const QString error = Engine::WebDavBackup::Test(config);
            runOnUiThread([self, target, button, progress, error] {
                if (progress) progress->close();
                if (self) self->webDavBusy = false;
                if (button) button->setEnabled(true);
                if (!target) return;
                if (error.isEmpty()) {
                    basicSettingsWebDavMessage(target, QMessageBox::Information, tr("WebDAV Settings"), tr("Connected. The backup folder is ready."));
                } else {
                    basicSettingsWebDavMessage(target, QMessageBox::Warning, tr("WebDAV Settings"), tr("Connection failed:\n%1").arg(error));
                }
            });
        });
    });
    connect(buttons, &QDialogButtonBox::accepted, dlg, [dlg, fields, device] {
        const auto config = fields();
        const QString scheme = QUrl(config.server).scheme().toLower();
        if (!config.server.isEmpty() && scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
            basicSettingsWebDavMessage(dlg, QMessageBox::Warning, tr("WebDAV Settings"), tr("The server URL must start with http:// or https://."));
            return;
        }
        auto &saved = *Configs::dataManager->settingsRepo;
        saved.webdav_server = config.server;
        saved.webdav_user = config.user;
        saved.webdav_pass = config.pass;
        saved.webdav_path = config.path;
        saved.webdav_use_proxy = config.useProxy;
        saved.device_name = device->text().trimmed();
        saved.Save();
        dlg->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    dlg->open();
}

void DialogBasicSettings::on_webdav_upload_clicked() {
    const QString title = tr("Upload to WebDAV");
    if (!webDavReady(title)) return;
    const Configs::BackupParts parts = basicSettingsCheckedParts(ui);
    if (!parts.any()) {
        QMessageBox::warning(this, title, tr("Select at least one part to include in the backup."));
        return;
    }
    basicSettingsRememberCheckedParts(parts);

    Engine::BackupArchive::Prepare(parts);
    const auto config = Engine::WebDavBackup::FromSettings();
    const QString tempPath = Engine::BackupArchive::TempPath(QStringLiteral("webdav-"));
    webDavBusy = true;
    const QPointer<DialogBasicSettings> self(this);
    const auto progress = basicSettingsWebDavProgress(this, tr("Uploading the backup..."));
    runOnNewThread([self, progress, parts, config, tempPath, title] {
        Configs::BackupParts written;
        QString fileName;
        QString indexWarning;
        QString error = Engine::BackupArchive::Build(parts, tempPath, &written);
        const bool built = error.isEmpty();
        if (built) {
            error = Engine::WebDavBackup::Upload(config, tempPath, Engine::BackupArchive::PartNames(written), &fileName, &indexWarning);
        }
        QFile::remove(tempPath);
        runOnUiThread([self, progress, title, built, error, fileName, indexWarning] {
            if (progress) progress->close();
            if (!self) return;
            self->webDavBusy = false;
            if (!error.isEmpty()) {
                basicSettingsWebDavMessage(self, QMessageBox::Critical, title,
                                           built ? tr("Upload failed:\n%1").arg(error) : basicSettingsBackupError(error));
                return;
            }
            if (indexWarning.isEmpty()) {
                basicSettingsWebDavMessage(self, QMessageBox::Information, title, tr("Backup uploaded:\n%1").arg(fileName));
            } else {
                basicSettingsWebDavMessage(self, QMessageBox::Warning, title,
                                           tr("Backup uploaded, but the backup list could not be updated: %1").arg(indexWarning));
            }
        });
    });
}

void DialogBasicSettings::on_webdav_restore_clicked() {
    if (!webDavReady(tr("Restore from WebDAV"))) return;
    loadWebDavBackups(nullptr);
}

void DialogBasicSettings::loadWebDavBackups(const std::shared_ptr<WebDavChooser> &chooser, const QString &notice) {
    const auto config = Engine::WebDavBackup::FromSettings();
    webDavBusy = true;
    const QPointer<DialogBasicSettings> self(this);
    QWidget *parent = this;
    if (chooser && chooser->dialog) parent = chooser->dialog;
    const auto progress = basicSettingsWebDavProgress(parent, tr("Loading the backup list..."));
    runOnNewThread([self, chooser, progress, config, notice] {
        QList<Engine::WebDavBackup::Entry> entries;
        QString indexWarning;
        const QString error = Engine::WebDavBackup::List(config, &entries, &indexWarning);
        runOnUiThread([self, chooser, progress, notice, entries, indexWarning, error] {
            if (progress) progress->close();
            if (!self) return;
            self->webDavBusy = false;
            if (chooser && !chooser->dialog) return;
            if (!error.isEmpty()) {
                QString text = tr("Could not list the backups:\n%1").arg(error);
                if (!notice.isEmpty()) text += QStringLiteral("\n\n") + notice;
                basicSettingsWebDavMessage(chooser ? static_cast<QWidget *>(chooser->dialog) : self.data(), QMessageBox::Critical,
                                           tr("Restore from WebDAV"), text);
                return;
            }
            if (!chooser) {
                self->showWebDavBackups(entries, indexWarning);
                return;
            }
            QStringList note;
            if (!notice.isEmpty()) note << notice;
            if (!indexWarning.isEmpty()) note << tr("The backup list could not be read: %1").arg(indexWarning);
            self->fillWebDavChooser(*chooser, entries, note.join(QLatin1Char('\n')));
        });
    });
}

void DialogBasicSettings::showWebDavBackups(const QList<Engine::WebDavBackup::Entry> &entries, const QString &indexWarning) {
    const QString title = tr("Restore from WebDAV");
    if (entries.isEmpty()) {
        basicSettingsWebDavMessage(this, QMessageBox::Information, title, tr("There are no backups in the WebDAV folder yet."));
        return;
    }

    auto chooser = std::make_shared<WebDavChooser>();
    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(title);
    chooser->dialog = dlg;
    auto *layout = new QVBoxLayout(dlg);
    chooser->tree = new QTreeWidget(dlg);
    chooser->tree->setRootIsDecorated(false);
    chooser->tree->setUniformRowHeights(true);
    chooser->tree->setSelectionMode(QAbstractItemView::SingleSelection);
    chooser->tree->setHeaderLabels({tr("Created"), tr("Device"), tr("Platform"), tr("Version"), tr("Parts"), tr("Size")});
    layout->addWidget(chooser->tree);
    chooser->note = new QLabel(dlg);
    chooser->note->setTextFormat(Qt::PlainText);
    chooser->note->setWordWrap(true);
    layout->addWidget(chooser->note);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    chooser->restore = buttons->addButton(tr("Restore"), QDialogButtonBox::ActionRole);
    chooser->remove = buttons->addButton(tr("Delete"), QDialogButtonBox::ActionRole);
    chooser->restore->setDefault(true);
    layout->addWidget(buttons);

    connect(chooser->tree, &QTreeWidget::currentItemChanged, dlg, [chooser](QTreeWidgetItem *current) {
        chooser->restore->setEnabled(current != nullptr);
        chooser->remove->setEnabled(current != nullptr);
    });
    connect(chooser->tree, &QTreeWidget::itemDoubleClicked, dlg, [this, chooser] { restoreWebDavBackup(chooser); });
    connect(chooser->restore, &QPushButton::clicked, dlg, [this, chooser] { restoreWebDavBackup(chooser); });
    connect(chooser->remove, &QPushButton::clicked, dlg, [this, chooser] { deleteWebDavBackup(chooser); });
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    fillWebDavChooser(*chooser, entries, indexWarning.isEmpty() ? QString() : tr("The backup list could not be read: %1").arg(indexWarning));
    int width = 2 * chooser->tree->frameWidth() + chooser->tree->verticalScrollBar()->sizeHint().width() +
                layout->contentsMargins().left() + layout->contentsMargins().right();
    for (int column = 0; column < chooser->tree->columnCount(); ++column) {
        chooser->tree->resizeColumnToContents(column);
        width += chooser->tree->columnWidth(column);
    }
    QSize size = dlg->sizeHint().expandedTo(QSize(width, 0));
    if (const QScreen *scr = screen()) size = size.boundedTo(scr->availableGeometry().size() - QSize(48, 72));
    dlg->resize(size);
    dlg->open();
}

void DialogBasicSettings::fillWebDavChooser(WebDavChooser &chooser, const QList<Engine::WebDavBackup::Entry> &entries, const QString &note) {
    const QString thisDevice = Engine::BackupArchive::ThisDevice().id;
    const QLocale locale;
    chooser.entries = entries;
    chooser.tree->clear();
    for (qsizetype i = 0; i < entries.size(); ++i) {
        const auto &entry = entries.at(i);
        QString device = basicSettingsRemoteText(entry.deviceName);
        if (device.isEmpty()) device = tr("Unknown device");
        if (!entry.deviceId.isEmpty() && entry.deviceId == thisDevice) device = tr("%1 (this device)").arg(device);
        auto *item = new QTreeWidgetItem(chooser.tree);
        item->setText(0, entry.createdAt.isValid() ? locale.toString(entry.createdAt.toLocalTime(), QLocale::ShortFormat) : tr("Unknown"));
        item->setText(1, device);
        item->setText(2, basicSettingsPlatformName(entry.platform));
        item->setText(3, basicSettingsRemoteText(entry.appVersion));
        item->setText(4, basicSettingsPartsText(entry.parts));
        item->setText(5, entry.size >= 0 ? locale.formattedDataSize(entry.size) : QString());
        item->setTextAlignment(5, Qt::AlignRight | Qt::AlignVCenter);
        item->setData(0, Qt::UserRole, static_cast<int>(i));
    }
    chooser.note->setText(note);
    chooser.note->setVisible(!note.isEmpty());
    const bool any = chooser.tree->topLevelItemCount() > 0;
    if (any) chooser.tree->setCurrentItem(chooser.tree->topLevelItem(0));
    chooser.restore->setEnabled(any);
    chooser.remove->setEnabled(any);
}

void DialogBasicSettings::restoreWebDavBackup(const std::shared_ptr<WebDavChooser> &chooser) {
    if (webDavBusy || !chooser->dialog) return;
    const auto *item = chooser->tree->currentItem();
    const int row = item != nullptr ? item->data(0, Qt::UserRole).toInt() : -1;
    if (row < 0 || row >= chooser->entries.size()) return;
    const auto entry = chooser->entries.at(row);

    const auto config = Engine::WebDavBackup::FromSettings();
    const QString tempPath = Engine::BackupArchive::TempPath(QStringLiteral("webdav-"));
    webDavBusy = true;
    const QPointer<DialogBasicSettings> self(this);
    const QPointer<QDialog> dialog(chooser->dialog);
    const auto progress = basicSettingsWebDavProgress(chooser->dialog, tr("Downloading the backup..."));
    runOnNewThread([self, dialog, progress, config, entry, tempPath] {
        const QString error = Engine::WebDavBackup::Download(config, entry, tempPath);
        runOnUiThread([self, dialog, progress, entry, tempPath, error] {
            if (progress) progress->close();
            if (!self) {
                QFile::remove(tempPath);
                return;
            }
            self->webDavBusy = false;
            if (!error.isEmpty()) {
                basicSettingsWebDavMessage(dialog ? static_cast<QWidget *>(dialog) : self.data(), QMessageBox::Critical,
                                           tr("Restore from WebDAV"), tr("Could not download the backup:\n%1").arg(error));
                return;
            }
            if (dialog) dialog->close();
            self->restoreBackupFile(tempPath, entry.indexed ? entry.deviceName : QString());
            QFile::remove(tempPath);
        });
    });
}

void DialogBasicSettings::deleteWebDavBackup(const std::shared_ptr<WebDavChooser> &chooser) {
    if (webDavBusy || !chooser->dialog) return;
    const auto *item = chooser->tree->currentItem();
    const int row = item != nullptr ? item->data(0, Qt::UserRole).toInt() : -1;
    if (row < 0 || row >= chooser->entries.size()) return;
    const auto entry = chooser->entries.at(row);

    auto *confirm = new QMessageBox(QMessageBox::Question, tr("Delete backup"),
                                    tr("Delete this backup from the WebDAV server? This cannot be undone.\n\n%1").arg(basicSettingsRemoteText(entry.file)),
                                    QMessageBox::Yes | QMessageBox::No, chooser->dialog.data());
    confirm->setTextFormat(Qt::PlainText);
    confirm->setDefaultButton(QMessageBox::No);
    confirm->setAttribute(Qt::WA_DeleteOnClose);
    connect(confirm->button(QMessageBox::Yes), &QAbstractButton::clicked, chooser->dialog.data(), [this, chooser, entry] {
        if (webDavBusy || !chooser->dialog) return;
        const auto config = Engine::WebDavBackup::FromSettings();
        webDavBusy = true;
        const QPointer<DialogBasicSettings> self(this);
        const auto progress = basicSettingsWebDavProgress(chooser->dialog, tr("Deleting the backup..."));
        runOnNewThread([self, chooser, progress, config, entry] {
            QString indexWarning;
            const QString error = Engine::WebDavBackup::Delete(config, entry, &indexWarning);
            runOnUiThread([self, chooser, progress, error, indexWarning] {
                if (progress) progress->close();
                if (!self) return;
                self->webDavBusy = false;
                if (!chooser->dialog) return;
                if (!error.isEmpty()) {
                    basicSettingsWebDavMessage(chooser->dialog, QMessageBox::Critical, tr("Delete backup"),
                                               tr("Could not delete the backup:\n%1").arg(error));
                    return;
                }
                self->loadWebDavBackups(chooser, indexWarning.isEmpty() ? QString()
                                                                        : tr("Backup deleted, but the backup list could not be updated: %1").arg(indexWarning));
            });
        });
    });
    confirm->open();
}

