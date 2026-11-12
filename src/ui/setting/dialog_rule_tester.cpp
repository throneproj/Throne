#include "include/ui/setting/dialog_rule_tester.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QSpinBox>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

#include "include/api/RPC.h"
#include "include/configs/generate.h"
#include "include/database/DatabaseManager.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/RoutesRepo.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/ui/setting/ProcessPicker.h"

namespace {
    enum RuleTesterColumn { RuleTesterColRule = 0, RuleTesterColAction, RuleTesterColDetails, RuleTesterColCount };

    // QTreeWidget's own hint ignores its columns.
    class RuleTesterStepsTree : public QTreeWidget {
    public:
        using QTreeWidget::QTreeWidget;

        [[nodiscard]] QSize sizeHint() const override {
            const int ch = fontMetrics().averageCharWidth();
            int width = 2 * frameWidth() + verticalScrollBar()->sizeHint().width();
            for (int column = 0; column + 1 < columnCount(); column++) width += columnWidth(column);
            width += ch * 44;
            return {width, header()->sizeHint().height() + fontMetrics().height() * 12};
        }
    };

    QString ruleTesterText(const std::optional<std::string> &value) {
        return QString::fromStdString(value.value_or(""));
    }

    bool ruleTesterHasRawRules(const QList<Configs::RouteRuleOrigin> &origins) {
        return std::any_of(origins.cbegin(), origins.cend(),
                           [](const Configs::RouteRuleOrigin &origin) { return origin.kind == Configs::RouteRuleOriginKind::RawRule; });
    }

    // sing-box describes a route action as "route(<outbound>[,option...])".
    QString ruleTesterActionOutbound(const QString &action, const QString &detail) {
        const auto prefix = action + "(";
        if (!detail.startsWith(prefix) || !detail.endsWith(')')) return {};
        return detail.mid(prefix.size(), detail.size() - prefix.size() - 1).section(',', 0, 0).trimmed();
    }

    bool ruleTesterNotRunning(const QString &error) {
        return error.contains("not running", Qt::CaseInsensitive);
    }
}

