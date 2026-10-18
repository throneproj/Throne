#include "include/ui/profile/dialog_edit_profile.h"

#include "include/configs/generate.h"

#include "include/ui/profile/edit_advanced.h"
#include "include/ui/profile/edit_anytls.h"
#include "include/ui/profile/edit_autoselector.h"
#include "include/ui/profile/edit_chain.h"
#include "include/ui/profile/edit_custom.h"
#include "include/ui/profile/edit_direct.h"
#include "include/ui/profile/edit_extra_core.h"
#include "include/ui/profile/edit_http.h"
#include "include/ui/profile/edit_hysteria.h"
#include "include/ui/profile/edit_juicity.h"
#include "include/ui/profile/edit_masque.h"
#include "include/ui/profile/edit_mieru.h"
#include "include/ui/profile/edit_naive.h"
#include "include/ui/profile/edit_openconnect.h"
#include "include/ui/profile/edit_openvpn.h"
#include "include/ui/profile/edit_shadowsocks.h"
#include "include/ui/profile/edit_shadowtls.h"
#include "include/ui/profile/edit_snell.h"
#include "include/ui/profile/edit_socks.h"
#include "include/ui/profile/edit_ssh.h"
#include "include/ui/profile/edit_tailscale.h"
#include "include/ui/profile/edit_trojan.h"
#include "include/ui/profile/edit_trusttunnel.h"
#include "include/ui/profile/edit_tuic.h"
#include "include/ui/profile/edit_vless.h"
#include "include/ui/profile/edit_vmess.h"
#include "include/ui/profile/edit_wireguard.h"
#include "include/ui/profile/edit_xrayvless.h"

#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/ui/utils/GuiUtils.hpp"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QSet>

namespace {
using EditorFactory = std::pair<QWidget *, ProfileEditor *> (*)(QWidget *);

template <class Editor>
std::pair<QWidget *, ProfileEditor *> makeEditor(QWidget *parent) {
    auto *editor = new Editor(parent);
    return {editor, editor};
}

EditorFactory editorFactory(const QString &type) {
    static const QHash<QString, EditorFactory> factories = {
        {"socks", makeEditor<EditSocks>},
        {"http", makeEditor<EditHttp>},
        {"shadowsocks", makeEditor<EditShadowSocks>},
        {"trojan", makeEditor<EditTrojan>},
        {"vmess", makeEditor<EditVMess>},
        {"vless", makeEditor<EditVless>},
        {"xrayvless", makeEditor<EditXrayVless>},
        {"hysteria", makeEditor<EditHysteria>},
        {"tuic", makeEditor<EditTuic>},
        {"juicity", makeEditor<EditJuicity>},
        {"naive", makeEditor<EditNaive>},
        {"trusttunnel", makeEditor<EditTrustTunnel>},
        {"anytls", makeEditor<EditAnyTLS>},
        {"mieru", makeEditor<EditMieru>},
        {"snell", makeEditor<EditSnell>},
        {"shadowtls", makeEditor<EditShadowTLS>},
        {"wireguard", makeEditor<EditWireguard>},
        {"masque", makeEditor<EditMasque>},
        {"openvpn", makeEditor<EditOpenVPN>},
        {"openconnect", makeEditor<EditOpenConnect>},
        {"tailscale", makeEditor<EditTailScale>},
        {"ssh", makeEditor<EditSSH>},
        {"direct", makeEditor<EditDirect>},
        {"chain", makeEditor<EditChain>},
        {"autoselector", makeEditor<EditAutoSelector>},
        {"extracore", makeEditor<EditExtraCore>},
        {"custom", makeEditor<EditCustom>},
    };
    return factories.value(type);
}

bool isCustomSubtype(const QString &type) {
    return type == Configs::Custom::CustomOutbound || type == Configs::Custom::CustomFullConfig ||
           type == Configs::Custom::CustomXrayOutbound || type == Configs::Custom::CustomXrayFullConfig;
}

bool hasServerAddress(const QString &type) {
    static const QSet<QString> serverless = {"chain", "autoselector", "direct", "extracore", "tailscale", "custom"};
    return !serverless.contains(type);
}

bool serverFieldsEditable(const QString &type, ProfileEditor *editor) {
    return hasServerAddress(type) && editor != nullptr && !editor->locksServerAddress();
}

// What an endpoint source makes the profile dial instead of its own address.
Configs::EndpointResolution endpointOverride(const std::shared_ptr<Configs::Profile> &ent, bool editable) {
    if (!editable || !Configs::EndpointOverrideBlocker(ent).isEmpty()) return {};
    return Configs::ResolveEndpointSource(Configs::EffectiveEndpointSource(*ent));
}

QWidget *effectiveFocusWidget(QWidget *widget) {
    QSet<QWidget *> visited;
    while (widget && widget->focusProxy() && !visited.contains(widget)) {
        visited.insert(widget);
        widget = widget->focusProxy();
    }
    return widget;
}

QList<QWidget *> collectTabOrder(QWidget *window, const QWidget *subtree = nullptr,
                                 const QWidget *marker = nullptr) {
    if (!window) return {};

    QList<QWidget *> tabOrder;
    QSet<QWidget *> visited;
    auto *current = window;
    while (true) {
        auto *next = current->nextInFocusChain();
        if (!next || next == window || visited.contains(next)) break;
        visited.insert(next);
        current = next;

        if (subtree && current != subtree && !subtree->isAncestorOf(current)) continue;

        const bool isMarker = current == marker;
        if (!isMarker && !(current->focusPolicy() & Qt::TabFocus)) continue;

        auto *focusWidget = isMarker ? current : effectiveFocusWidget(current);
        if (!focusWidget || tabOrder.contains(focusWidget)) continue;
        if (subtree && focusWidget != subtree && !subtree->isAncestorOf(focusWidget)) continue;
        tabOrder.append(focusWidget);
    }

    return tabOrder;
}

void rebuildTabOrder(const QList<QWidget *> &tabOrder) {
    for (qsizetype i = 1; i < tabOrder.size(); ++i) {
        QWidget::setTabOrder(tabOrder.at(i - 1), tabOrder.at(i));
    }
}
}

