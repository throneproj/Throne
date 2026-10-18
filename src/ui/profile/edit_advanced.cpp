#include "include/ui/profile/edit_advanced.h"

#include "include/ui/utils/GuiUtils.hpp"

#include <QGuiApplication>
#include <QInputDialog>
#include <QNetworkInterface>
#include <QScreen>
#include <QStandardItemModel>
#include <QAbstractSocket>
#include "include/configs/generate.h"
#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/IpListsRepo.h"
#include "include/ui/profile/editor_table_utils.h"
#include "include/ui/utils/ValidationForm.h"

#include <climits>

EditAdvanced::InterfaceFields EditAdvanced::GetInterfaceFields() const {
    if (auto *openvpn = ent->OpenVPN(); openvpn != nullptr) {
        return {&openvpn->system, &openvpn->interface_name, &openvpn->udp_timeout,
                &openvpn->udp_mapping, &openvpn->udp_filtering, &openvpn->udp_nat_max};
    }
    if (auto *openconnect = ent->OpenConnect(); openconnect != nullptr) {
        return {&openconnect->system, &openconnect->interface_name, &openconnect->udp_timeout,
                &openconnect->udp_mapping, &openconnect->udp_filtering, &openconnect->udp_nat_max};
    }
    return {};
}

EditAdvanced::EditAdvanced(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent)
    : QDialog(parent)
    , ui(new Ui::EditAdvanced)
{
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    ent = _ent;
    bindValidation();
    auto dialFieldsObj = ent->outbound->dialFields;
    ui->reuse_addr->setChecked(dialFieldsObj->reuse_addr);
    ui->tcp_fast_open->setChecked(dialFieldsObj->tcp_fast_open);
    ui->udp_fragment->setChecked(dialFieldsObj->udp_fragment);
    ui->tcp_multipath->setChecked(dialFieldsObj->tcp_multi_path);
    ui->connect_timeout->setText(dialFieldsObj->connect_timeout);

    for (const auto& ifc : QNetworkInterface::allInterfaces())
        m_systemInterfaces << ifc.humanReadableName();
    for (const auto& addr : QNetworkInterface::allAddresses()) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol)
            m_systemIpv4Addresses << addr.toString();
        else if (addr.protocol() == QAbstractSocket::IPv6Protocol)
            m_systemIpv6Addresses << addr.toString();
    }

    auto populateBindCombo = [](QComboBox* combo, const QStringList& systemItems,
                                const QStringList& history, const QString& current) {
        combo->addItem("");
        combo->addItems(systemItems);
        for (const auto& h : history) {
            if (!systemItems.contains(h))
                combo->addItem(h);
        }
        combo->setCurrentText(current);
    };

    auto* repo = Configs::dataManager->settingsRepo.get();
    populateBindCombo(ui->bind_interface,    m_systemInterfaces,    repo->dial_bind_interface_history,    dialFieldsObj->bind_interface);
    populateBindCombo(ui->inet4_bind_address, m_systemIpv4Addresses, repo->dial_inet4_bind_address_history, dialFieldsObj->inet4_bind_address);
    populateBindCombo(ui->inet6_bind_address, m_systemIpv6Addresses, repo->dial_inet6_bind_address_history, dialFieldsObj->inet6_bind_address);

    if (ent->outbound->HasTLS()) {
        auto tlsObj = ent->outbound->GetTLS();
        ui->disable_sni->setChecked(tlsObj->disable_sni);
        ui->min_version->setText(tlsObj->min_version);
        ui->max_version->setText(tlsObj->max_version);
        ui->tls_spoof_state->setCurrentIndex(tlsObj->getSpoofState());
        ui->tls_spoof->setText(tlsObj->spoof);
        // a non-editable combo drops setCurrentText while it holds no items
        ui->tls_spoof_method->addItems(Configs::tlsSpoofMethods);
        ui->tls_spoof_method->setCurrentText(tlsObj->spoof_method);
        auto syncSpoofFields = [this] {
            const bool on = ui->tls_spoof_state->currentIndex() != 2;
            ui->tls_spoof->setEnabled(on);
            ui->tls_spoof_l->setEnabled(on);
            ui->tls_spoof_method->setEnabled(on && !ui->tls_spoof->text().isEmpty());
            ui->tls_spoof_method_l->setEnabled(on && !ui->tls_spoof->text().isEmpty());
        };
        connect(ui->tls_spoof, &QLineEdit::textChanged, this, syncSpoofFields);
        connect(ui->tls_spoof_state, &QComboBox::currentIndexChanged, this, syncSpoofFields);
        syncSpoofFields();
        ui->enable_ech->setChecked(tlsObj->ech->enabled);
        ui->ech_server_name->setText(tlsObj->ech->QueryTarget());

        CACHE.echConfig = tlsObj->ech->config;
        CACHE.certSha256 = tlsObj->certificate_sha256;
        CACHE.certPublicKeySha256 = tlsObj->certificate_public_key_sha256;
        CACHE.clientCert = tlsObj->client_certificate;
        CACHE.clientKey = tlsObj->client_key;
        setCacheButtonText(ui->ech_config, CACHE.echConfig);
        setCacheButtonText(ui->cert_sha256, CACHE.certSha256);
        setCacheButtonText(ui->cert_public_key_sha256, CACHE.certPublicKeySha256);
        setCacheButtonText(ui->client_cert, CACHE.clientCert);
        setCacheButtonText(ui->client_key, CACHE.clientKey);
    } else {
        ui->tls_box->hide();
    }

    if (ent->outbound->HasQUIC()) {
        auto quicObj = ent->outbound->GetQUIC();
        ui->quic_idle_timeout->setText(quicObj->idle_timeout);
        ui->quic_keep_alive_period->setText(quicObj->keep_alive_period);
        ui->quic_stream_receive_window->setText(quicObj->stream_receive_window);
        ui->quic_connection_receive_window->setText(quicObj->connection_receive_window);
        ui->quic_max_concurrent_streams->setText(EditorNumText(quicObj->max_concurrent_streams));
        ui->quic_initial_packet_size->setText(EditorNumText(quicObj->initial_packet_size));
        ui->quic_disable_path_mtu_discovery->setCurrentIndex(quicObj->getPathMtuState());
    } else {
        ui->quic_box->hide();
    }

    if (auto fields = GetInterfaceFields(); fields.system != nullptr) {
        ui->udp_mapping->addItems({"", "endpoint_independent", "address_dependent", "address_and_port_dependent"});
        ui->udp_filtering->addItems({"", "endpoint_independent", "address_dependent", "address_and_port_dependent"});
        ui->system->setChecked(*fields.system);
        ui->interface_name->setText(*fields.interface_name);
        ui->udp_timeout->setText(*fields.udp_timeout);
        ui->udp_mapping->setCurrentText(*fields.udp_mapping);
        ui->udp_filtering->setCurrentText(*fields.udp_filtering);
        ui->udp_nat_max->setText(EditorNumText(*fields.udp_nat_max));
    } else {
        ui->interface_box->hide();
    }

    if (Configs::EndpointOverrideBlocker(ent).isEmpty()) {
        loadEndpoint();
    } else {
        ui->endpoint_box->hide();
    }

    ADD_ASTERISK(this)
    form->markAll();

    // adjustSize() clamps to 2/3 of the screen.
    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    if (scr != nullptr) resize(sizeHint().boundedTo(scr->availableGeometry().size()));
}

