#include "include/ui/profile/edit_openconnect_advanced.h"

#include <QGuiApplication>
#include <QHeaderView>
#include <QInputDialog>
#include <QLayout>
#include <QScreen>
#include <QScrollBar>
#include <QTableWidgetItem>

#include "include/ui/utils/GuiUtils.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/profile/editor_table_utils.h"
#include "include/ui/utils/ValidationForm.h"

#include <climits>

namespace {
    // The row shows a state; the PEM itself lives in the item's user role.
    constexpr int kPemRole = Qt::UserRole + 1;
}

EditOpenConnectAdvanced::EditOpenConnectAdvanced(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent,
                                                 std::function<void(Configs::openconnect &)> pageInput)
    : QDialog(parent)
    , ui(new Ui::EditOpenConnectAdvanced)
    , pageInput(std::move(pageInput))
{
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    ent = _ent;
    auto outbound = ent->OpenConnect();

    ui->token_mode->addItems({"", "totp", "hotp", "stoken", "oidc"});
    ui->compression_mode->addItems({"", "stateless", "all"});

    ui->tncc_certificates_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->form_entries_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tncc_certificates_table->verticalHeader()->setVisible(false);
    ui->form_entries_table->verticalHeader()->setVisible(false);

    ui->cookie->setText(outbound->cookie);
    ui->reported_os->setText(outbound->reported_os);
    ui->user_agent->setText(outbound->user_agent);
    ui->version->setText(outbound->version);
    ui->local_hostname->setText(outbound->local_hostname);

    auto token = outbound->token;
    ui->token_mode->setCurrentText(token->mode);
    ui->token_secret->setText(token->secret);
    ui->token_secret_path->setText(token->secret_path);
    ui->token_pin->setText(token->pin);
    ui->token_password->setText(token->password);
    ui->token_device_id->setText(token->device_id);
    ui->token_counter->setText(EditorNumText(token->counter));

    auto tls = outbound->tls;
    ui->tls_system_trust_disabled->setChecked(tls->system_trust_disabled);
    ui->tls_certificate_authority_path->setText(tls->certificate_authority_path);
    ui->tls_client_certificate_path->setText(tls->client_certificate_path);
    ui->tls_client_key_path->setText(tls->client_key_path);
    ui->tls_mca_certificate_path->setText(tls->mca_certificate_path);
    ui->tls_mca_key_path->setText(tls->mca_key_path);
    ui->tls_mca_key_password->setText(tls->mca_key_password);
    CACHE.peerFingerprint = tls->peer_fingerprint;
    CACHE.mcaCertificate = tls->mca_certificate;
    CACHE.mcaKey = tls->mca_key;

    ui->mobile_platform_version->setText(outbound->mobile->platform_version);
    ui->mobile_device_type->setText(outbound->mobile->device_type);
    ui->mobile_device_unique_id->setText(outbound->mobile->device_unique_id);

    ui->csd_wrapper_path->setText(outbound->csd->wrapper_path);
    ui->hip_wrapper_path->setText(outbound->hip->wrapper_path);
    ui->fortinet_hostcheck->setText(outbound->fortinet_host_check->hostcheck);
    ui->fortinet_check_virtual_desktop->setText(outbound->fortinet_host_check->check_virtual_desktop);

    ui->tncc_wrapper_path->setText(outbound->tncc->wrapper_path);
    ui->tncc_device_id->setText(outbound->tncc->device_id);
    ui->tncc_user_agent->setText(outbound->tncc->user_agent);
    ui->tncc_machine_identification_enabled->setChecked(outbound->tncc->machine_identification_enabled);
    for (const auto &cert: outbound->tncc->certificates) {
        addTnccCertificateRow(cert->certificate, cert->certificate_path);
    }

    for (const auto &entry: outbound->form_entries) {
        addFormEntryRow(entry->form_id, entry->submission_key, entry->name, entry->value, entry->promote);
    }

    ui->no_udp->setChecked(outbound->no_udp);
    ui->dtls_local_port->setText(EditorNumText(outbound->dtls_local_port));
    ui->compression_disabled->setChecked(outbound->compression_disabled);
    ui->compression_mode->setCurrentText(outbound->compression_mode);
    ui->ipv6_disabled->setChecked(outbound->ipv6_disabled);
    ui->http_keepalive_disabled->setChecked(outbound->http_keepalive_disabled);
    ui->xml_post_disabled->setChecked(outbound->xml_post_disabled);
    ui->external_auth_disabled->setChecked(outbound->external_auth_disabled);
    ui->password_authentication_disabled->setChecked(outbound->password_authentication_disabled);
    ui->tcp_keep_alive_enabled->setChecked(outbound->tcp_keep_alive_enabled);
    ui->pfs->setChecked(outbound->pfs);
    ui->allow_insecure_crypto->setChecked(outbound->allow_insecure_crypto);
    ui->base_mtu->setText(EditorNumText(outbound->base_mtu));
    ui->dpd_interval->setText(outbound->dpd_interval);
    ui->reconnect_timeout->setText(outbound->reconnect_timeout);
    ui->trojan_interval->setText(outbound->trojan_interval);
    ui->queue_length->setText(EditorNumText(outbound->queue_length));

    ui->tls_peer_fingerprint->setText(CACHE.peerFingerprint.isEmpty() ? tr("Not set") : tr("Already set"));
    ui->tls_mca_certificate->setText(CACHE.mcaCertificate.isEmpty() ? tr("Not set") : tr("Already set"));
    ui->tls_mca_key->setText(CACHE.mcaKey.isEmpty() ? tr("Not set") : tr("Already set"));

    connect(ui->tls_peer_fingerprint, &QPushButton::clicked, this,
            [=, this] { editPem(ui->tls_peer_fingerprint, tr("Peer Fingerprint"), CACHE.peerFingerprint); });
    connect(ui->tls_mca_certificate, &QPushButton::clicked, this,
            [=, this] { editPem(ui->tls_mca_certificate, tr("MCA Certificate"), CACHE.mcaCertificate); });
    connect(ui->tls_mca_key, &QPushButton::clicked, this,
            [=, this] { editPem(ui->tls_mca_key, tr("MCA Key"), CACHE.mcaKey); });

    connect(ui->tncc_certificates_table_add, &QPushButton::clicked, this,
            [=, this] { addTnccCertificateRow({}, ""); });
    connect(ui->tncc_certificates_table_remove, &QPushButton::clicked, this, [=, this] {
        ui->tncc_certificates_table->removeRow(ui->tncc_certificates_table->currentRow());
        form->revalidate(ui->tncc_certificates_table);
    });
    connect(ui->tncc_certificates_table_edit, &QPushButton::clicked, this, [=, this] { editTnccCertificate(); });

    connect(ui->form_entries_table_add, &QPushButton::clicked, this,
            [=, this] { addFormEntryRow("", "", "", "", false); });
    connect(ui->form_entries_table_remove, &QPushButton::clicked, this, [=, this] {
        ui->form_entries_table->removeRow(ui->form_entries_table->currentRow());
        form->revalidate(ui->form_entries_table);
    });
    // A committed cell edit (or a toggled Promote box) is the tables' focus-out.
    connect(ui->tncc_certificates_table, &QTableWidget::itemChanged, this, [this] { form->revalidate(ui->tncc_certificates_table); });
    connect(ui->form_entries_table, &QTableWidget::itemChanged, this, [this] { form->revalidate(ui->form_entries_table); });

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    bindValidation();

    ADD_ASTERISK(this)

    // QScrollArea::sizeHint() caps at 36 font heights, and the nested form layouts stay inert until activated.
    for (auto *nested: ui->scroll_content->findChildren<QLayout *>()) nested->activate();
    const auto margins = layout()->contentsMargins();
    const auto width = ui->scroll->widget()->sizeHint().width() + 2 * ui->scroll->frameWidth() +
                       ui->scroll->verticalScrollBar()->sizeHint().width() + margins.left() + margins.right();

    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    if (scr != nullptr) resize(QSize(width, sizeHint().height()).boundedTo(scr->availableGeometry().size()));

    form->markAll();
}