DialogEditProfile::DialogEditProfile(const QString &_type, int profileOrGroupId, QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogEditProfile) {
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);

    outerTabOrder = collectTabOrder(this, nullptr, ui->fake);
    innerTabOrderIndex = outerTabOrder.indexOf(ui->fake);
    if (innerTabOrderIndex >= 0) outerTabOrder.removeAt(innerTabOrderIndex);

    ui->dialog_layout->setStretch(0, 0);
    ui->dialog_layout->setStretch(1, 1);
    ui->dialog_layout->setStretch(2, 1);
    ui->dialog_layout->setAlignment(ui->left_w, Qt::AlignTop);
    ui->dialog_layout->setAlignment(ui->right_all_w, Qt::AlignTop);
    ui->left->setAlignment(Qt::AlignTop);
    ui->right_layout->setAlignment(Qt::AlignTop);
    ui->verticalLayout_5->setAlignment(Qt::AlignTop);
    ui->verticalLayout_8->setAlignment(Qt::AlignTop);
    ui->port->setValidator(QRegExpValidator_Number);
    form->bind(ui->address, [this](const QString &text) {
        const auto empty = innerEditor->serverAddressOptional() ? Validation::Empty::Allowed : Validation::Empty::Required;
        const auto address = QString(text).remove(' ');
        if (type == "openvpn" && (address.startsWith('[') || address.endsWith(']'))) return Configs::Checks::OpenVPNRemoteHost(address);
        return Validation::Host(address, empty);
    }).when([this] { return serverFieldsEditable(type, innerEditor); });
    form->bind(ui->port, [this](const QString &text) {
        if (innerEditor->serverPortOptional() && (text.isEmpty() || Validation::ToInt(text) == 0)) return QString();
        return Validation::Port(text);
    }).when([this] { return serverFieldsEditable(type, innerEditor); });

    setupSingboxStream();
    setupXrayStream();

    addressEffective = new QLineEdit(this);
    addressEffective->setEnabled(false);
    addressEffective->hide();
    ui->gridLayout_2->addWidget(addressEffective, 2, 1);

    connect(ui->advanced_button, &QPushButton::clicked, this, [this] {
        auto *advanced = new EditAdvanced(this, ent);
        advanced->setAttribute(Qt::WA_DeleteOnClose);
        connect(advanced, &QDialog::accepted, this, [this] {
            updateControls();
            // Rules anchored here also read what Advanced just wrote into the profile.
            for (QWidget *anchor: std::initializer_list<QWidget *>{ui->reality_pbk, ui->certificate_edit, ui->sni, ui->utlsFingerprint}) {
                form->revalidate(anchor);
            }
        });
        advanced->show();
    });

    newEnt = !_type.isEmpty();
    if (newEnt) {
        groupId = profileOrGroupId;
        type = _type;
        setupTypeList();
    } else {
        ent = Configs::dataManager->profilesRepo->GetProfile(profileOrGroupId);
        if (ent == nullptr) {
            deleteLater();
            return;
        }
        editLock.emplace(Engine::EditTarget::Profile, ent->id);
        type = ent->type;
        setRowVisible(ui->type_l, ui->type, false);
    }

    if (!typeSelected(type)) {
        deleteLater();
        return;
    }
    runOnThread([self = QPointer<DialogEditProfile>(this)] {
        if (self == nullptr) return;
        self->fitToContent();
        self->show();
    }, this);
}