EditAdvanced::~EditAdvanced()
{
    delete form;
    form = nullptr;
    delete ui;
}

void EditAdvanced::bindValidation() {
    using Validation::Severity;
    namespace Rules = Validation::Rules;
    namespace Checks = Configs::Checks;

    form->bind(ui->connect_timeout, Rules::Duration());
    form->bind(ui->inet4_bind_address, Rules::Ip(Validation::Family::V4, Validation::Empty::Allowed));
    form->bind(ui->inet6_bind_address, Checks::BindAddress6);
    form->addRule({ui->tcp_fast_open}, [this] {
        if (!ui->tcp_fast_open->isChecked() || ent->type != "anytls") return QString();
        return tr("AnyTLS does not support TCP Fast Open, so it is turned off when the profile starts.");
    }, nullptr, Severity::Warning);
    form->addRule({ui->tcp_fast_open}, [this] {
        // outbound::Build drops TFO for the dialer-level fragment, which the core cannot combine with it.
        if (!ui->tcp_fast_open->isChecked() || !Configs::CustomFragmentApplies(*ent->outbound)) return QString();
        return tr("TCP Fast Open is ignored while the custom TLS fragment is on.");
    }, nullptr, Severity::Warning);

    form->bind(ui->min_version, Checks::TlsVersion);
    form->bind(ui->max_version, Checks::TlsVersion);
    form->addRule({ui->min_version, ui->max_version}, [this] {
        return Checks::TlsVersionOrder(ui->min_version->text(), ui->max_version->text());
    });
    form->bind(ui->tls_spoof, Rules::Host(Validation::Empty::Allowed)).warning();
    form->bind(ui->ech_server_name, [this](const QString &text) -> QString {
        Configs::ECH target;
        target.SetQueryTarget(text);
        if (!target.serverName.isEmpty()) {
            if (auto message = Validation::Host(target.serverName); !message.isEmpty()) return message;
        }
        if (target.resolver.isEmpty()) return {};
        // generate.cpp falls back to the default resolver for anything it cannot use.
        Configs::DnsAddress resolver;
        QString why;
        if (!Configs::ParseDnsAddress(target.resolver, &resolver, &why)) return why;
        if (resolver.type == "dhcp") return tr("A dhcp:// resolver cannot fetch the ECH config. Use a udp://, tls:// or https:// address.");
        return {};
    }).warning();
    form->addRule({ui->ech_config, ui->enable_ech}, [this] {
        return ui->enable_ech->isChecked() ? Checks::EchConfig(CACHE.echConfig) : QString();
    });
    form->addRule({ui->cert_sha256}, [this] { return Checks::CertificatePins(CACHE.certSha256); });
    form->addRule({ui->cert_public_key_sha256}, [this] { return Checks::CertificatePins(CACHE.certPublicKeySha256); });
    form->addRule({ui->client_cert}, [this] { return Checks::PemCertificates(CACHE.clientCert); });
    form->addRule({ui->client_key}, [this] { return Checks::PemPrivateKey(CACHE.clientKey); });
    form->addRule({ui->client_cert, ui->client_key}, [this] {
        const auto tls = ent->outbound->GetTLS();
        return Checks::ClientCertificatePair(!CACHE.clientCert.isEmpty() || !tls->client_certificate_path.isEmpty(),
                                             !CACHE.clientKey.isEmpty() || !tls->client_key_path.isEmpty());
    });

    for (auto *edit: {ui->quic_idle_timeout, ui->quic_keep_alive_period, ui->udp_timeout}) form->bind(edit, Rules::Duration());
    for (auto *edit: {ui->quic_stream_receive_window, ui->quic_connection_receive_window}) form->bind(edit, Rules::MemoryBytes(true));
    form->bind(ui->quic_max_concurrent_streams, Rules::Int(0, INT_MAX));
    form->bind(ui->quic_initial_packet_size, Checks::QuicInitialPacketSize);
    form->bind(ui->udp_nat_max, Rules::Int(0, INT_MAX));
}