DialogRuleTester::DialogRuleTester(Engine::ConnectionManager *connection, int preferredProfileId, QWidget *parent)
    : QDialog(parent), m_connection(connection), m_preferredProfileId(preferredProfileId)
{
    setWindowTitle(tr("Route path lookup"));

    auto *layout = new QVBoxLayout(this);

    m_scope = new QLabel(this);
    m_scope->setTextFormat(Qt::PlainText);
    m_scope->setWordWrap(true);
    layout->addWidget(m_scope);

    m_routeRow = new QWidget(this);
    auto *routeLayout = new QHBoxLayout(m_routeRow);
    routeLayout->setContentsMargins(0, 0, 0, 0);
    auto *routeLabel = new QLabel(tr("Routing profile"), m_routeRow);
    m_route = new QComboBox(m_routeRow);
    routeLabel->setBuddy(m_route);
    routeLayout->addWidget(routeLabel);
    routeLayout->addWidget(m_route, 1);
    layout->addWidget(m_routeRow);

    m_offlineHint = new QLabel(tr("Nothing is running, so the test builds the configuration this routing profile would start with. "
                                  "Outbounds are not dialed, endpoint rules are skipped and domains resolve with the system resolver."),
                               this);
    m_offlineHint->setWordWrap(true);
    layout->addWidget(m_offlineHint);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_process = new QLineEdit(this);
    m_process->setPlaceholderText(tr("chrome.exe or full path"));
    m_process->setClearButtonEnabled(true);
    auto *completer = new QCompleter(m_process);
    completer->setModel(new ProcessCompletionModel({.names = true, .paths = true}, completer));
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->popup()->setTextElideMode(Qt::ElideMiddle);
    m_process->setCompleter(completer);
    auto *pick = new QPushButton(tr("Pick…"), this);
    pick->setAutoDefault(false);
    pick->setToolTip(tr("Choose from running programs and the ones seen connecting, or browse for a file."));
    connect(pick, &QPushButton::clicked, this, &DialogRuleTester::pickProcess);
    auto *processRow = new QHBoxLayout();
    processRow->addWidget(m_process, 1);
    processRow->addWidget(pick);
    form->addRow(tr("Process"), processRow);

    m_domain = new QLineEdit(this);
    m_domain->setPlaceholderText(tr("example.com"));
    m_domain->setClearButtonEnabled(true);
    form->addRow(tr("Domain"), m_domain);

    m_ip = new QLineEdit(this);
    m_ip->setPlaceholderText(tr("1.1.1.1 or 2606:4700::1111"));
    m_ip->setClearButtonEnabled(true);
    form->addRow(tr("IP address"), m_ip);

    m_port = new QSpinBox(this);
    m_port->setRange(1, 65535);
    m_port->setValue(443);
    form->addRow(tr("Port"), m_port);

    m_network = new QComboBox(this);
    m_network->addItem(QStringLiteral("TCP"), QStringLiteral("tcp"));
    m_network->addItem(QStringLiteral("UDP"), QStringLiteral("udp"));
    form->addRow(tr("Network"), m_network);

    m_protocol = new QComboBox(this);
    m_protocol->addItem(tr("Auto (by port)"), QStringLiteral("auto"));
    m_protocol->addItem(tr("None"), QStringLiteral("none"));
    const QList<QPair<QString, QString>> sniffers{
        {"TLS", "tls"}, {"HTTP", "http"}, {"QUIC", "quic"}, {"DNS", "dns"}, {"STUN", "stun"}, {"BitTorrent", "bittorrent"},
        {"DTLS", "dtls"}, {"SSH", "ssh"}, {"RDP", "rdp"}, {"NTP", "ntp"},
    };
    for (const auto &[label, name] : sniffers) m_protocol->addItem(label, name);
    m_protocol->setToolTip(tr("What a sniff rule finds in the connection's first packets."));
    form->addRow(tr("Sniffed protocol"), m_protocol);
    layout->addLayout(form);

    auto *testRow = new QHBoxLayout();
    testRow->addStretch();
    m_test = new QPushButton(tr("Test"), this);
    m_test->setDefault(true);
    connect(m_test, &QPushButton::clicked, this, &DialogRuleTester::runTest);
    testRow->addWidget(m_test);
    layout->addLayout(testRow);

    m_headline = new QLabel(this);
    auto headlineFont = m_headline->font();
    headlineFont.setBold(true);
    m_headline->setFont(headlineFont);
    m_headline->setTextFormat(Qt::PlainText);
    m_headline->setWordWrap(true);
    m_headline->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_headline);

    m_summary = new QLabel(this);
    auto summaryFont = m_summary->font();
    if (summaryFont.pointSizeF() > 0) summaryFont.setPointSizeF(summaryFont.pointSizeF() * 0.9);
    m_summary->setFont(summaryFont);
    m_summary->setTextFormat(Qt::PlainText);
    m_summary->setWordWrap(true);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_summary);

    m_skipped = new QLabel(this);
    m_skipped->setTextFormat(Qt::PlainText);
    m_skipped->setWordWrap(true);
    layout->addWidget(m_skipped);

    m_steps = new RuleTesterStepsTree(this);
    m_steps->setColumnCount(RuleTesterColCount);
    m_steps->setHeaderLabels({tr("Rule"), tr("Action"), tr("Details")});
    m_steps->setRootIsDecorated(false);
    m_steps->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_steps->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_steps->setWordWrap(true);
    m_steps->setAlternatingRowColors(true);
    m_steps->header()->setStretchLastSection(true);
    m_steps->header()->setSectionResizeMode(QHeaderView::Interactive);
    const int ch = m_steps->fontMetrics().averageCharWidth();
    m_steps->setColumnWidth(RuleTesterColRule, ch * 30);
    m_steps->setColumnWidth(RuleTesterColAction, ch * 24);
    layout->addWidget(m_steps, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (m_connection != nullptr) {
        connect(m_connection, &Engine::ConnectionManager::statusChanged, this, [this] {
            if (!m_busy) updateHeader(isRunning());
        });
    }
    fillRouteCombo(Configs::dataManager->settingsRepo->current_route_id);
    updateHeader(isRunning());
    m_skipped->hide();

    const auto *screenOf = parentWidget() != nullptr ? parentWidget()->screen() : screen();
    resize(sizeHint().boundedTo(screenOf->availableGeometry().size()));
}