DialogEditProfile::~DialogEditProfile() {
    delete form;
    form = nullptr;
    delete ui;
}

void DialogEditProfile::setupTypeList() {
    const auto addType = [this](const QString &t) {
        ui->type->addItem(Configs::dataManager->profilesRepo->NewProfile(t)->outbound->DisplayType(), t);
    };
    for (const auto *t: {"autoselector", "socks", "http", "shadowsocks", "trojan", "vmess", "vless", "xrayvless",
                         "hysteria", "tuic", "juicity", "naive", "trusttunnel", "anytls", "mieru", "snell",
                         "shadowtls", "wireguard", "masque", "openvpn", "openconnect", "tailscale", "ssh", "direct"}) {
        addType(t);
    }
    ui->type->addItem(tr("Custom (%1 outbound)").arg(software_core_name), Configs::Custom::CustomOutbound);
    ui->type->addItem(tr("Custom (%1 config)").arg(software_core_name), Configs::Custom::CustomFullConfig);
    ui->type->addItem(tr("Custom (Xray outbound)"), Configs::Custom::CustomXrayOutbound);
    ui->type->addItem(tr("Custom (Xray config)"), Configs::Custom::CustomXrayFullConfig);
    ui->type->addItem(tr("Extra Core"), "extracore");
    addType("chain");

    ui->type->setCurrentIndex(qMax(0, ui->type->findData(type)));
    connect(ui->type, &QComboBox::currentIndexChanged, this, [this](int index) {
        typeSelected(ui->type->itemData(index).toString());
    });
}

bool DialogEditProfile::typeSelected(const QString &newType) {
    auto editorType = newType;
    QString customType;
    if (editorType == "custom" || isCustomSubtype(editorType)) {
        customType = newEnt ? editorType : ent->Custom()->type;
        editorType = "custom";
    }
    const auto factory = editorFactory(editorType);
    if (factory == nullptr) {
        MessageBoxWarning(newType, "Wrong type");
        return false;
    }

    {
        // Ends with form->refresh(), which re-checks fields the user marked before a type switch.
        Validation::Form::Pause pause(*form);
        type = editorType;
        if (newEnt) {
            ent = Configs::dataManager->profilesRepo->NewProfile(type);
            ent->gid = groupId;
        }

        const auto [widget, editor] = factory(this);
        if (auto *custom = qobject_cast<EditCustom *>(widget)) custom->preset_core = customType;
        mountEditor(widget, editor);
        ui->bean->setTitle(ent->outbound->DisplayType());

        loadSingboxStream();
        loadXrayStream();
        innerEditor->onStart(ent);
        innerEditor->bindValidation(*form);
        ui->name->setText(ent->outbound->name);
        ui->address->setText(ent->outbound->GetAddress());
        ui->port->setText(ent->outbound->GetPort());

        ADD_ASTERISK(this)
        editor_cache_updated_impl();
        relayout();
    }
    if (!newEnt) form->markAll();
    return true;
}

void DialogEditProfile::mountEditor(QWidget *widget, ProfileEditor *editor) {
    auto *beanLayout = ui->bean->layout();
    auto *old = beanLayout->itemAt(0)->widget();
    beanLayout->removeWidget(old);
    delete old;

    widget->layout()->setContentsMargins(0, 0, 0, 0);
    beanLayout->addWidget(widget);
    innerWidget = widget;
    innerEditor = editor;

    const auto innerTabOrder = collectTabOrder(this, widget);
    if (!innerTabOrder.isEmpty() && innerTabOrderIndex >= 0 && innerTabOrderIndex <= outerTabOrder.size()) {
        auto tabOrder = outerTabOrder.mid(0, innerTabOrderIndex);
        tabOrder.append(innerTabOrder);
        tabOrder.append(outerTabOrder.mid(innerTabOrderIndex));
        rebuildTabOrder(tabOrder);
    }

    editor->get_edit_dialog = [this] { return static_cast<QWidget *>(this); };
    editor->get_edit_text_name = [this] { return ui->name->text(); };
    editor->get_edit_text_serverAddress = [this] { return ui->address->text(); };
    editor->get_edit_text_serverPort = [this] { return ui->port->text(); };
    editor->set_edit_text_serverAddress = [this](const QString &v) { ui->address->setText(v); };
    editor->set_edit_text_serverPort = [this](const QString &v) { ui->port->setText(v); };
    editor->editor_cache_updated = [this] { editor_cache_updated_impl(); };
    editor->editor_state_changed = [this] { relayout(); };
}