void EditAdvanced::loadEndpoint() {
    using Mode = Configs::EndpointSource::Mode;
    ui->endpoint_mode->addItem(tr("Inherit from group"), static_cast<int>(Mode::Inherit));
    ui->endpoint_mode->addItem(tr("Own address"), static_cast<int>(Mode::Own));
    ui->endpoint_mode->addItem(tr("IP list"), static_cast<int>(Mode::IpList));

    const auto &source = ent->endpoint;
    for (const auto &list : Configs::dataManager->ipListsRepo->GetAllIpLists()) {
        ui->endpoint_list->addItem(list->name, list->id);
    }
    if (source.mode == Mode::IpList) {
        // A gone list stays selected, so saving never points the profile at another list by itself.
        if (ui->endpoint_list->findData(source.ipListId) < 0) ui->endpoint_list->insertItem(0, tr("Missing list"), source.ipListId);
        ui->endpoint_list->setCurrentIndex(ui->endpoint_list->findData(source.ipListId));
    }
    if (auto *model = qobject_cast<QStandardItemModel *>(ui->endpoint_mode->model()); model != nullptr && ui->endpoint_list->count() == 0) {
        model->item(ui->endpoint_mode->findData(static_cast<int>(Mode::IpList)))->setEnabled(false);
    }
    ui->endpoint_mode->setCurrentIndex(qMax(0, ui->endpoint_mode->findData(static_cast<int>(source.mode))));

    connect(ui->endpoint_mode, &QComboBox::currentIndexChanged, this, [this] { syncEndpoint(); });
    connect(ui->endpoint_list, &QComboBox::currentIndexChanged, this, [this] { syncEndpoint(); });
    syncEndpoint();
}