EditOpenConnectAdvanced::~EditOpenConnectAdvanced()
{
    delete ui;
}

void EditOpenConnectAdvanced::editPem(QPushButton *button, const QString &title, QStringList &target) {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, title, "", target.join("\n"), &ok);
    if (!ok) return;
    target = txt.split("\n", Qt::SkipEmptyParts);
    button->setText(target.isEmpty() ? tr("Not set") : tr("Already set"));
    form->revalidate(button);
}

void EditOpenConnectAdvanced::addTnccCertificateRow(const QStringList &certificate, const QString &path) {
    auto row = ui->tncc_certificates_table->rowCount();
    ui->tncc_certificates_table->insertRow(row);
    auto item = new QTableWidgetItem(certificate.isEmpty() ? tr("Not set") : tr("Already set"));
    item->setData(kPemRole, certificate.join("\n"));
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    ui->tncc_certificates_table->setItem(row, 0, item);
    ui->tncc_certificates_table->setItem(row, 1, new QTableWidgetItem(path));
}

void EditOpenConnectAdvanced::editTnccCertificate() {
    auto row = ui->tncc_certificates_table->currentRow();
    if (row < 0) return;
    auto item = ui->tncc_certificates_table->item(row, 0);
    if (item == nullptr) return;
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, tr("TNCC Machine Certificate"), "",
                                              item->data(kPemRole).toString(), &ok);
    if (!ok) return;
    auto lines = txt.split("\n", Qt::SkipEmptyParts);
    item->setData(kPemRole, lines.join("\n"));
    item->setText(lines.isEmpty() ? tr("Not set") : tr("Already set"));
    form->revalidate(ui->tncc_certificates_table);
}