// Visibility and enabled state are derived from the profile and the current input, never from other widgets.
void DialogEditProfile::updateControls() {
    if (ent == nullptr || innerEditor == nullptr) return;
    updateCommonRows();
    updateSingboxRows();
    updateTlsControlsEnabled();
    updateXrayRows();
}

void DialogEditProfile::updateCommonRows() {
    const bool server = hasServerAddress(type);
    const bool locked = innerEditor->locksServerAddress();
    for (QWidget *w: std::initializer_list<QWidget *>{ui->address_l, ui->address, ui->port_l, ui->port}) {
        w->setVisible(server);
        w->setEnabled(!locked);
    }
    const auto endpoint = endpointOverride(ent, server && !locked);
    const bool overridden = !endpoint.address.isEmpty();
    ui->address->setVisible(server && !overridden);
    addressEffective->setVisible(overridden);
    addressEffective->setText(endpoint.address);
    const QString own = ui->address->text().trimmed();
    addressEffective->setToolTip(!overridden ? QString()
                                 : own.isEmpty() ? tr("Set by %1.").arg(endpoint.origin)
                                                 : tr("Set by %1; the profile's own address is %2.").arg(endpoint.origin, own));
    ui->advanced_button->setVisible(server || type == "direct");
}

void DialogEditProfile::relayout() {
    updateControls();
    if (form != nullptr) form->refresh();
    runOnThread([self = QPointer<DialogEditProfile>(this)] {
        if (self != nullptr) self->fitToContent();
    }, this);
}

void DialogEditProfile::fitToContent() {
    adjustSize();
    adjustPosition(mainwindow);
}

bool DialogEditProfile::onEnd() {
    if (!form->validateAll()) return false;
    if (!innerEditor->onEnd()) return false;

    ent->outbound->name = ui->name->text().trimmed();
    ent->outbound->SetAddress(ui->address->text().remove(' '));
    ent->outbound->SetPort(ui->port->text().trimmed().toInt());
    saveSingboxStream();
    saveXrayStream();
    return true;
}

void DialogEditProfile::accept() {
    if (!onEnd()) return;

    QStringList args;
    if (newEnt) {
        if (!Configs::dataManager->profilesRepo->AddProfile(ent, groupId)) MessageBoxWarning("???", "id exists");
    } else if (!Configs::dataManager->profilesRepo->Save(ent)) {
        MessageBoxWarning(software_name, tr("This profile was deleted while you were editing it."));
        QDialog::reject();
        return;
    } else if (Configs::dataManager->settingsRepo->started_id == ent->id || Configs::RunningUsesProfile(ent->id)) {
        args << MwArg::RestartProxy;
    }
    if (!newEnt) Configs::RepositionLater(QList<std::shared_ptr<Configs::Profile>>{ent}, Configs::SortInput::Config);

    MW_dialog_message(MwMessage::ProfileChanged, args);
    QDialog::accept();
}

void DialogEditProfile::editor_cache_updated_impl() {
    setCacheButtonText(ui->certificate_edit, !CACHE.certificate.isEmpty());
    setCacheButtonText(ui->xray_downloadsettings_edit, !CACHE.XrayDownloadSettings.isEmpty());
    setCacheButtonText(ui->xray_finalmask_edit, !CACHE.XrayFinalmask.isEmpty());
    for (const auto &[button, value]: innerEditor->get_editor_cached()) {
        setCacheButtonText(button, !value.isEmpty());
    }
}

void DialogEditProfile::setCacheButtonText(QPushButton *button, bool isSet) {
    button->setText(isSet ? tr("Already set") : tr("Not set"));
}

void DialogEditProfile::setRowVisible(QWidget *label, QWidget *field, bool visible) {
    label->setVisible(visible);
    field->setVisible(visible);
}

void DialogEditProfile::selectComboText(QComboBox *combo, const QString &text) {
    combo->setCurrentIndex(qMax(0, combo->findText(text)));
}
