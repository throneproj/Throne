#include "include/ui/profile/edit_dnstt.h"

#include "include/configs/common/DnsTunnel.h"
#include "include/global/Utils.hpp"
#include "include/ui/utils/NativeFileDialog.h"
#include "include/ui/utils/ValidationForm.h"

#include <QDir>
#include <QRegularExpressionValidator>

#include <algorithm>
#include <cmath>

namespace {
    enum EditDnsttMode { kEditDnsttClassic, kEditDnsttVayDns, kEditDnsttVayDnsCompat };

    void editDnsttRowVisible(QFormLayout *form, QWidget *field, bool visible) {
        field->setVisible(visible);
        if (auto *label = form->labelForField(field)) label->setVisible(visible);
    }

    void editDnsttSelect(QComboBox *combo, const QString &value) {
        int index = combo->findData(value);
        if (index < 0) {
            combo->addItem(value, value);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    }

    QString editDnsttNumber(int value) {
        return value > 0 ? QString::number(value) : QString();
    }

    Validation::DurationRule editDnsttDuration() {
        Validation::DurationRule rule;
        rule.minNs = 1'000'000;
        return rule;
    }
}

EditDnstt::EditDnstt(QWidget *parent) : QWidget(parent), ui(new Ui::EditDnstt) {
    ui->setupUi(this);

    ui->mode->addItem(QStringLiteral("DNSTT"));
    ui->mode->addItem(QStringLiteral("VayDNS"));
    ui->mode->addItem(tr("VayDNS (dnstt-compatible)"));
    for (const auto &type: Configs::dnsttRecordTypes) {
        ui->record_type->addItem(type.isEmpty() ? tr("Default") : type.toUpper(), type);
    }
    for (const auto &strategy: Configs::dnsttResolverStrategies) {
        ui->resolver_strategy->addItem(strategy.isEmpty() ? tr("Default") : strategy, strategy);
    }
    ui->backend->addItem(tr("None (chain only)"), QString());
    ui->backend->addItem(QStringLiteral("SOCKS5"), QStringLiteral("socks"));
    ui->backend->addItem(QStringLiteral("Shadowsocks"), QStringLiteral("shadowsocks"));
    ui->backend->addItem(QStringLiteral("SSH"), QStringLiteral("ssh"));
    ui->ss_method->addItems(Configs::shadowsocksMethods);
    ui->resolvers->setFlavor(Configs::ResolverFlavor::Dnstt);
    ui->ssh_private_key->setVisibleLines(3);
    ui->ssh_host_key->setVisibleLines(3);

    auto *digits = new QRegularExpressionValidator(QRegularExpression("^[0-9]*$"), this);
    ui->client_id_size->setValidator(digits);
    ui->max_qname_len->setValidator(digits);
    ui->rps->setValidator(new QRegularExpressionValidator(QRegularExpression("^([0-9]+(\\.[0-9]*)?)?$"), this));
    ui->client_id_size->setPlaceholderText(QStringLiteral("2"));
    const auto payloadTip = tr("Tunnel data each query carries; a longer domain leaves less room.");
    ui->payload_l->setToolTip(payloadTip);
    ui->payload->setToolTip(payloadTip);

    const auto relayout = [this] {
        updateControls();
        if (editor_state_changed) editor_state_changed();
    };
    connect(ui->mode, &QComboBox::currentIndexChanged, this, relayout);
    connect(ui->mode, &QComboBox::currentIndexChanged, this, [this] {
        if (set_edit_title) set_edit_title(Configs::DnsttDisplayType(mode()));
    });
    connect(ui->backend, &QComboBox::currentIndexChanged, this, relayout);
    for (auto *edit: {ui->domain, ui->max_qname_len, ui->client_id_size}) {
        connect(edit, &QLineEdit::textChanged, this, &EditDnstt::updatePayload);
    }
    connect(ui->ssh_choose_pk, &QPushButton::clicked, this, [this] {
        const auto fn = NativeFileDialog::getOpenFileName(this, QObject::tr("Select"), QDir::currentPath(),
                                                          "", nullptr, QFileDialog::Option::ReadOnly);
        if (fn.isEmpty()) return;
        ui->ssh_private_key_path->setText(fn);
        if (form) form->revalidate(ui->ssh_private_key_path);
    });
    updateControls();
}

EditDnstt::~EditDnstt() {
    delete ui;
}

QString EditDnstt::mode() const {
    return ui->mode->currentIndex() == kEditDnsttClassic ? QStringLiteral("dnstt") : QStringLiteral("vaydns");
}

bool EditDnstt::compat() const {
    return ui->mode->currentIndex() == kEditDnsttVayDnsCompat;
}

bool EditDnstt::vayNative() const {
    return ui->mode->currentIndex() == kEditDnsttVayDns;
}

QString EditDnstt::backend() const {
    return ui->backend->currentData().toString();
}

void EditDnstt::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Dnstt();