void DialogRuleTester::setPreferredProfile(int profileId) {
    m_preferredProfileId = profileId;
}

bool DialogRuleTester::isRunning() const {
    return m_connection != nullptr && m_connection->runningSnapshot() != nullptr;
}

void DialogRuleTester::updateHeader(bool running) {
    if (running) {
        const int routeId = Configs::RunningRouteId();
        const auto route = routeId >= 0 ? Configs::dataManager->routesRepo->GetRouteProfile(routeId) : nullptr;
        if (route != nullptr) {
            m_scope->setText(tr("Testing the running configuration, routing profile “%1”.").arg(route->name));
        } else if (routeId < 0) {
            m_scope->setText(tr("Testing the running configuration. It is a custom configuration, so rules are shown as sing-box describes them."));
        } else {
            m_scope->setText(tr("Testing the running configuration."));
        }
    } else {
        fillRouteCombo(m_route->currentData().isValid() ? m_route->currentData().toInt() : -1);
    }
    m_scope->setVisible(running);
    m_routeRow->setVisible(!running);
    m_offlineHint->setVisible(!running);
}

void DialogRuleTester::fillRouteCombo(int selectRouteId) {
    QList<QPair<int, QString>> routes;
    for (const auto &route : Configs::dataManager->routesRepo->GetAllRouteProfiles()) {
        if (route != nullptr) routes.append({route->id, route->name});
    }
    bool same = routes.size() == m_route->count();
    for (int i = 0; same && i < routes.size(); i++) same = m_route->itemData(i).toInt() == routes[i].first && m_route->itemText(i) == routes[i].second;
    // Rebuilding would close an open popup on every status change.
    if (same && m_route->currentData().toInt() == selectRouteId) return;
    if (!same) {
        m_route->clear();
        for (const auto &[id, name] : routes) m_route->addItem(name, id);
    }
    int index = m_route->findData(selectRouteId);
    if (index < 0) index = m_route->findData(Configs::dataManager->settingsRepo->current_route_id);
    m_route->setCurrentIndex(std::max(index, 0));
}

void DialogRuleTester::pickProcess() {
    if (const auto picked = ProcessPicker::ChooseProgram(this); !picked.isEmpty()) m_process->setText(picked);
}

bool DialogRuleTester::buildRequest(libcore::TestRouteRequest &request, QString *error) const {
    auto process = m_process->text().trimmed();
    if (process.size() >= 2 && process.startsWith('"') && process.endsWith('"')) process = process.mid(1, process.size() - 2).trimmed();

    auto domain = m_domain->text().trimmed();
    if (domain.contains("://")) domain = QUrl(domain).host();
    while (domain.endsWith('.')) domain.chop(1);

    auto ip = m_ip->text().trimmed();
    if (ip.startsWith('[') && ip.endsWith(']')) ip = ip.mid(1, ip.size() - 2);

    if (process.isEmpty() && domain.isEmpty() && ip.isEmpty()) {
        *error = tr("Enter a process, a domain or an IP address.");
        return false;
    }
    if (!domain.isEmpty()) {
        if (QHostAddress probe; probe.setAddress(domain)) {
            *error = tr("“%1” is an IP address: enter it under IP address.").arg(domain);
            return false;
        }
        const auto ace = QString::fromLatin1(QUrl::toAce(domain)).toLower();
        if (ace.isEmpty()) {
            *error = tr("“%1” is not a valid domain.").arg(domain);
            return false;
        }
        domain = ace;
    }
    if (!ip.isEmpty()) {
        QHostAddress address;
        if (!address.setAddress(ip)) {
            *error = tr("“%1” is not an IP address.").arg(ip);
            return false;
        }
        ip = address.toString();
    }

    request.process = process.toStdString();
    request.domain = domain.toStdString();
    request.ip = ip.toStdString();
    request.port = m_port->value();
    request.network = m_network->currentData().toString().toStdString();
    request.protocol = m_protocol->currentData().toString().toStdString();
    request.config = std::string();
    return true;
}

