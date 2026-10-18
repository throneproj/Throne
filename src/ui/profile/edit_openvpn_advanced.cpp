#include "include/ui/profile/edit_openvpn_advanced.h"

#include <QComboBox>
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
    const QStringList kPullFilterActions = {"accept", "ignore", "reject"};
    constexpr auto kOpenVPNDefaultPort = "1194";
}

EditOpenVPNAdvanced::EditOpenVPNAdvanced(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent,
                                         std::function<void(Configs::openvpn &)> pageInput)
    : QDialog(parent)
    , ui(new Ui::EditOpenVPNAdvanced)
    , pageInput(std::move(pageInput))
{
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    ent = _ent;
    auto outbound = ent->OpenVPN();

    ui->mode->addItems({"", "tls", "static_key"});
    ui->topology->addItems({"", "net30", "p2p", "subnet"});
    ui->auth_retry->addItems({"", "none", "nointeract", "interact"});
    ui->key_direction->addItems({"", "server", "client"});
    ui->tls_server_name_type->addItems({"", "subject", "name", "name-prefix"});
    ui->tls_remote_certificate_tls->addItems({"", "server", "client", "none"});
    ui->tls_certificate_profile->addItems({"", "legacy", "preferred", "insecure", "suiteb"});
    ui->tls_ns_certificate_type->addItems({"", "server", "client"});
    ui->tls_version_min->addItems({"", "1.0", "1.1", "1.2", "1.3"});
    ui->tls_version_max->addItems({"", "1.0", "1.1", "1.2", "1.3"});
    ui->mss_fix_mode->addItems({"", "mtu", "fixed"});
    ui->compression->addItems({"", "none", "no", "lz4", "lz4-v2", "stub", "stub-v2", "disabled", "off"});
    ui->compression_lzo->addItems({"", "none", "no", "yes", "adaptive", "asym", "disabled", "off"});
    ui->allow_compression->addItems({"", "no", "asym", "yes"});

    ui->servers_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->pull_filters_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->servers_table->verticalHeader()->setVisible(false);
    ui->pull_filters_table->verticalHeader()->setVisible(false);

    ui->mode->setCurrentText(outbound->mode);
    ui->topology->setCurrentText(outbound->topology);
    ui->auth_retry->setCurrentText(outbound->auth_retry);
    ui->remote_random->setChecked(outbound->remote_random);
    ui->address->setText(outbound->address.join(","));
    ui->peer_address->setText(outbound->peer_address);
    ui->peer_address_ipv6->setText(outbound->peer_address_ipv6);

    for (const auto &server: outbound->servers) {
        addServerRow(server->server, EditorNumText(server->server_port), server->network);
    }

    ui->static_key_path->setText(outbound->static_key_path);
    ui->key_direction->setCurrentText(outbound->key_direction);
    ui->cipher->setText(outbound->cipher);
    CACHE.staticKey = outbound->static_key;

    auto tls = outbound->tls;
    ui->tls_server_name->setText(tls->server_name);
    ui->tls_server_name_type->setCurrentText(tls->server_name_type);
    ui->tls_certificate_path->setText(tls->certificate_path);
    ui->tls_client_certificate_path->setText(tls->client_certificate_path);
    ui->tls_client_key_path->setText(tls->client_key_path);
    ui->control_wrap_key_path->setText(tls->control_wrap->key_path);
    ui->tls_crl_path->setText(tls->crl_path);
    ui->tls_remote_certificate_ku->setText(tls->remote_certificate_ku.join(","));
    ui->tls_remote_certificate_eku->setText(tls->remote_certificate_eku);
    ui->tls_remote_certificate_tls->setCurrentText(tls->remote_certificate_tls);
    ui->tls_certificate_profile->setCurrentText(tls->certificate_profile);
    ui->tls_ns_certificate_type->setCurrentText(tls->ns_certificate_type);
    ui->tls_version_min->setCurrentText(tls->version_min);
    ui->tls_version_max->setCurrentText(tls->version_max);
    ui->tls_cipher->setText(tls->cipher);
    ui->tls_groups->setText(tls->groups);
    CACHE.peerFingerprint = tls->peer_fingerprint;

    ui->data_ciphers->setText(outbound->data_ciphers.join(","));
    ui->data_ciphers_fallback->setText(outbound->data_ciphers_fallback);
    ui->auth->setText(outbound->auth);
    ui->mss_fix->setText(EditorNumText(outbound->mss_fix));
    ui->mss_fix_disabled->setChecked(outbound->mss_fix_disabled);
    ui->mss_fix_mode->setCurrentText(outbound->mss_fix_mode);
    ui->fragment->setText(EditorNumText(outbound->fragment));
    ui->replay_window->setText(EditorNumText(outbound->replay_window));
    ui->replay_window_time->setText(outbound->replay_window_time);

    ui->compression->setCurrentText(outbound->compression);
    ui->compression_lzo->setCurrentText(outbound->compression_lzo);
    ui->allow_compression->setCurrentText(outbound->allow_compression);

    ui->route_no_pull->setChecked(outbound->route_no_pull);
    ui->routes->setText(outbound->routes.join(","));
    ui->route_gateway->setText(outbound->route_gateway);
    ui->route_metric->setText(EditorNumText(outbound->route_metric));
    ui->redirect_gateway->setChecked(outbound->redirect_gateway);
    ui->redirect_gateway_flags->setText(outbound->redirect_gateway_flags.join(","));
    ui->redirect_private->setChecked(outbound->redirect_private);
    ui->block_ipv6->setChecked(outbound->block_ipv6);
    for (const auto &filter: outbound->pull_filters) {
        addPullFilterRow(filter->action, filter->text);
    }

    ui->ping_interval->setText(outbound->ping_interval);
    ui->ping_restart->setText(outbound->ping_restart);
    ui->ping_restart_disabled->setChecked(outbound->ping_restart_disabled);
    ui->renegotiate_interval->setText(outbound->renegotiate_interval);
    ui->renegotiate_disabled->setChecked(outbound->renegotiate_disabled);
    ui->renegotiate_bytes->setText(EditorNumText(outbound->renegotiate_bytes));
    ui->renegotiate_packets->setText(EditorNumText(outbound->renegotiate_packets));
    ui->tls_timeout->setText(outbound->tls_timeout);
    ui->handshake_window->setText(outbound->handshake_window);
    ui->explicit_exit_notify->setText(EditorNumText(outbound->explicit_exit_notify));

    ui->static_key->setText(CACHE.staticKey.isEmpty() ? tr("Not set") : tr("Already set"));
    ui->tls_peer_fingerprint->setText(CACHE.peerFingerprint.isEmpty() ? tr("Not set") : tr("Already set"));

    connect(ui->static_key, &QPushButton::clicked, this,
            [=, this] { editPem(ui->static_key, tr("Static Key"), CACHE.staticKey); });
    connect(ui->tls_peer_fingerprint, &QPushButton::clicked, this,
            [=, this] { editPem(ui->tls_peer_fingerprint, tr("Peer Fingerprint"), CACHE.peerFingerprint); });

    connect(ui->servers_table_add, &QPushButton::clicked, this, [=, this] { addServerRow("", kOpenVPNDefaultPort, ""); });
    connect(ui->servers_table_remove, &QPushButton::clicked, this, [=, this] {
        ui->servers_table->removeRow(ui->servers_table->currentRow());
        form->revalidate(ui->servers_table);
    });
    connect(ui->pull_filters_table_add, &QPushButton::clicked, this, [=, this] { addPullFilterRow("", ""); });
    connect(ui->pull_filters_table_remove, &QPushButton::clicked, this, [=, this] {
        ui->pull_filters_table->removeRow(ui->pull_filters_table->currentRow());
        form->revalidate(ui->pull_filters_table);
    });
    // A committed cell edit is the tables' focus-out.
    connect(ui->servers_table, &QTableWidget::itemChanged, this, [this] { form->revalidate(ui->servers_table); });
    connect(ui->pull_filters_table, &QTableWidget::itemChanged, this, [this] { form->revalidate(ui->pull_filters_table); });

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

EditOpenVPNAdvanced::~EditOpenVPNAdvanced()
{
    delete ui;
}

void EditOpenVPNAdvanced::editPem(QPushButton *button, const QString &title, QStringList &target) {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, title, "", target.join("\n"), &ok);
    if (!ok) return;
    target = txt.split("\n", Qt::SkipEmptyParts);
    button->setText(target.isEmpty() ? tr("Not set") : tr("Already set"));
    form->revalidate(button);
}