    const bool vay = outbound->mode == QLatin1String("vaydns");
    ui->mode->setCurrentIndex(!vay ? kEditDnsttClassic : outbound->dnstt_compat ? kEditDnsttVayDnsCompat : kEditDnsttVayDns);
    ui->domain->setText(outbound->domain);
    ui->public_key->setText(outbound->public_key);
    editDnsttSelect(ui->record_type, outbound->record_type);
    ui->client_id_size->setText(editDnsttNumber(outbound->client_id_size));
    ui->max_qname_len->setText(editDnsttNumber(outbound->max_qname_len));
    ui->resolvers->load(outbound->resolvers);
    ui->resolvers->setProfileId(ent->id);
    editDnsttSelect(ui->resolver_strategy, outbound->resolver_strategy);
    ui->tcp_fallback->setChecked(outbound->tcp_fallback);
    ui->idle_timeout->setText(outbound->idle_timeout);
    ui->keepalive_interval->setText(outbound->keepalive_interval);
    ui->rps->setText(outbound->rps > 0 ? QString::number(outbound->rps) : QString());

    editDnsttSelect(ui->backend, outbound->backend);
    ui->socks_username->setText(outbound->socks_username);
    ui->socks_password->setText(outbound->socks_password);
    ui->socks_udp_over_tcp->setChecked(outbound->socks_udp_over_tcp);
    ui->ss_method->setCurrentText(outbound->ss_method);
    ui->ss_password->setText(outbound->ss_password);
    ui->ss_udp_over_tcp->setChecked(outbound->ss_udp_over_tcp);
    ui->ssh_user->setText(outbound->ssh_user);
    ui->ssh_password->setText(outbound->ssh_password);
    ui->ssh_private_key->setPlainText(outbound->ssh_private_key);
    ui->ssh_private_key_path->setText(outbound->ssh_private_key_path);
    ui->ssh_private_key_pass->setText(outbound->ssh_private_key_passphrase);
    ui->ssh_host_key->setPlainText(outbound->ssh_host_key.join("\n"));
    ui->ssh_host_key_algs->setText(outbound->ssh_host_key_algorithms.join(","));
    ui->ssh_client_version->setText(outbound->ssh_client_version);
    updateControls();
}

bool EditDnstt::onEnd() {
    auto outbound = this->ent->Dnstt();

    const bool native = vayNative();
    outbound->mode = mode();
    outbound->dnstt_compat = compat();
    outbound->domain = ui->domain->text().trimmed();
    outbound->public_key = ui->public_key->text().trimmed();
    outbound->record_type = native ? ui->record_type->currentData().toString() : QString();
    outbound->client_id_size = native ? ui->client_id_size->text().toInt() : 0;
    outbound->max_qname_len = outbound->mode == QLatin1String("vaydns") ? ui->max_qname_len->text().toInt() : 0;
    outbound->resolvers = ui->resolvers->save();
    outbound->resolver_strategy = ui->resolver_strategy->currentData().toString();
    outbound->tcp_fallback = ui->tcp_fallback->isChecked();
    outbound->idle_timeout = ui->idle_timeout->text().trimmed();
    outbound->keepalive_interval = ui->keepalive_interval->text().trimmed();
    auto rps = ui->rps->text().trimmed();
    if (rps.endsWith(u'.')) rps.chop(1);
    bool ok = false;
    const double rate = rps.toDouble(&ok);
    outbound->rps = ok && std::isfinite(rate) && rate > 0 ? rate : 0;

    outbound->backend = backend();
    outbound->socks_username = ui->socks_username->text().trimmed();
    outbound->socks_password = ui->socks_password->text();
    outbound->socks_udp_over_tcp = ui->socks_udp_over_tcp->isChecked();
    outbound->ss_method = ui->ss_method->currentText().trimmed();
    outbound->ss_password = ui->ss_password->text();
    outbound->ss_udp_over_tcp = ui->ss_udp_over_tcp->isChecked();
    outbound->ssh_user = ui->ssh_user->text().trimmed();
    outbound->ssh_password = ui->ssh_password->text();
    outbound->ssh_private_key = ui->ssh_private_key->toPlainText();
    outbound->ssh_private_key_path = ui->ssh_private_key_path->text().trimmed();
    outbound->ssh_private_key_passphrase = ui->ssh_private_key_pass->text();
    outbound->ssh_host_key = SplitAndTrim(ui->ssh_host_key->toPlainText(), "\n", false);
    outbound->ssh_host_key_algorithms = SplitAndTrim(ui->ssh_host_key_algs->text(), ",", false);
    outbound->ssh_client_version = ui->ssh_client_version->text().trimmed();
    return true;
}