void DialogRuleTester::runTest() {
    if (m_busy) return;
    libcore::TestRouteRequest request;
    QString error;
    if (!buildRequest(request, &error)) {
        showMessage(error);
        return;
    }
    const bool running = isRunning();
    updateHeader(running);
    if (!running) {
        testOffline(request);
        return;
    }
    auto rules = Configs::GetRunningRouteRules();
    RuleNames names;
    names.origins = rules.origins;
    names.outboundNames = rules.outboundNames;
    names.vpnEndpointProfiles = rules.vpnEndpointProfiles;
    names.rawProfile = ruleTesterHasRawRules(names.origins);
    names.routeId = Configs::RunningRouteId();
    send(request, names, true);
}

void DialogRuleTester::testOffline(libcore::TestRouteRequest request) {
    const int routeId = m_route->currentData().isValid() ? m_route->currentData().toInt() : -1;
    if (routeId < 0) {
        showMessage(tr("Select a routing profile to test."));
        return;
    }
    const auto built = Configs::BuildRuleTestConfig(routeId, m_preferredProfileId);
    if (!built.error.isEmpty() || built.build == nullptr) {
        showMessage(tr("Could not build the configuration: %1").arg(built.error));
        return;
    }
    request.config = QJsonObject2QString(built.build->coreConfig, true).toStdString();
    RuleNames names;
    names.origins = built.build->routeRuleOrigins;
    names.outboundNames = built.build->outboundNames;
    names.vpnEndpointProfiles = built.build->vpnEndpointProfiles;
    names.rawProfile = ruleTesterHasRawRules(names.origins);
    names.routeId = routeId;
    names.builtWith = built.profileId;
    send(request, names, false);
}

void DialogRuleTester::send(const libcore::TestRouteRequest &request, const RuleNames &names, bool fallBackOffline) {
    setBusy(true);
    showMessage(tr("Testing…"));
    QPointer<DialogRuleTester> self(this);
    runOnNewThread([self, request, names, fallBackOffline] {
        bool rpcOK = false;
        QString coreError;
        const auto reply = API::defaultClient->TestRoute(&rpcOK, request, &coreError);
        runOnUiThread([self, request, names, fallBackOffline, rpcOK, coreError, reply] {
            if (self == nullptr) return;
            self->finish(request, names, fallBackOffline, rpcOK, coreError, reply);
        });
    });
}

void DialogRuleTester::finish(const libcore::TestRouteRequest &request, const RuleNames &names, bool fallBackOffline, bool rpcOK,
                              const QString &coreError, const libcore::TestRouteResponse &reply) {
    setBusy(false);
    const auto failure = !rpcOK ? (coreError.isEmpty() ? tr("the core did not answer") : coreError) : ruleTesterText(reply.error);
    if (failure.isEmpty()) {
        showResult(reply, names);
        return;
    }
    if (fallBackOffline && ruleTesterNotRunning(failure)) {
        fillRouteCombo(names.routeId >= 0 ? names.routeId : Configs::dataManager->settingsRepo->current_route_id);
        updateHeader(false);
        testOffline(request);
        return;
    }
    showMessage(tr("The test failed: %1").arg(failure));
}

void DialogRuleTester::showMessage(const QString &text) {
    m_headline->setText(text);
    m_summary->clear();
    m_summary->hide();
    m_skipped->clear();
    m_skipped->hide();
    m_steps->clear();
}

void DialogRuleTester::setBusy(bool busy) {
    m_busy = busy;
    m_test->setEnabled(!busy);
}