void EditOpenVPNAdvanced::addServerRow(const QString &server, const QString &port, const QString &network) {
    auto row = ui->servers_table->rowCount();
    ui->servers_table->insertRow(row);
    ui->servers_table->setItem(row, 0, new QTableWidgetItem(server));
    ui->servers_table->setItem(row, 1, new QTableWidgetItem(port));
    EditorSetComboCell(ui->servers_table, row, 2, Configs::openvpnNetworks, network);
}

void EditOpenVPNAdvanced::addPullFilterRow(const QString &action, const QString &text) {
    auto row = ui->pull_filters_table->rowCount();
    ui->pull_filters_table->insertRow(row);
    EditorSetComboCell(ui->pull_filters_table, row, 0, kPullFilterActions, action);
    ui->pull_filters_table->setItem(row, 1, new QTableWidgetItem(text));
}

void EditOpenVPNAdvanced::accept() {
    if (!form->validateAll()) return;
    saveInto(*ent->OpenVPN());
    QDialog::accept();
}

void EditOpenVPNAdvanced::saveInto(Configs::openvpn &outbound) const {
    const auto upper = [](const QStringList &names) {
        QStringList out;
        for (const auto &name: names) out << name.toUpper();
        return out;
    };

    outbound.mode = ui->mode->currentText().trimmed();
    outbound.topology = ui->topology->currentText().trimmed();
    outbound.auth_retry = ui->auth_retry->currentText().trimmed();
    outbound.remote_random = ui->remote_random->isChecked();
    outbound.address = SplitAndTrim(ui->address->text(), ",", false);
    outbound.peer_address = ui->peer_address->text().trimmed();
    outbound.peer_address_ipv6 = ui->peer_address_ipv6->text().trimmed();

    outbound.servers = {};
    for (int row = 0; row < ui->servers_table->rowCount(); ++row) {
        auto server = EditorCellText(ui->servers_table, row, 0);
        if (server.isEmpty()) continue;
        auto remote = std::make_shared<Configs::OpenVPNRemote>();
        remote->server = server;
        remote->server_port = EditorCellText(ui->servers_table, row, 1).toInt();
        remote->network = EditorComboCellText(ui->servers_table, row, 2);
        outbound.servers += remote;
    }

    outbound.static_key = CACHE.staticKey;
    outbound.static_key_path = ui->static_key_path->text().trimmed();
    outbound.key_direction = ui->key_direction->currentText().trimmed();
    outbound.cipher = ui->cipher->text().trimmed().toUpper();

    auto tls = outbound.tls;
    tls->server_name = ui->tls_server_name->text().trimmed();
    tls->server_name_type = ui->tls_server_name_type->currentText().trimmed();
    tls->certificate_path = ui->tls_certificate_path->text().trimmed();
    tls->client_certificate_path = ui->tls_client_certificate_path->text().trimmed();
    tls->client_key_path = ui->tls_client_key_path->text().trimmed();
    tls->control_wrap->key_path = ui->control_wrap_key_path->text().trimmed();
    tls->peer_fingerprint = {};
    for (const auto &line: CACHE.peerFingerprint) {
        if (const auto hex = Configs::Checks::OpenVPNNormalizeFingerprint(line); !hex.isEmpty()) tls->peer_fingerprint << hex;
    }
    tls->crl_path = ui->tls_crl_path->text().trimmed();
    tls->remote_certificate_ku = SplitAndTrim(ui->tls_remote_certificate_ku->text(), ",", false);
    tls->remote_certificate_eku = ui->tls_remote_certificate_eku->text().trimmed();
    tls->remote_certificate_tls = ui->tls_remote_certificate_tls->currentText().trimmed();
    tls->certificate_profile = ui->tls_certificate_profile->currentText().trimmed();
    tls->ns_certificate_type = ui->tls_ns_certificate_type->currentText().trimmed();
    tls->version_min = ui->tls_version_min->currentText().trimmed();
    tls->version_max = ui->tls_version_max->currentText().trimmed();
    tls->cipher = Configs::Checks::OpenVPNNormalizeTlsCipher(ui->tls_cipher->text());
    tls->groups = ui->tls_groups->text().trimmed().toUpper();

    outbound.data_ciphers = upper(SplitAndTrim(ui->data_ciphers->text(), ",", false));
    outbound.data_ciphers_fallback = ui->data_ciphers_fallback->text().trimmed().toUpper();
    outbound.auth = ui->auth->text().trimmed().toUpper();
    outbound.mss_fix = ui->mss_fix->text().trimmed().toInt();
    outbound.mss_fix_disabled = ui->mss_fix_disabled->isChecked();
    outbound.mss_fix_mode = ui->mss_fix_mode->currentText().trimmed();
    outbound.fragment = ui->fragment->text().trimmed().toInt();
    outbound.replay_window = ui->replay_window->text().trimmed().toInt();
    outbound.replay_window_time = ui->replay_window_time->text().trimmed();

    outbound.compression = ui->compression->currentText().trimmed();
    outbound.compression_lzo = ui->compression_lzo->currentText().trimmed();
    outbound.allow_compression = ui->allow_compression->currentText().trimmed();

    outbound.route_no_pull = ui->route_no_pull->isChecked();
    outbound.routes = SplitAndTrim(ui->routes->text(), ",", false);
    outbound.route_gateway = ui->route_gateway->text().trimmed();
    outbound.route_metric = ui->route_metric->text().trimmed().toInt();
    outbound.redirect_gateway = ui->redirect_gateway->isChecked();
    outbound.redirect_gateway_flags = SplitAndTrim(ui->redirect_gateway_flags->text(), ",", false);
    outbound.redirect_private = ui->redirect_private->isChecked();
    outbound.block_ipv6 = ui->block_ipv6->isChecked();

    outbound.pull_filters = {};
    for (int row = 0; row < ui->pull_filters_table->rowCount(); ++row) {
        auto text = EditorCellText(ui->pull_filters_table, row, 1);
        if (text.isEmpty()) continue;
        auto filter = std::make_shared<Configs::OpenVPNPullFilter>();
        filter->action = EditorComboCellText(ui->pull_filters_table, row, 0);
        filter->text = text;
        outbound.pull_filters += filter;
    }

    outbound.ping_interval = ui->ping_interval->text().trimmed();
    outbound.ping_restart = ui->ping_restart->text().trimmed();
    outbound.ping_restart_disabled = ui->ping_restart_disabled->isChecked();
    outbound.renegotiate_interval = ui->renegotiate_interval->text().trimmed();
    outbound.renegotiate_disabled = ui->renegotiate_disabled->isChecked();
    outbound.renegotiate_bytes = ui->renegotiate_bytes->text().trimmed().toLongLong();
    outbound.renegotiate_packets = ui->renegotiate_packets->text().trimmed().toLongLong();
    outbound.tls_timeout = ui->tls_timeout->text().trimmed();
    outbound.handshake_window = ui->handshake_window->text().trimmed();
    outbound.explicit_exit_notify = ui->explicit_exit_notify->text().trimmed().toInt();
}

