#include "include/ui/setting/dialog_integration.h"

#include <include/ui/utils/GuiUtils.hpp>

#include "include/api/remote/Server.hpp"
#include "include/global/LocalNetwork.hpp"
#include "include/global/Validation.hpp"
#include "include/ui/mainwindow.h"
#include "include/ui/utils/ValidationForm.h"
#include <QAction>
#include <QClipboard>
#include <QHostAddress>
#include <QLabel>
#include <QMessageBox>
#include <QScreen>

namespace {
    // Mirrors GlobalHotkeys::setActions, which registers only the first chord and refuses keys that would swallow typing.
    QString integrationGlobalHotkeyError(const QKeySequence &key) {
        if (key.isEmpty()) return {};
        if (key.count() > 1) return DialogIntegration::tr("Global hotkeys can only be a single key combination.");
        const QKeyCombination chord = key[0];
        if (!(chord.keyboardModifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) && chord.key() < Qt::Key_F1)
            return DialogIntegration::tr("\"%1\" needs Ctrl, Alt or Meta, otherwise it would block typing in other applications.")
                .arg(key.toString(QKeySequence::NativeText));
        return {};
    }

    // Accepts exactly what RemoteApi::ConfigFromSettings keeps, netmask and leading-zero forms included.
    QString integrationAllowEntryError(const QString &entry) {
        const auto host = entry.section(QLatin1Char('/'), 0, 0);
        if (host.contains(QLatin1Char(':')) || host.count(QLatin1Char('.')) == 3) {
            const auto subnet = entry.contains(QLatin1Char('/')) ? entry : entry + (host.contains(QLatin1Char(':')) ? QStringLiteral("/128") : QStringLiteral("/32"));
            if (QHostAddress::parseSubnet(subnet).second >= 0) return {};
        }
        if (auto message = Validation::Cidr(entry, Validation::Family::Any, true); !message.isEmpty()) return message;
        return DialogIntegration::tr("Enter an IP address or a range such as 192.168.1.0/24 or fd00::/8.");
    }

    QString integrationFieldLabel(QFormLayout *layout, QWidget *field) {
        const auto *label = qobject_cast<QLabel *>(layout->labelForField(field));
        return label != nullptr ? label->text() : QString();
    }
}

DialogIntegration::DialogIntegration(QWidget *parent, const QList<QAction*>& actions) : QDialog(parent), ui(new Ui::DialogIntegration) {
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    ui->show_mainwindow->setKeySequence(Configs::dataManager->settingsRepo->hotkey_mainwindow);
    ui->show_groups->setKeySequence(Configs::dataManager->settingsRepo->hotkey_group);
    ui->show_routes->setKeySequence(Configs::dataManager->settingsRepo->hotkey_route);
    ui->system_proxy->setKeySequence(Configs::dataManager->settingsRepo->hotkey_system_proxy_menu);
    ui->toggle_proxy->setKeySequence(Configs::dataManager->settingsRepo->hotkey_toggle_system_proxy);
    ui->toggle_connection->setKeySequence(Configs::dataManager->settingsRepo->hotkey_toggle_connection);
    ui->toggle_tun->setKeySequence(Configs::dataManager->settingsRepo->hotkey_toggle_tun);

    generateShortcutItems(actions);
    bindHotkeyValidation();

    GetMainWindow()->RegisterHotkey(true);
    if (!GetMainWindow()->IsGlobalHotkeySupported()) {
        auto note = new QLabel(QCoreApplication::translate("GlobalHotkeys", "Global hotkeys are not supported in this desktop session."), ui->tab);
        note->setWordWrap(true);
        ui->formLayout->insertRow(0, note);
    }

    setupApiTab();
    // The Remote API reads the list split at commas and whitespace (RemoteApi::ConfigFromSettings).
    form->bind(ui->api_allow, [](const QString &text) {
        return Validation::EachItem(QString(text).replace(QLatin1Char(','), QLatin1Char(' ')), Validation::Split::Whitespace,
                                    integrationAllowEntryError);
    });

    // Sized with the LAN warning shown, so selecting "Local network" later never needs a taller window.
    ui->api_lan_warning->setVisible(true);
    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    if (scr != nullptr) resize(sizeHint().boundedTo(scr->availableGeometry().size()));
    updateApiControls();
    form->markAll();
}