void EditDnstt::bindValidation(Validation::Form &form) {
    using namespace Validation;
    using namespace Configs::Checks;
    this->form = &form;

    form.bind(ui->domain, DnsTunnelDomain, this);
    form.bind(ui->public_key, DnsttPublicKey, this);
    form.bind(ui->client_id_size, Rules::Int(0, 253), this);
    form.bind(ui->max_qname_len, Rules::Int(0, 253), this);
    form.addRule({ui->record_type}, [this] {
        return DnsttRecordType(mode(), compat(), ui->record_type->currentData().toString());
    }, this);
    form.addRule({ui->domain, ui->max_qname_len, ui->client_id_size, ui->mode}, [this] {
        const auto domain = ui->domain->text().trimmed();
        if (!DnsTunnelDomain(domain).isEmpty()) return QString();
        return DnsttMtuError(mode(), compat(), domain, ui->max_qname_len->text().toInt(), ui->client_id_size->text().toInt());
    }, this);

    ui->resolvers->bindValidation(form, this);
    form.addRule({ui->resolver_strategy}, [this] {
        return OneOf(ui->resolver_strategy->currentData().toString(), Configs::dnsttResolverStrategies);
    }, this);

    form.bind(ui->idle_timeout, Rules::Duration(editDnsttDuration()), this);
    form.bind(ui->keepalive_interval, Rules::Duration(editDnsttDuration()), this);
    form.addRule({ui->keepalive_interval, ui->idle_timeout}, [this] {
        return DnsttKeepalive(mode(), compat(), ui->idle_timeout->text(), ui->keepalive_interval->text());
    }, this);

    form.addRule({ui->backend}, [this] { return OneOf(backend(), Configs::dnsttBackends); }, this);
    form.addRule({ui->backend}, [this] { return DnsttChainOnly(backend()); }, this, Severity::Warning);

    form.bind(ui->ss_method, Rules::OneOf(Configs::shadowsocksMethods, Empty::Required), this);
    form.addRule({ui->ss_password, ui->ss_method}, [this] {
        return ShadowsocksPassword(ui->ss_method->currentText().trimmed(), ui->ss_password->text());
    }, this);

    form.bindLines(ui->ssh_host_key, SshHostKey, this);
    form.bind(ui->ssh_client_version, SshClientVersion, this).warning();
    form.addRule({ui->ssh_private_key}, [this] { return SshPrivateKey(ui->ssh_private_key->toPlainText()); }, this);
    form.addRule({ui->ssh_password, ui->ssh_private_key, ui->ssh_private_key_path}, [this] {
        return SshAuthentication(ui->ssh_password->text(), ui->ssh_private_key->toPlainText(), ui->ssh_private_key_path->text());
    }, this, Severity::Warning);
    form.addRule({ui->ssh_private_key, ui->ssh_private_key_path}, [this] {
        return SshKeySources(ui->ssh_private_key->toPlainText(), ui->ssh_private_key_path->text());
    }, this, Severity::Warning);
}

void EditDnstt::updateControls() {
    const bool vay = mode() == QLatin1String("vaydns");
    const bool native = vayNative();
    editDnsttRowVisible(ui->tunnel_form, ui->record_type, native);
    editDnsttRowVisible(ui->tunnel_form, ui->client_id_size, native);
    editDnsttRowVisible(ui->tunnel_form, ui->max_qname_len, vay);
    ui->max_qname_len->setPlaceholderText(native ? QStringLiteral("101") : QStringLiteral("253"));
    ui->idle_timeout->setPlaceholderText(native ? QStringLiteral("10s") : QStringLiteral("2m"));
    ui->keepalive_interval->setPlaceholderText(native ? QStringLiteral("2s") : QStringLiteral("10s"));

    const auto type = backend();
    for (auto *field: {static_cast<QWidget *>(ui->socks_username), static_cast<QWidget *>(ui->socks_password),
                       static_cast<QWidget *>(ui->socks_udp_over_tcp)}) {
        editDnsttRowVisible(ui->backend_form, field, type == QLatin1String("socks"));
    }
    for (auto *field: {static_cast<QWidget *>(ui->ss_method), static_cast<QWidget *>(ui->ss_password),
                       static_cast<QWidget *>(ui->ss_udp_over_tcp)}) {
        editDnsttRowVisible(ui->backend_form, field, type == QLatin1String("shadowsocks"));
    }
    for (auto *field: {static_cast<QWidget *>(ui->ssh_user), static_cast<QWidget *>(ui->ssh_password),
                       static_cast<QWidget *>(ui->ssh_private_key), static_cast<QWidget *>(ui->ssh_private_key_path_w),
                       static_cast<QWidget *>(ui->ssh_private_key_pass), static_cast<QWidget *>(ui->ssh_host_key),
                       static_cast<QWidget *>(ui->ssh_host_key_algs), static_cast<QWidget *>(ui->ssh_client_version)}) {
        editDnsttRowVisible(ui->backend_form, field, type == QLatin1String("ssh"));
    }
    updatePayload();
}

void EditDnstt::updatePayload() {
    const auto domain = ui->domain->text().trimmed();
    if (!Configs::Checks::DnsTunnelDomain(domain).isEmpty()) {
        ui->payload->clear();
        return;
    }
    const int qname = mode() == QLatin1String("vaydns") ? ui->max_qname_len->text().toInt() : 0;
    const int mtu = Configs::Checks::DnsttMtu(mode(), compat(), domain, qname, vayNative() ? ui->client_id_size->text().toInt() : 0);
    ui->payload->setText(tr("%1 bytes").arg(std::max(mtu, 0)));
}