Configs::EndpointSource EditAdvanced::endpointFromUi() const {
    Configs::EndpointSource source;
    source.mode = static_cast<Configs::EndpointSource::Mode>(ui->endpoint_mode->currentData().toInt());
    if (source.mode == Configs::EndpointSource::Mode::IpList) source.ipListId = ui->endpoint_list->currentData().toInt();
    return source;
}

void EditAdvanced::syncEndpoint() {
    using Mode = Configs::EndpointSource::Mode;
    const auto source = endpointFromUi();
    ui->endpoint_list->setVisible(source.mode == Mode::IpList);

    Configs::EndpointResolution resolution;
    if (source.mode == Mode::IpList) {
        resolution = Configs::ResolveEndpointSource(source);
    } else if (source.mode == Mode::Inherit) {
        if (const auto group = Configs::dataManager->groupsRepo->GetGroup(ent->gid)) resolution = Configs::ResolveEndpointSource(group->endpoint);
    }
    QString hint;
    if (!resolution.address.isEmpty()) {
        hint = tr("Connects to %1 (%2).").arg(resolution.address, resolution.origin);
    } else if (!resolution.problem.isEmpty()) {
        hint = tr("%1, so the profile's own address is used.").arg(resolution.problem);
    }
    ui->endpoint_hint->setText(hint);
    ui->endpoint_hint->setHidden(hint.isEmpty());
}

void EditAdvanced::accept() {
    if (!form->validateAll()) return;
    auto dialFieldsObj = ent->outbound->dialFields;
    dialFieldsObj->reuse_addr = ui->reuse_addr->isChecked();
    dialFieldsObj->tcp_fast_open = ui->tcp_fast_open->isChecked();
    dialFieldsObj->udp_fragment = ui->udp_fragment->isChecked();
    dialFieldsObj->tcp_multi_path = ui->tcp_multipath->isChecked();
    dialFieldsObj->connect_timeout = ui->connect_timeout->text().trimmed();
    dialFieldsObj->bind_interface = ui->bind_interface->currentText().trimmed();
    dialFieldsObj->inet4_bind_address = ui->inet4_bind_address->currentText().trimmed();
    dialFieldsObj->inet6_bind_address = ui->inet6_bind_address->currentText().trimmed();

    auto updateHistory = [](QStringList& history, const QStringList& systemItems, const QString& value) {
        if (value.isEmpty() || systemItems.contains(value)) return;
        history.removeAll(value);
        history.prepend(value);
        if (history.size() > 5) history = history.mid(0, 5);
    };

    auto* repo = Configs::dataManager->settingsRepo.get();
    updateHistory(repo->dial_bind_interface_history,    m_systemInterfaces,    dialFieldsObj->bind_interface);
    updateHistory(repo->dial_inet4_bind_address_history, m_systemIpv4Addresses, dialFieldsObj->inet4_bind_address);
    updateHistory(repo->dial_inet6_bind_address_history, m_systemIpv6Addresses, dialFieldsObj->inet6_bind_address);
    repo->Save();

    if (ent->outbound->HasTLS()) {
        auto tlsObj = ent->outbound->GetTLS();
        tlsObj->disable_sni = ui->disable_sni->isChecked();
        tlsObj->min_version = ui->min_version->text().trimmed();
        tlsObj->max_version = ui->max_version->text().trimmed();
        tlsObj->saveSpoofState(ui->tls_spoof_state->currentIndex());
        tlsObj->spoof = ui->tls_spoof->text().trimmed();
        tlsObj->spoof_method = ui->tls_spoof_method->currentText().trimmed();
        tlsObj->ech->enabled = ui->enable_ech->isChecked();
        tlsObj->ech->SetQueryTarget(ui->ech_server_name->text());
        tlsObj->ech->config = CACHE.echConfig;
        tlsObj->client_certificate = CACHE.clientCert;
        tlsObj->client_key = CACHE.clientKey;
        tlsObj->certificate_sha256 = CACHE.certSha256;
        tlsObj->certificate_public_key_sha256 = CACHE.certPublicKeySha256;
    }

    if (ent->outbound->HasQUIC()) {
        auto quicObj = ent->outbound->GetQUIC();
        quicObj->idle_timeout = ui->quic_idle_timeout->text().trimmed();
        quicObj->keep_alive_period = ui->quic_keep_alive_period->text().trimmed();
        quicObj->stream_receive_window = ui->quic_stream_receive_window->text().trimmed();
        quicObj->connection_receive_window = ui->quic_connection_receive_window->text().trimmed();
        quicObj->max_concurrent_streams = ui->quic_max_concurrent_streams->text().trimmed().toInt();
        quicObj->initial_packet_size = ui->quic_initial_packet_size->text().trimmed().toInt();
        quicObj->savePathMtuState(ui->quic_disable_path_mtu_discovery->currentIndex());
    }

    if (auto fields = GetInterfaceFields(); fields.system != nullptr) {
        *fields.system = ui->system->isChecked();
        *fields.interface_name = ui->interface_name->text().trimmed();
        *fields.udp_timeout = ui->udp_timeout->text().trimmed();
        *fields.udp_mapping = ui->udp_mapping->currentText().trimmed();
        *fields.udp_filtering = ui->udp_filtering->currentText().trimmed();
        *fields.udp_nat_max = ui->udp_nat_max->text().trimmed().toInt();
    }

    if (!ui->endpoint_box->isHidden()) ent->endpoint = endpointFromUi();
    QDialog::accept();
}