void DialogIntegration::generateShortcutItems(const QList<QAction*>& actions)
{
    auto widget = new QWidget(ui->shortcut_area);
    auto layout = new QFormLayout(widget);
    widget->setLayout(layout);
    ui->shortcut_area->setWidget(widget);
    for (auto action : actions)
    {
        auto kseq = new QtExtKeySequenceEdit(this);
        if (!action->shortcut().isEmpty()) kseq->setKeySequence(action->shortcut());
        seqEdit2ID[kseq] = action->data().toString();
        layout->addRow(action->text(), kseq);
        hotkeyEdits.append({kseq, action->text(), false});
    }
}

void DialogIntegration::bindHotkeyValidation()
{
    QList<HotkeyEdit> globals;
    for (auto *edit : {ui->show_mainwindow, ui->show_groups, ui->show_routes, ui->system_proxy, ui->toggle_proxy, ui->toggle_connection, ui->toggle_tun})
        globals.append({edit, integrationFieldLabel(ui->formLayout, edit), true});
    hotkeyEdits = globals + hotkeyEdits;

    for (const auto &field : std::as_const(hotkeyEdits)) {
        // QKeySequenceEdit is its line edit's focus proxy: the line edit carries the mark, the edit reports when recording ends.
        auto *display = field.edit->findChild<QLineEdit *>();
        QWidget *target = display != nullptr ? static_cast<QWidget *>(display) : field.edit;
        form->addRule({target}, [this, field] {
            const auto key = field.edit->keySequence();
            if (key.isEmpty()) return QString();
            if (field.global) {
                if (auto problem = integrationGlobalHotkeyError(key); !problem.isEmpty()) return problem;
            }
            for (const auto &other : std::as_const(hotkeyEdits)) {
                if (other.edit != field.edit && other.edit->keySequence() == key)
                    return tr("\"%1\" is also assigned to \"%2\".").arg(key.toString(QKeySequence::NativeText), other.label);
            }
            return QString();
        }, this, Validation::Severity::Error, field.label);
        const auto recheck = [this, target] {
            form->revalidate(target);
            form->refresh();
        };
        connect(field.edit, &QKeySequenceEdit::editingFinished, this, recheck);
        connect(field.edit, &QKeySequenceEdit::keySequenceChanged, this, [recheck](const QKeySequence &key) {
            if (key.isEmpty()) recheck();
        });
    }
}

void DialogIntegration::setupApiTab()
{
    const auto &settings = Configs::dataManager->settingsRepo;
    const auto key = RemoteApi::KeyFromText(settings->remote_api_key);
    savedApiKey = key.isEmpty() ? QString() : RemoteApi::KeyToText(key);
    ui->api_enable->setChecked(settings->remote_api_enable);
    ui->api_listen->setCurrentIndex(settings->remote_api_lan ? 1 : 0);
    ui->api_port->setValue(IsValidPort(settings->remote_api_port) ? settings->remote_api_port : RemoteApi::kDefaultPort);
    ui->api_allow->setText(settings->remote_api_allow);
    ui->api_admin_lan->setChecked(settings->remote_api_admin_lan);
    ui->api_key->setText(savedApiKey);
    if (ui->api_enable->isChecked() && savedApiKey.isEmpty()) ui->api_key->setText(RemoteApi::KeyToText(RemoteApi::GenerateKey()));
    ui->api_help->setText(tr("Scripts connect with thronectl or the Throne API library.").toHtmlEscaped()
                          + QStringLiteral("<br><a href=\"https://github.com/throneproj/Throne-api\">https://github.com/throneproj/Throne-api</a>"));

    auto *server = RemoteApi::Server::instance();
    ui->api_status->setText(server->statusText());
    connect(server, &RemoteApi::Server::statusChanged, this, [this](const QString &text) { ui->api_status->setText(text); });

    connect(ui->api_enable, &QCheckBox::toggled, this, [this](bool on) {
        if (on && ui->api_key->text().isEmpty()) ui->api_key->setText(RemoteApi::KeyToText(RemoteApi::GenerateKey()));
        updateApiControls();
    });
    connect(ui->api_listen, &QComboBox::currentIndexChanged, this, [this] { updateApiControls(); });
    connect(ui->api_key_show, &QPushButton::toggled, this, [this](bool on) {
        ui->api_key->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
        ui->api_key_show->setText(on ? tr("Hide") : tr("Show"));
    });
    connect(ui->api_key_copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(ui->api_key->text()); });
    connect(ui->api_key_regenerate, &QPushButton::clicked, this, [this] { regenerateApiKey(); });
    connect(ui->api_copy_env, &QPushButton::clicked, this, [this] { copyForScripts(); });
}