void DialogRuleTester::showResult(const libcore::TestRouteResponse &reply, const RuleNames &names) {
    const auto action = ruleTesterText(reply.action);
    const bool isFinal = reply.is_final.value_or(false);
    const auto *deciding = !isFinal && !reply.steps.empty() ? &reply.steps.back() : nullptr;
    const auto decidingLabel = deciding != nullptr ? ruleLabel(deciding->index.value_or(-1), ruleTesterText(deciding->rule), names) : QString();

    QString headline;
    if (action == "reject") {
        headline = decidingLabel.isEmpty() ? tr("Rejected") : tr("Rejected by %1").arg(decidingLabel);
    } else if (action == "hijack-dns") {
        headline = decidingLabel.isEmpty() ? tr("Handled as DNS") : tr("Handled as DNS by %1").arg(decidingLabel);
    } else if (action == "error") {
        auto error = !reply.steps.empty() ? ruleTesterText(reply.steps.back().error) : QString();
        if (error.isEmpty()) error = tr("unknown error");
        headline = tr("The connection would be closed: %1").arg(error);
    } else {
        auto outbound = outboundLabel(ruleTesterText(reply.outbound), names);
        for (const auto &pick : reply.outbound_chain) outbound += QStringLiteral(" → ") + outboundLabel(QString::fromStdString(pick), names);
        const auto decision = isFinal || decidingLabel.isEmpty() ? tr("No rule matched: the default outbound applies.")
                                                                 : tr("Matched %1").arg(decidingLabel);
        headline = outbound + "\n" + decision;
    }
    m_headline->setText(headline);

    QStringList summary;
    if (const auto inbound = ruleTesterText(reply.inbound); !inbound.isEmpty()) summary << tr("Inbound: %1").arg(inbound);
    // A process-only test has no address, which sing-box formats as ":port".
    if (const auto destination = ruleTesterText(reply.destination); !destination.isEmpty() && !destination.startsWith(':'))
        summary << tr("Destination: %1").arg(destination);
    if (const auto domain = ruleTesterText(reply.domain); !domain.isEmpty()) summary << tr("Domain: %1").arg(domain);
    if (!reply.addresses.empty()) {
        QStringList addresses;
        for (const auto &address : reply.addresses) addresses << QString::fromStdString(address);
        summary << tr("Addresses: %1").arg(addresses.join(", "));
    }
    if (names.builtWith >= 0) {
        if (const auto name = profileName(names.builtWith); !name.isEmpty()) summary << tr("Built with %1").arg(name);
    }
    m_summary->setText(summary.join(QStringLiteral("  ·  ")));
    m_summary->setVisible(!summary.isEmpty());

    if (!reply.skipped.empty()) {
        QStringList skipped;
        for (const auto index : reply.skipped) skipped << ruleLabel(index, {}, names);
        m_skipped->setText(tr("%n endpoint rule(s) were skipped: they need a running profile.", nullptr, static_cast<int>(reply.skipped.size())) +
                           "\n" + skipped.join(", "));
        m_skipped->show();
    } else {
        m_skipped->clear();
        m_skipped->hide();
    }

    m_steps->clear();
    for (size_t i = 0; i < reply.steps.size(); i++) {
        const auto &step = reply.steps[i];
        const int index = step.index.value_or(-1);
        const auto ruleText = ruleTesterText(step.rule);
        auto *item = new QTreeWidgetItem(m_steps);
        item->setText(RuleTesterColRule, ruleLabel(index, ruleText, names));
        item->setToolTip(RuleTesterColRule, ruleText.isEmpty() ? QStringLiteral("route.rules[%1]").arg(index)
                                                               : QStringLiteral("route.rules[%1]: %2").arg(index).arg(ruleText));
        item->setText(RuleTesterColAction, actionText(step, names));
        item->setToolTip(RuleTesterColAction, ruleTesterText(step.action_detail));
        const auto details = detailsText(step);
        item->setText(RuleTesterColDetails, details);
        item->setToolTip(RuleTesterColDetails, details);
        if (&step == deciding) {
            for (int column = 0; column < RuleTesterColCount; column++) {
                auto font = item->font(column);
                font.setBold(true);
                item->setFont(column, font);
            }
        }
    }
}

QString DialogRuleTester::profileName(int profileId) const {
    const auto ent = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (ent == nullptr) return {};
    return ent->outbound != nullptr ? ent->outbound->DisplayName() : ent->name;
}

QString DialogRuleTester::outboundLabel(const QString &tag, const RuleNames &names) const {
    if (tag.isEmpty()) return tr("Unknown outbound");
    if (tag == "direct") return tr("Direct");
    if (tag == "block") return tr("Block");
    if (tag == "l3-direct") return tr("Direct (L3 bridge)");
    if (const auto name = names.outboundNames.value(tag); !name.isEmpty()) return name;
    if (names.vpnEndpointProfiles.contains(tag)) {
        if (const auto name = profileName(names.vpnEndpointProfiles.value(tag)); !name.isEmpty()) return name;
    }
    return tag;
}