void EditOpenConnectAdvanced::addFormEntryRow(const QString &formId, const QString &submissionKey,
                                              const QString &name, const QString &value, bool promote) {
    auto row = ui->form_entries_table->rowCount();
    ui->form_entries_table->insertRow(row);
    ui->form_entries_table->setItem(row, 0, new QTableWidgetItem(formId));
    ui->form_entries_table->setItem(row, 1, new QTableWidgetItem(submissionKey));
    ui->form_entries_table->setItem(row, 2, new QTableWidgetItem(name));
    ui->form_entries_table->setItem(row, 3, new QTableWidgetItem(value));
    auto promoteItem = new QTableWidgetItem();
    promoteItem->setFlags((promoteItem->flags() & ~Qt::ItemIsEditable) | Qt::ItemIsUserCheckable);
    promoteItem->setCheckState(promote ? Qt::Checked : Qt::Unchecked);
    ui->form_entries_table->setItem(row, 4, promoteItem);
}

void EditOpenConnectAdvanced::accept() {
    if (!form->validateAll()) return;
    saveInto(*ent->OpenConnect());
    QDialog::accept();
}

void EditOpenConnectAdvanced::saveInto(Configs::openconnect &outbound) const {
    outbound.cookie = ui->cookie->text().trimmed();
    outbound.reported_os = ui->reported_os->text().trimmed();
    outbound.user_agent = ui->user_agent->text().trimmed();
    outbound.version = ui->version->text().trimmed();
    outbound.local_hostname = ui->local_hostname->text().trimmed();

    auto token = outbound.token;
    token->mode = ui->token_mode->currentText().trimmed();
    token->secret = ui->token_secret->text().trimmed();
    token->secret_path = ui->token_secret_path->text().trimmed();
    token->pin = ui->token_pin->text();
    token->password = ui->token_password->text();
    token->device_id = ui->token_device_id->text().trimmed();
    token->counter = ui->token_counter->text().trimmed().toLongLong();

    auto tls = outbound.tls;
    tls->peer_fingerprint = {};
    for (const auto &line: CACHE.peerFingerprint) {
        if (const auto trimmed = line.trimmed(); !trimmed.isEmpty()) tls->peer_fingerprint << trimmed;
    }
    tls->system_trust_disabled = ui->tls_system_trust_disabled->isChecked();
    tls->certificate_authority_path = ui->tls_certificate_authority_path->text().trimmed();
    tls->client_certificate_path = ui->tls_client_certificate_path->text().trimmed();
    tls->client_key_path = ui->tls_client_key_path->text().trimmed();
    tls->mca_certificate = CACHE.mcaCertificate;
    tls->mca_certificate_path = ui->tls_mca_certificate_path->text().trimmed();
    tls->mca_key = CACHE.mcaKey;
    tls->mca_key_path = ui->tls_mca_key_path->text().trimmed();
    tls->mca_key_password = ui->tls_mca_key_password->text();

    outbound.mobile->platform_version = ui->mobile_platform_version->text().trimmed();
    outbound.mobile->device_type = ui->mobile_device_type->text().trimmed();
    outbound.mobile->device_unique_id = ui->mobile_device_unique_id->text().trimmed();

    outbound.csd->wrapper_path = ui->csd_wrapper_path->text().trimmed();
    outbound.hip->wrapper_path = ui->hip_wrapper_path->text().trimmed();
    outbound.fortinet_host_check->hostcheck = ui->fortinet_hostcheck->text().trimmed();
    outbound.fortinet_host_check->check_virtual_desktop = ui->fortinet_check_virtual_desktop->text().trimmed();

    outbound.tncc->wrapper_path = ui->tncc_wrapper_path->text().trimmed();
    outbound.tncc->device_id = ui->tncc_device_id->text().trimmed();
    outbound.tncc->user_agent = ui->tncc_user_agent->text().trimmed();
    outbound.tncc->machine_identification_enabled = ui->tncc_machine_identification_enabled->isChecked();
    outbound.tncc->certificates = {};
    for (int row = 0; row < ui->tncc_certificates_table->rowCount(); ++row) {
        auto item = ui->tncc_certificates_table->item(row, 0);
        auto pem = item == nullptr ? QString() : item->data(kPemRole).toString();
        auto path = EditorCellText(ui->tncc_certificates_table, row, 1);
        if (pem.isEmpty() && path.isEmpty()) continue;
        auto cert = std::make_shared<Configs::OpenConnectTNCCCertificate>();
        cert->certificate = pem.isEmpty() ? QStringList() : pem.split("\n", Qt::SkipEmptyParts);
        cert->certificate_path = path;
        outbound.tncc->certificates += cert;
    }

    outbound.form_entries = {};
    for (int row = 0; row < ui->form_entries_table->rowCount(); ++row) {
        auto formId = EditorCellText(ui->form_entries_table, row, 0);
        auto submissionKey = EditorCellText(ui->form_entries_table, row, 1);
        auto name = EditorCellText(ui->form_entries_table, row, 2);
        if (submissionKey.isEmpty() && (formId.isEmpty() || name.isEmpty())) continue;
        auto entry = std::make_shared<Configs::OpenConnectFormEntry>();
        entry->form_id = formId;
        entry->submission_key = submissionKey;
        entry->name = name;
        entry->value = EditorCellText(ui->form_entries_table, row, 3);
        auto promoteItem = ui->form_entries_table->item(row, 4);
        entry->promote = promoteItem != nullptr && promoteItem->checkState() == Qt::Checked;
        outbound.form_entries += entry;
    }

    outbound.no_udp = ui->no_udp->isChecked();
    outbound.dtls_local_port = ui->dtls_local_port->text().trimmed().toInt();
    outbound.compression_disabled = ui->compression_disabled->isChecked();
    outbound.compression_mode = ui->compression_mode->currentText().trimmed();
    outbound.ipv6_disabled = ui->ipv6_disabled->isChecked();
    outbound.http_keepalive_disabled = ui->http_keepalive_disabled->isChecked();
    outbound.xml_post_disabled = ui->xml_post_disabled->isChecked();
    outbound.external_auth_disabled = ui->external_auth_disabled->isChecked();
    outbound.password_authentication_disabled = ui->password_authentication_disabled->isChecked();
    outbound.tcp_keep_alive_enabled = ui->tcp_keep_alive_enabled->isChecked();
    outbound.pfs = ui->pfs->isChecked();
    outbound.allow_insecure_crypto = ui->allow_insecure_crypto->isChecked();
    outbound.base_mtu = ui->base_mtu->text().trimmed().toInt();
    outbound.dpd_interval = ui->dpd_interval->text().trimmed();
    outbound.reconnect_timeout = ui->reconnect_timeout->text().trimmed();
    outbound.trojan_interval = ui->trojan_interval->text().trimmed();
    outbound.queue_length = ui->queue_length->text().trimmed().toInt();
}