void EditAdvanced::setCacheButtonText(QPushButton *button, const QStringList &value) {
    button->setText(value.isEmpty() ? tr("Not Set") : tr("Already set"));
}

void EditAdvanced::editCachedList(QPushButton *button, const QString &title, QStringList &target) {
    bool ok;
    const auto txt = QInputDialog::getMultiLineText(this, title, "", target.join("\n"), &ok);
    if (!ok) return;
    target = txt.split("\n", Qt::SkipEmptyParts);
    setCacheButtonText(button, target);
    form->revalidate(button);
}

void EditAdvanced::on_ech_config_clicked() {
    editCachedList(ui->ech_config, tr("ECH Config"), CACHE.echConfig);
    CACHE.echConfig = Configs::ECH::NormalizeConfig(CACHE.echConfig);
    setCacheButtonText(ui->ech_config, CACHE.echConfig);
    form->revalidate(ui->ech_config);
}

void EditAdvanced::on_client_cert_clicked() {
    editCachedList(ui->client_cert, tr("Client Certificate"), CACHE.clientCert);
}

void EditAdvanced::on_client_key_clicked() {
    editCachedList(ui->client_key, tr("Client Key"), CACHE.clientKey);
}

void EditAdvanced::on_cert_sha256_clicked() {
    editCachedList(ui->cert_sha256, tr("Certificate SHA256"), CACHE.certSha256);
    CACHE.certSha256 = Configs::CertificatePinsFromHex(CACHE.certSha256);
    setCacheButtonText(ui->cert_sha256, CACHE.certSha256);
    form->revalidate(ui->cert_sha256);
}

void EditAdvanced::on_cert_public_key_sha256_clicked() {
    editCachedList(ui->cert_public_key_sha256, tr("Public Key SHA256"), CACHE.certPublicKeySha256);
    CACHE.certPublicKeySha256 = Configs::CertificatePinsFromHex(CACHE.certPublicKeySha256);
    setCacheButtonText(ui->cert_public_key_sha256, CACHE.certPublicKeySha256);
    form->revalidate(ui->cert_public_key_sha256);
}