QString DialogRuleTester::originLabel(const Configs::RouteRuleOrigin &origin, const RuleNames &names) const {
    using Kind = Configs::RouteRuleOriginKind;
    switch (origin.kind) {
        case Kind::ProfileRule:
            return origin.name.isEmpty() ? tr("Rule %1").arg(origin.profileRule + 1) : tr("“%1”").arg(origin.name);
        case Kind::RawRule:
            return tr("Raw rule %1").arg(origin.profileRule + 1);
        case Kind::XrayBridge:
            return tr("Xray bridge (Throne)");
        case Kind::ExtraCore:
            return tr("Extra core goes direct (Throne)");
        case Kind::SystemDns:
            return tr("System DNS (Throne)");
        case Kind::Sniff:
            return tr("Sniff (Throne)");
        case Kind::Resolve:
            return tr("Domain resolve (Throne)");
        case Kind::DnsHijack:
            return tr("DNS hijack (Throne)");
        case Kind::DnsInboundReject:
            return tr("DNS inbound guard (Throne)");
        case Kind::AdBlock:
            return tr("AdBlock");
        case Kind::EndpointGate: {
            auto name = names.vpnEndpointProfiles.contains(origin.name) ? profileName(names.vpnEndpointProfiles.value(origin.name)) : QString();
            if (name.isEmpty()) name = outboundLabel(origin.name, names);
            return tr("Endpoint: %1").arg(name);
        }
        case Kind::L3Bridge: {
            if (origin.profileRule < 0) return tr("L3 bridge direct (Throne)");
            const auto source = names.rawProfile        ? tr("raw rule %1").arg(origin.profileRule + 1)
                                : origin.name.isEmpty() ? tr("rule %1").arg(origin.profileRule + 1)
                                                        : tr("“%1”").arg(origin.name);
            return tr("L3 bridge copy of %1").arg(source);
        }
        case Kind::TunnelGate:
            return tr("Tunnel gate (Throne)");
        case Kind::BlockFinal:
            return tr("Block the rest (Throne)");
    }
    return {};
}

QString DialogRuleTester::ruleLabel(int index, const QString &ruleText, const RuleNames &names) const {
    if (index >= 0 && index < names.origins.size()) return originLabel(names.origins[index], names);
    return ruleText.isEmpty() ? QStringLiteral("route.rules[%1]").arg(index) : ruleText;
}

QString DialogRuleTester::actionText(const libcore::TestRouteStep &step, const RuleNames &names) const {
    const auto action = ruleTesterText(step.action);
    const auto detail = ruleTesterText(step.action_detail);
    if (action == "route" || action == "bypass") {
        const auto tag = ruleTesterActionOutbound(action, detail);
        if (action == "route") return tag.isEmpty() ? tr("Route") : tr("Route to %1").arg(outboundLabel(tag, names));
        return tag.isEmpty() ? tr("Bypass") : tr("Bypass to %1").arg(outboundLabel(tag, names));
    }
    if (action == "route-options") return tr("Route options");
    if (action == "reject") return tr("Reject");
    if (action == "hijack-dns") return tr("Hijack DNS");
    if (action == "sniff") return tr("Sniff");
    if (action == "resolve") return tr("Resolve");
    return detail.isEmpty() ? action : detail;
}

QString DialogRuleTester::detailsText(const libcore::TestRouteStep &step) const {
    QStringList lines;
    if (const auto matched = ruleTesterText(step.matched); !matched.isEmpty()) lines << tr("Matched %1").arg(matched);
    if (ruleTesterText(step.action) == "sniff") {
        const auto protocol = ruleTesterText(step.protocol);
        const auto domain = ruleTesterText(step.sniffed_domain);
        if (protocol.isEmpty() && domain.isEmpty()) lines << tr("Found nothing");
        else if (domain.isEmpty()) lines << tr("Found %1, no domain").arg(protocol);
        else lines << tr("Found %1, domain %2").arg(protocol.isEmpty() ? tr("a protocol") : protocol, domain);
    }
    if (!step.addresses.empty()) {
        QStringList addresses;
        for (const auto &address : step.addresses) addresses << QString::fromStdString(address);
        lines << tr("Resolved to %1").arg(addresses.join(", "));
    }
    if (const auto error = ruleTesterText(step.error); !error.isEmpty()) lines << tr("Error: %1").arg(error);
    if (const auto note = ruleTesterText(step.note); !note.isEmpty()) lines << note;
    return lines.join("\n");
}