void DialogIntegration::updateApiControls()
{
    const bool enabled = ui->api_enable->isChecked();
    const bool lan = ui->api_listen->currentIndex() == 1;
    for (auto *widget : std::initializer_list<QWidget *>{ui->api_listen_l, ui->api_listen, ui->api_port_l, ui->api_port, ui->api_key_l, ui->api_key,
                                                         ui->api_key_show, ui->api_key_copy, ui->api_key_regenerate, ui->api_copy_env}) {
        widget->setEnabled(enabled);
    }
    ui->api_allow_l->setEnabled(enabled && lan);
    ui->api_allow->setEnabled(enabled && lan);
    ui->api_admin_lan->setEnabled(enabled && lan);
    ui->api_lan_warning->setVisible(enabled && lan);
}

void DialogIntegration::regenerateApiKey()
{
    if (!savedApiKey.isEmpty() && ui->api_key->text() == savedApiKey) {
        QMessageBox box(QMessageBox::Question, tr("Regenerate Key"), tr("Generate a new key?"), QMessageBox::Yes | QMessageBox::Cancel, this);
        box.setInformativeText(tr("Scripts using the current key will stop working."));
        if (box.exec() != QMessageBox::Yes) return;
    }
    ui->api_key->setText(RemoteApi::KeyToText(RemoteApi::GenerateKey()));
}

void DialogIntegration::copyForScripts()
{
    auto host = QStringLiteral("127.0.0.1");
    if (ui->api_listen->currentIndex() == 1) {
        if (const auto lan = LocalNetwork::LanAddress(); !lan.isEmpty()) host = lan.contains(':') ? "[" + lan + "]" : lan;
    }
    QApplication::clipboard()->setText(QStringLiteral("THRONE_ADDR=%1:%2\nTHRONE_KEY=%3").arg(host, QString::number(ui->api_port->value()), ui->api_key->text()));
}

void DialogIntegration::accept()
{
    if (!form->validateAll()) return;

    const auto &settings = Configs::dataManager->settingsRepo;
    settings->hotkey_mainwindow = ui->show_mainwindow->keySequence().toString();
    settings->hotkey_group = ui->show_groups->keySequence().toString();
    settings->hotkey_route = ui->show_routes->keySequence().toString();
    settings->hotkey_system_proxy_menu = ui->system_proxy->keySequence().toString();
    settings->hotkey_toggle_system_proxy = ui->toggle_proxy->keySequence().toString();
    settings->hotkey_toggle_connection = ui->toggle_connection->keySequence().toString();
    settings->hotkey_toggle_tun = ui->toggle_tun->keySequence().toString();

    auto mp = seqEdit2ID.toStdMap();
    for (const auto& [kseq, actionID] : mp)
    {
        settings->shortcuts[actionID] = kseq->keySequence();
    }

    const bool apiEnable = ui->api_enable->isChecked();
    const bool apiLan = ui->api_listen->currentIndex() == 1;
    const int apiPort = ui->api_port->value();
    const auto apiKey = ui->api_key->text();
    const auto apiAllow = ui->api_allow->text().trimmed();
    const bool apiAdminLan = ui->api_admin_lan->isChecked();
    const bool apiChanged = apiEnable != settings->remote_api_enable || apiLan != settings->remote_api_lan || apiPort != settings->remote_api_port
                            || apiKey != settings->remote_api_key || apiAllow != settings->remote_api_allow
                            || apiAdminLan != settings->remote_api_admin_lan;
    settings->remote_api_enable = apiEnable;
    settings->remote_api_lan = apiLan;
    settings->remote_api_port = apiPort;
    settings->remote_api_key = apiKey;
    settings->remote_api_allow = apiAllow;
    settings->remote_api_admin_lan = apiAdminLan;
    settings->Save();

    MW_dialog_message(MwMessage::UpdateShortcuts, {});
    if (apiChanged) MW_dialog_message(MwMessage::UpdateSettings, {MwArg::RemoteApi});
    const auto failures = GetMainWindow()->RegisterHotkey(false);
    if (!failures.isEmpty() && GetMainWindow()->IsGlobalHotkeySupported()) {
        QMessageBox box(QMessageBox::Warning, tr("Hotkey"), tr("These global hotkeys could not be registered:") + "\n\n" + failures.join('\n'), QMessageBox::Ok, this);
        box.setTextFormat(Qt::PlainText);
        box.exec();
    }
    QDialog::accept();
}

void DialogIntegration::reject()
{
    GetMainWindow()->RegisterHotkey(false);
    QDialog::reject();
}

DialogIntegration::~DialogIntegration() {
    delete ui;
}