std::shared_ptr<Configs::openconnect> EditOpenConnectAdvanced::withInput() const {
    auto outbound = ent->OpenConnect()->DetachedCopy();
    if (pageInput) pageInput(*outbound);
    saveInto(*outbound);
    return outbound;
}

void EditOpenConnectAdvanced::bindValidation() {
    using namespace Validation;
    using namespace Configs::Checks;
    using Check = std::function<QString(const Configs::openconnect &)>;

    // The protocol picked on the profile page decides which reported OS names count.
    form->bind(ui->reported_os, [this](const QString &os) { return OpenConnectReportedOs(withInput()->flavor, os); });
    form->bind(ui->token_counter, Rules::Int(0, LLONG_MAX));
    form->bind(ui->dtls_local_port, [](const QString &port) { return port == "0" ? QString() : Validation::Port(port, Empty::Allowed); });
    form->bind(ui->base_mtu, Rules::Int(0, 65535));
    form->bind(ui->base_mtu, OpenConnectMtuFloor).warning();
    form->bind(ui->queue_length, Rules::Int(0, INT_MAX));
    form->bind(ui->dpd_interval, Rules::Duration());
    form->bind(ui->dpd_interval, OpenConnectDpdFloor).warning();
    form->bind(ui->reconnect_timeout, Rules::Duration());
    form->bind(ui->trojan_interval, Rules::Duration());

    form->addRule({ui->tls_peer_fingerprint}, [this] {
        return EachItem(CACHE.peerFingerprint.join('\n'), Split::Newline, OpenConnectFingerprint);
    }, this);
    form->addRule({ui->form_entries_table}, [this] {
        const auto *table = ui->form_entries_table;
        for (int row = 0; row < table->rowCount(); ++row) {
            const auto value = EditorCellText(table, row, 3);
            const auto *promoteItem = table->item(row, 4);
            const bool promote = promoteItem != nullptr && promoteItem->checkState() == Qt::Checked;
            const auto formId = EditorCellText(table, row, 0), submissionKey = EditorCellText(table, row, 1), name = EditorCellText(table, row, 2);
            if (formId.isEmpty() && submissionKey.isEmpty() && name.isEmpty() && value.isEmpty() && !promote) continue;
            if (auto message = OpenConnectFormEntry(formId, submissionKey, name, value, promote); !message.isEmpty()) {
                return tr("Row %1: %2").arg(QString::number(row + 1), message);
            }
        }
        return QString();
    }, this, Severity::Error, tr("Authentication Forms"));
    form->addRule({ui->tncc_certificates_table}, [this] {
        const auto *table = ui->tncc_certificates_table;
        for (int row = 0; row < table->rowCount(); ++row) {
            const auto *item = table->item(row, 0);
            const auto pem = item == nullptr ? QString() : item->data(kPemRole).toString();
            if (auto message = VpnMaterialConflict(pem.split('\n', Qt::SkipEmptyParts), EditorCellText(table, row, 1)); !message.isEmpty()) {
                return tr("Row %1: %2").arg(QString::number(row + 1), message);
            }
        }
        return QString();
    }, this, Severity::Error, tr("TNCC Machine Certificates"));

    const auto rule = [this](const QList<QWidget *> &participants, const Check &check, Severity severity = Severity::Error) {
        form->addRule(participants, [this, check] { return check(*withInput()); }, this, severity);
    };
    rule({ui->token_mode, ui->token_secret, ui->token_secret_path}, OpenConnectToken);
    rule({ui->token_counter, ui->token_mode}, OpenConnectTokenCounter, Severity::Warning);
    const QList<QPair<QWidget *, std::function<bool(const Configs::OpenConnectToken &)>>> tokenFields = {
        {ui->token_secret, [](const Configs::OpenConnectToken &t) { return !t.secret.isEmpty(); }},
        {ui->token_secret_path, [](const Configs::OpenConnectToken &t) { return !t.secret_path.isEmpty(); }},
        {ui->token_pin, [](const Configs::OpenConnectToken &t) { return !t.pin.isEmpty(); }},
        {ui->token_password, [](const Configs::OpenConnectToken &t) { return !t.password.isEmpty(); }},
        {ui->token_device_id, [](const Configs::OpenConnectToken &t) { return !t.device_id.isEmpty(); }},
        {ui->token_counter, [](const Configs::OpenConnectToken &t) { return t.counter > 0; }},
    };
    for (const auto &field: tokenFields) {
        rule({field.first, ui->token_mode}, [used = field.second](const Configs::openconnect &o) {
            return used(*o.token) ? OpenConnectTokenUnused(o) : QString();
        }, Severity::Warning);
    }
    rule({ui->mobile_platform_version, ui->mobile_device_type, ui->mobile_device_unique_id}, OpenConnectMobile);
    rule({ui->compression_mode, ui->compression_disabled}, OpenConnectCompression);
    rule({ui->tls_mca_certificate, ui->tls_mca_certificate_path}, [](const Configs::openconnect &o) {
        return VpnMaterialConflict(o.tls->mca_certificate, o.tls->mca_certificate_path);
    });
    rule({ui->tls_mca_key, ui->tls_mca_key_path}, [](const Configs::openconnect &o) { return VpnMaterialConflict(o.tls->mca_key, o.tls->mca_key_path); });
    rule({ui->tls_mca_certificate_path, ui->tls_mca_key_path, ui->tls_mca_certificate, ui->tls_mca_key}, [](const Configs::openconnect &o) {
        return OpenConnectCertificatePair(o.tls->mca_certificate, o.tls->mca_certificate_path, o.tls->mca_key, o.tls->mca_key_path);
    });

    // The pasted counterparts live on the profile page, so the conflict is fixed there as well.
    rule({ui->tls_certificate_authority_path}, [](const Configs::openconnect &o) {
        return VpnMaterialConflict(o.tls->certificate_authority, o.tls->certificate_authority_path);
    }, Severity::Warning);
    rule({ui->tls_client_certificate_path}, [](const Configs::openconnect &o) {
        return VpnMaterialConflict(o.tls->client_certificate, o.tls->client_certificate_path);
    }, Severity::Warning);
    rule({ui->tls_client_key_path}, [](const Configs::openconnect &o) {
        return VpnMaterialConflict(o.tls->client_key, o.tls->client_key_path);
    }, Severity::Warning);
    // The authenticator entry is picked on the profile page.
    rule({ui->token_pin}, [](const Configs::openconnect &o) { return OtpPlaceholderUnbound(o.token->pin, o.otp_profile_id); }, Severity::Warning);
    rule({ui->token_password}, [](const Configs::openconnect &o) {
        return OtpPlaceholderUnbound(o.token->password, o.otp_profile_id);
    }, Severity::Warning);
}
