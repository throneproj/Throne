#include "include/ui/profile/edit_wireguard.h"

#include "include/ui/profile/edit_wireguard_amnezia.h"

#include <QPointer>

#include <limits>

#include "include/configs/sub/warp.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

namespace {
    // The Amnezia Options labels, translated in that dialog's context.
    QString editWireguardAmneziaLabel(const QString &field) {
        static const QList<std::pair<const char *, const char *>> labels = {
            {"s1", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Init Packet Junk Size (S1)")},
            {"s2", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Response Packet Junk Size (S2)")},
            {"s3", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Cookie Reply Packet Junk Size (S3)")},
            {"s4", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Transport Packet Junk Size (S4)")},
            {"h1", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Init Packet Magic Header (H1)")},
            {"h2", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Response Packet Magic Header (H2)")},
            {"h3", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Cookie Reply Packet Magic Header (H3)")},
            {"h4", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Transport Packet Magic Header (H4)")},
            {"i1", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Signature Packet (I1)")},
            {"i2", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Signature Packet (I2)")},
            {"i3", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Signature Packet (I3)")},
            {"i4", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Signature Packet (I4)")},
            {"i5", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Signature Packet (I5)")},
            {"header_protection_key", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Header Protection Key")},
            {"content_padding_addition", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Content Padding Addition")},
            {"rekey_after_time", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Rekey After Time")},
            {"rekey_timeout", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Rekey Timeout")},
            {"reject_after_time", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Reject After Time")},
            {"keepalive_timeout", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Keepalive Timeout")},
            {"max_handshake_attempts", QT_TRANSLATE_NOOP("EditWireguardAmnezia", "Max Handshake Attempts")},
        };
        for (const auto &[key, label]: labels) {
            if (field == QLatin1String(key)) return QCoreApplication::translate("EditWireguardAmnezia", label);
        }
        return field;
    }
}

EditWireguard::EditWireguard(QWidget *parent) : QWidget(parent), ui(new Ui::EditWireguard) {
    ui->setupUi(this);

    connect(ui->amnezia_options, &QPushButton::clicked, this, [=, this] {
        if (ent == nullptr) return;
        auto *amnezia = new EditWireguardAmnezia(this, ent);
        amnezia->setAttribute(Qt::WA_DeleteOnClose);
        // The dialog writes into the profile, which the Enable Amnezia rule reads.
        connect(amnezia, &QDialog::accepted, this, [this] {
            if (form) form->revalidate(ui->enable_amnezia);
        });
        amnezia->show();
    });

    connect(ui->warp_autogen, &QPushButton::clicked, this, [this] {
        if (!Configs_network::ConfirmWarpTerms(this)) return;

        const auto originalText = ui->warp_autogen->text();
        ui->warp_autogen->setEnabled(false);
        ui->warp_autogen->setText(tr("Generating config..."));

        QPointer<EditWireguard> self(this);
        runOnNewThread([self, originalText] {
            QString error;
            const auto conf = Configs_network::RegisterWarp("wireguard", &error);
            runOnUiThread([self, originalText, conf, error] {
                if (self == nullptr) return;
                auto *editor = self.data();
                if (!conf) {
                    editor->ui->warp_autogen->setText(originalText);
                    editor->ui->warp_autogen->setEnabled(true);
                    MessageBoxWarning(tr("Failed to generate warp config"), error);
                    return;
                }
                editor->ui->private_key->setText(conf->privateKey);
                editor->ui->public_key->setText(conf->peerPublicKey);
                editor->ui->local_addr->setText(conf->ipv4 + "/32," + conf->ipv6 + "/128");
                editor->ui->mtu->setText("1280");
                editor->ui->persistent_keepalive->setText("30");
                // The endpoint lives in the outer profile dialog's address/port fields.
                if (auto sep = conf->endpoint.lastIndexOf(':'); sep > 0) {
                    auto host = conf->endpoint.left(sep);
                    if (host.startsWith('[') && host.endsWith(']')) host = host.mid(1, host.length() - 2);
                    if (editor->set_edit_text_serverAddress) editor->set_edit_text_serverAddress(host);
                    if (editor->set_edit_text_serverPort) editor->set_edit_text_serverPort(conf->endpoint.mid(sep + 1));
                }
                editor->ui->reserved->setText(QListInt2QListString(conf->reserved).join(","));
                editor->ui->warp_autogen->setText(tr("Success!"));
                setTimeout([editor, originalText] {
                    editor->ui->warp_autogen->setText(originalText);
                    editor->ui->warp_autogen->setEnabled(true);
                }, editor, 2000);
            });
        });
    });
}

EditWireguard::~EditWireguard() {
    delete ui;
}

void EditWireguard::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Wireguard();

#ifndef Q_OS_LINUX
    adjustSize();
#endif

    ui->private_key->setText(outbound->private_key);
    ui->public_key->setText(outbound->peer->public_key);
    ui->preshared_key->setText(outbound->peer->pre_shared_key);
    ui->reserved->setText(QListInt2QListString(outbound->peer->reserved).join(","));
    ui->persistent_keepalive->setText(outbound->peer->persistent_keepalive);
    ui->mtu->setText(Int2String(outbound->mtu));
    ui->sys_ifc->setChecked(outbound->system);
    ui->local_addr->setText(outbound->address.join(","));
    ui->workers->setText(Int2String(outbound->worker_count));

    ui->enable_amnezia->setChecked(outbound->enable_amnezia);
}

bool EditWireguard::onEnd() {
    auto outbound = this->ent->Wireguard();

    outbound->private_key = ui->private_key->text().trimmed();
    outbound->peer->public_key = ui->public_key->text().trimmed();
    outbound->peer->pre_shared_key = ui->preshared_key->text().trimmed();
    Configs::Checks::WireguardReserved(ui->reserved->text(), &outbound->peer->reserved);
    outbound->peer->persistent_keepalive = ui->persistent_keepalive->text().trimmed();
    outbound->mtu = ui->mtu->text().trimmed().toInt();
    outbound->system = ui->sys_ifc->isChecked();
    outbound->address = SplitAndTrim(ui->local_addr->text(), ",", false);
    outbound->FixAddress();
    outbound->worker_count = ui->workers->text().trimmed().toInt();

    outbound->enable_amnezia = ui->enable_amnezia->isChecked();

    return true;
}

void EditWireguard::bindValidation(Validation::Form &form) {
    using namespace Validation;
    this->form = &form;
    constexpr qint64 kIntMax = std::numeric_limits<int>::max();

    form.bind(ui->private_key, Rules::Base64Key(32), this);
    form.bind(ui->public_key, Rules::Base64Key(32), this);
    form.bind(ui->preshared_key, Rules::Base64Key(32, false, true, Empty::Allowed), this);
    form.bind(ui->reserved, [](const QString &text) { return Configs::Checks::WireguardReserved(text); }, this);
    form.bind(ui->persistent_keepalive, Configs::Checks::WireguardKeepalive, this);
    // Bare addresses are saved with /32 or /128.
    form.bind(ui->local_addr, Rules::List(Split::Comma, Rules::Cidr(Family::Any, true), Empty::Required), this);
    form.bind(ui->mtu, Rules::Int(0, kIntMax), this);
    form.bind(ui->workers, Rules::Int(0, kIntMax), this);
    form.addRule({ui->enable_amnezia}, [this] {
        if (!ui->enable_amnezia->isChecked() || ent == nullptr) return QString();
        for (const auto &issue: ent->Wireguard()->AmneziaIssues()) {
            if (issue.severity == Severity::Error) {
                return tr("Open Amnezia Options and fix %1: %2").arg(editWireguardAmneziaLabel(issue.field), issue.message);
            }
        }
        return QString();
    }, this);
}