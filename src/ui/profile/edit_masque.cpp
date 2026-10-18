#include "include/ui/profile/edit_masque.h"

#include <QPointer>

#include "include/configs/sub/warp.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/ValidationForm.h"

namespace {
    constexpr int kHttp3Fallback = 0;
    constexpr int kHttp3Only = 1;
    constexpr int kHttp2 = 2;
}

EditMasque::EditMasque(QWidget *parent) : QWidget(parent), ui(new Ui::EditMasque) {
    ui->setupUi(this);
    ui->mtu->setValidator(QRegExpValidator_Number);

    connect(ui->warp_autogen, &QPushButton::clicked, this, [this] { generateWarpIdentity(); });
}

EditMasque::~EditMasque() {
    delete ui;
}

void EditMasque::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Masque();

    ui->private_key->setText(outbound->private_key);
    ui->peer_public_key->setText(outbound->peer_public_key);
    ui->address->setText(outbound->address.join(","));
    ui->mtu->setText(Int2String(outbound->mtu));
    if (outbound->http_version == 2) ui->http_version->setCurrentIndex(kHttp2);
    else if (outbound->disable_version_fallback) ui->http_version->setCurrentIndex(kHttp3Only);
    else ui->http_version->setCurrentIndex(kHttp3Fallback);
}

bool EditMasque::onEnd() {
    auto outbound = this->ent->Masque();

    outbound->private_key = ui->private_key->text().trimmed();
    outbound->peer_public_key = ui->peer_public_key->text().trimmed();
    outbound->address = Configs::masque::HostPrefixed(ui->address->text().remove(' ').split(",", Qt::SkipEmptyParts));
    outbound->mtu = ui->mtu->text().trimmed().toInt();
    switch (ui->http_version->currentIndex()) {
        case kHttp3Only:
            outbound->http_version = 3;
            outbound->disable_version_fallback = true;
            break;
        case kHttp2:
            outbound->http_version = 2;
            outbound->disable_version_fallback = false;
            break;
        default:
            outbound->http_version = 0;
            outbound->disable_version_fallback = false;
            break;
    }

    return true;
}

void EditMasque::bindValidation(Validation::Form &form) {
    using namespace Validation;
    form.bind(ui->private_key, [](const QString &text) { return Configs::Checks::MasqueKey(text, Empty::Required); }, this);
    form.bind(ui->peer_public_key, [](const QString &text) { return Configs::Checks::MasqueKey(text, Empty::Allowed); }, this);
    // Same normalisation as the save path; bare addresses are saved with /32 or /128.
    form.bind(ui->address, [](const QString &text) {
        return EachItem(QString(text).remove(' '), Split::Comma, Rules::Cidr(Family::Any, true), Empty::Required);
    }, this);
}

void EditMasque::generateWarpIdentity() {
    if (!Configs_network::ConfirmWarpTerms(this)) return;

    const auto originalText = ui->warp_autogen->text();
    ui->warp_autogen->setEnabled(false);
    ui->warp_autogen->setText(tr("Generating identity..."));

    QPointer<EditMasque> self(this);
    runOnNewThread([self, originalText] {
        QString error;
        const auto identity = Configs_network::RegisterWarp("masque", &error);
        runOnUiThread([self, originalText, identity, error] {
            if (self == nullptr) return;
            auto *editor = self.data();
            if (!identity) {
                editor->ui->warp_autogen->setText(originalText);
                editor->ui->warp_autogen->setEnabled(true);
                MessageBoxWarning(tr("Failed to generate WARP identity"), error);
                return;
            }
            editor->applyWarpIdentity(*identity);
            editor->ui->warp_autogen->setText(tr("Success!"));
            setTimeout([editor, originalText] {
                editor->ui->warp_autogen->setText(originalText);
                editor->ui->warp_autogen->setEnabled(true);
            }, editor, 2000);
        });
    });
}

void EditMasque::applyWarpIdentity(const Configs_network::WarpIdentity &identity) {
    ui->private_key->setText(identity.privateKey);
    ui->peer_public_key->setText(identity.peerPublicKey);
    QStringList addresses;
    if (!identity.ipv4.isEmpty()) addresses << identity.ipv4 + "/32";
    if (!identity.ipv6.isEmpty()) addresses << identity.ipv6 + "/128";
    ui->address->setText(addresses.join(","));
    ui->mtu->setText("1280");

    const auto sep = identity.endpoint.lastIndexOf(':');
    if (sep <= 0) return;
    auto host = identity.endpoint.left(sep);
    if (host.startsWith('[') && host.endsWith(']')) host = host.mid(1, host.length() - 2);
    if (set_edit_text_serverAddress) set_edit_text_serverAddress(host);
    if (set_edit_text_serverPort) set_edit_text_serverPort(identity.endpoint.mid(sep + 1));
}