std::shared_ptr<Configs::openvpn> EditOpenVPNAdvanced::withInput() const {
    auto outbound = ent->OpenVPN()->DetachedCopy();
    if (pageInput) pageInput(*outbound);
    saveInto(*outbound);
    return outbound;
}

void EditOpenVPNAdvanced::bindValidation() {
    using namespace Validation;
    using namespace Configs::Checks;
    using Check = std::function<QString(const Configs::openvpn &)>;

    form->bind(ui->address, Rules::List(Split::Comma, Rules::Cidr()));
    form->bind(ui->peer_address, Rules::Ip(Family::V4, Empty::Allowed));
    form->bind(ui->peer_address_ipv6, Rules::Ip(Family::V6, Empty::Allowed));
    form->bind(ui->cipher, OpenVPNCipherName);
    form->bind(ui->tls_remote_certificate_ku, Rules::List(Split::Comma, OpenVPNKeyUsage));
    form->bind(ui->tls_remote_certificate_eku, OpenVPNExtendedKeyUsage);
    form->bind(ui->tls_cipher, OpenVPNTlsCipher);
    form->bind(ui->tls_groups, OpenVPNTlsGroups);
    form->bind(ui->data_ciphers, Rules::List(Split::Comma, OpenVPNCipherName));
    form->bind(ui->data_ciphers_fallback, OpenVPNCipherName);
    form->bind(ui->auth, OpenVPNDigest);
    form->bind(ui->mss_fix, Rules::Int(0, 65535));
    form->bind(ui->fragment, OpenVPNFragment);
    form->bind(ui->replay_window, OpenVPNReplayWindow);
    form->bind(ui->replay_window_time, OpenVPNReplayWindowTime);
    form->bind(ui->routes, Rules::List(Split::Comma, Rules::Cidr()));
    form->bind(ui->route_gateway, Rules::Ip(Family::Any, Empty::Allowed));
    form->bind(ui->route_metric, Rules::Int(0, INT_MAX));
    form->bind(ui->ping_interval, OpenVPNPingInterval);
    form->bind(ui->ping_restart, OpenVPNPingInterval);
    form->bind(ui->renegotiate_interval, OpenVPNInterval);
    form->bind(ui->renegotiate_bytes, Rules::Int(0, LLONG_MAX));
    form->bind(ui->renegotiate_packets, Rules::Int(0, LLONG_MAX));
    form->bind(ui->tls_timeout, OpenVPNInterval);
    form->bind(ui->handshake_window, OpenVPNInterval);
    form->bind(ui->explicit_exit_notify, Rules::Int(0, INT_MAX));

    form->addRule({ui->tls_peer_fingerprint}, [this] {
        return EachItem(CACHE.peerFingerprint.join('\n'), Split::Newline, OpenVPNFingerprint);
    }, this);
    form->addRule({ui->servers_table}, [this] {
        for (int row = 0; row < ui->servers_table->rowCount(); ++row) {
            const auto host = EditorCellText(ui->servers_table, row, 0);
            if (host.isEmpty()) continue;
            if (auto message = OpenVPNRemoteHost(host); !message.isEmpty()) return tr("Row %1, host: %2").arg(QString::number(row + 1), message);
            const auto port = EditorCellText(ui->servers_table, row, 1);
            if (auto message = OpenVPNRemotePort(port); !message.isEmpty()) return tr("Row %1, port: %2").arg(QString::number(row + 1), message);
        }
        return QString();
    }, this, Severity::Error, tr("Servers"));
    form->addRule({ui->servers_table}, [this] {
        for (int row = 0; row < ui->servers_table->rowCount(); ++row) {
            if (EditorCellText(ui->servers_table, row, 0).isEmpty()) return tr("Row %1 has no host, so it is not saved.").arg(row + 1);
        }
        return QString();
    }, this, Severity::Warning, tr("Servers"));
    form->addRule({ui->pull_filters_table}, [this] {
        for (int row = 0; row < ui->pull_filters_table->rowCount(); ++row) {
            if (EditorCellText(ui->pull_filters_table, row, 1).isEmpty()) return tr("Row %1 has no text, so it is not saved.").arg(row + 1);
        }
        return QString();
    }, this, Severity::Warning, tr("Pull Filters"));

    const auto rule = [this](const QList<QWidget *> &participants, const Check &check, Severity severity = Severity::Error) {
        form->addRule(participants, [this, check] { return check(*withInput()); }, this, severity);
    };

    // In each pair the first widget is the one the mode makes unusable.
    const QList<QPair<QWidget *, QString>> modeBound = {
        {ui->static_key, "static_key"},
        {ui->static_key_path, "static_key_path"},
        {ui->key_direction, "key_direction"},
        {ui->cipher, "cipher"},
        {ui->auth_retry, "auth_retry"},
        {ui->tls_server_name, "tls.server_name"},
        {ui->tls_server_name_type, "tls.server_name_type"},
        {ui->tls_certificate_path, "tls.certificate_path"},
        {ui->tls_client_certificate_path, "tls.client_certificate_path"},
        {ui->tls_client_key_path, "tls.client_key_path"},
        {ui->tls_peer_fingerprint, "tls.peer_fingerprint"},
        {ui->tls_crl_path, "tls.crl_path"},
        {ui->tls_remote_certificate_ku, "tls.remote_certificate_ku"},
        {ui->tls_remote_certificate_eku, "tls.remote_certificate_eku"},
        {ui->tls_remote_certificate_tls, "tls.remote_certificate_tls"},
        {ui->tls_certificate_profile, "tls.certificate_profile"},
        {ui->tls_ns_certificate_type, "tls.ns_certificate_type"},
        {ui->tls_version_min, "tls.version_min"},
        {ui->tls_version_max, "tls.version_max"},
        {ui->tls_cipher, "tls.cipher"},
        {ui->tls_groups, "tls.groups"},
        {ui->data_ciphers, "data_ciphers"},
        {ui->data_ciphers_fallback, "data_ciphers_fallback"},
        {ui->route_no_pull, "route_no_pull"},
        {ui->pull_filters_table, "pull_filters"},
        {ui->renegotiate_interval, "renegotiate_interval"},
        {ui->renegotiate_disabled, "renegotiate_disabled"},
        {ui->renegotiate_bytes, "renegotiate_bytes"},
        {ui->renegotiate_packets, "renegotiate_packets"},
        {ui->tls_timeout, "tls_timeout"},
        {ui->handshake_window, "handshake_window"},
    };
    for (const auto &bound: modeBound) {
        rule({bound.first, ui->mode}, [key = bound.second](const Configs::openvpn &o) { return OpenVPNModeConflict(o, key); });
    }
    // The control channel wrap is switched on by its type on the profile page; only a key path set here is in the way.
    rule({ui->control_wrap_key_path, ui->mode}, [](const Configs::openvpn &o) {
        return o.tls->control_wrap->key_path.isEmpty() ? QString() : OpenVPNModeConflict(o, "tls.control_wrap");
    });
    rule({ui->mode}, [](const Configs::openvpn &o) {
        static const QStringList pageKeys = {"username", "password", "static_challenge", "static_challenge_echo",
                                             "tls.certificate", "tls.client_certificate", "tls.client_key", "tls.control_wrap"};
        for (const auto &key: OpenVPNModeConflicts(o)) {
            if (o.mode == "static_key" && pageKeys.contains(key)) {
                return tr("Static key mode does not use the credentials, certificates or control channel wrap on the profile page; clear them there.");
            }
        }
        return QString();
    }, Severity::Warning);

    rule({ui->static_key, ui->static_key_path, ui->mode}, OpenVPNStaticKeyMissing);
    rule({ui->static_key, ui->static_key_path}, [](const Configs::openvpn &o) { return VpnMaterialConflict(o.static_key, o.static_key_path); });
    rule({ui->cipher, ui->mode}, OpenVPNStaticCipher);
    rule({ui->address, ui->peer_address, ui->peer_address_ipv6, ui->mode}, OpenVPNStaticAddress);

    // The pasted counterparts live on the profile page, so the conflict is fixed there as well.
    rule({ui->tls_certificate_path}, [](const Configs::openvpn &o) {
        return VpnMaterialConflict(o.tls->certificate, o.tls->certificate_path);
    }, Severity::Warning);
    rule({ui->tls_client_certificate_path}, [](const Configs::openvpn &o) {
        return VpnMaterialConflict(o.tls->client_certificate, o.tls->client_certificate_path);
    }, Severity::Warning);
    rule({ui->tls_client_key_path}, [](const Configs::openvpn &o) {
        return VpnMaterialConflict(o.tls->client_key, o.tls->client_key_path);
    }, Severity::Warning);
    rule({ui->control_wrap_key_path}, [](const Configs::openvpn &o) {
        return VpnMaterialConflict(o.tls->control_wrap->key, o.tls->control_wrap->key_path);
    }, Severity::Warning);

    rule({ui->tls_remote_certificate_eku, ui->tls_remote_certificate_tls}, OpenVPNRemoteCertificate);
    rule({ui->tls_version_max, ui->tls_version_min}, OpenVPNTlsVersions);
    rule({ui->mss_fix, ui->mss_fix_disabled, ui->mss_fix_mode}, OpenVPNMssFix);
    rule({ui->ping_restart, ui->ping_restart_disabled}, OpenVPNPingRestart);
    rule({ui->renegotiate_interval, ui->renegotiate_disabled}, OpenVPNRenegotiate);
}
