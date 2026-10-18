#include "include/ui/setting/dialog_manage_routes.h"

#include <QClipboard>

#include "include/ui/utils/GuiUtils.hpp"

#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <QPointer>
#include <QShortcut>
#include <QTimer>
#include <QToolTip>
#include <QDialog>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QScreen>
#include <QGridLayout>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QDialogButtonBox>

#include <algorithm>
#include <climits>

#include "include/configs/generate.h"
#include "include/configs/sub/warp.h"
#include "include/configs/sub/RouteUpdater.hpp"

#include "include/database/RoutesRepo.h"
#include "include/ui/setting/RawRouteItem.h"

namespace {
    constexpr auto defaultWarpApiHost = "api.cloudflareclient.com";

    constexpr int manageRoutesDefaultCacheCapacity = 65536;

    // The same trimming as the core's warp normalizeHosts.
    QString normalizeWarpApiHost(const QString &line) {
        static const QRegularExpression pathStart("[/?#]");
        QString host = line.trimmed().toLower();
        if (host.startsWith("https://")) host.remove(0, 8);
        else if (host.startsWith("http://")) host.remove(0, 7);
        if (const auto end = host.indexOf(pathStart); end >= 0) host.truncate(end);
        return host;
    }

    QStringList normalizeWarpApiHosts(const QString &text) {
        QStringList hosts;
        for (const auto &line : text.split('\n')) {
            const auto host = normalizeWarpApiHost(line);
            if (!host.isEmpty() && !hosts.contains(host)) hosts << host;
        }
        return hosts;
    }

    // Names and notes from shared or fetched profiles must not render as rich text.
    void manageRoutesPlainBox(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text, const QString &details = {}) {
        QMessageBox box(icon, title, text, QMessageBox::Ok, parent);
        box.setTextFormat(Qt::PlainText);
        if (!details.isEmpty()) box.setDetailedText(details);
        box.exec();
    }

    QString manageRoutesDnsProblem(const QString &text) {
        Configs::DnsAddress parsed;
        QString why;
        return Configs::ParseDnsAddress(text, &parsed, &why) ? QString() : why;
    }

    // dns-local resolves the other servers' names, so it cannot depend on one itself.
    QString manageRoutesLocalDnsProblem(const QString &text) {
        if (text.isEmpty()) return {};
        Configs::DnsAddress parsed;
        QString why;
        if (!Configs::ParseDnsAddress(text, &parsed, &why)) return why;
        if (parsed.type == "local" || parsed.type == "dhcp" || Validation::Ip(parsed.server).isEmpty()) return {};
        return DialogManageRoutes::tr("Use an IP address, for example 223.5.5.5 or tls://1.1.1.1: this server resolves the names of the others.");
    }

    QString manageRoutesReservedProblem(const QString &text) {
        const auto items = Validation::Items(text, Validation::Split::Comma);
        if (items.isEmpty()) return {};
        const bool bytes = items.size() == 3 && std::all_of(items.begin(), items.end(), [](const QString &item) {
            return Validation::Int(item, 0, 255).isEmpty();
        });
        return bytes ? QString() : DialogManageRoutes::tr("Use three numbers from 0 to 255, for example 12,34,56.");
    }

    // The core takes interface addresses as prefixes only.
    QStringList manageRoutesWithPrefix(QStringList addresses) {
        for (auto &address : addresses) {
            // Unchecked while WARP is off, so only a real address gets a prefix.
            if (address.contains('/') || !Validation::Ip(address).isEmpty()) continue;
            address += Validation::Ip(address, Validation::Family::V4).isEmpty() ? "/32" : "/128";
        }
        return addresses;
    }

    // ParsePredefinedDNS's grammar: it hands the core the address as QHostAddress normalises it, zone dropped.
    QString manageRoutesPredefinedError(const QString &rawLine) {
        QString line = rawLine;
        if (const auto hash = line.indexOf('#'); hash != -1) line = line.left(hash);
        const auto fields = line.simplified().split(' ', Qt::SkipEmptyParts);
        if (fields.isEmpty()) return {};
        if (QHostAddress address; !address.setAddress(fields[0])) {
            const auto problem = Validation::Ip(fields[0]);
            return problem.isEmpty() ? DialogManageRoutes::tr("\"%1\" is not an IP address.").arg(fields[0]) : problem;
        }
        if (fields.size() < 2) return DialogManageRoutes::tr("Add at least one host name after the address, for example 10.0.0.5 nas.lan.");
        for (qsizetype i = 1; i < fields.size(); i++) {
            auto domain = fields[i];
            while (domain.endsWith('.')) domain.chop(1);
            if (domain.isEmpty()) return DialogManageRoutes::tr("\"%1\" is not a host name.").arg(fields[i]);
        }
        return {};
    }

    QString manageRoutesPredefinedWarning(const QString &text) {
        const QStringList lines = text.split('\n');
        for (qsizetype n = 0; n < lines.size(); n++) {
            if (!manageRoutesPredefinedError(lines[n]).isEmpty()) continue;
            QString line = lines[n];
            if (const auto hash = line.indexOf('#'); hash != -1) line = line.left(hash);
            const auto fields = line.simplified().split(' ', Qt::SkipEmptyParts);
            for (qsizetype i = 1; i < fields.size(); i++) {
                auto domain = fields[i];
                while (domain.endsWith('.')) domain.chop(1);
                if (const auto problem = Validation::Host(domain); !problem.isEmpty()) {
                    return DialogManageRoutes::tr("Line %1: \"%2\" never matches: %3").arg(QString::number(n + 1), domain, problem);
                }
            }
        }
        return {};
    }

    void editWarpApiHosts(QWidget *parent) {
        auto w = new QDialog(parent);
        w->setWindowTitle(DialogManageRoutes::tr("WARP Registration Domains"));
        w->setWindowModality(Qt::ApplicationModal);

        auto layout = new QVBoxLayout(w);

        auto hint = new QLabel(DialogManageRoutes::tr("One domain per line. They are tried in order and the first one that accepts the registration is used. "
                                                      "Leave empty to use %1.").arg(defaultWarpApiHost), w);
        hint->setWordWrap(true);
        layout->addWidget(hint);

        const auto &saved = Configs::dataManager->settingsRepo->warp_api_hosts;
        auto tEdit = new QPlainTextEdit(w);
        tEdit->setPlainText(saved.isEmpty() ? QString(defaultWarpApiHost) : saved.join('\n'));
        layout->addWidget(tEdit);

        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
        layout->addWidget(buttons);

        auto popupForm = new Validation::Form(w);
        popupForm->attachStatus(buttons);
        popupForm->bindLines(tEdit, [](const QString &line) {
            const auto host = normalizeWarpApiHost(line);
            if (host.isEmpty()) return QString();
            // The core requests "https://" + host, where a bare IPv6 address reads as host:port.
            if (host.count(':') > 1 && !host.startsWith('['))
                return DialogManageRoutes::tr("Put an IPv6 address in brackets, for example [2606:4700::1].");
            return Validation::HostPort(host, false);
        }).label(w->windowTitle());
        popupForm->markAll();

        QObject::connect(buttons, &QDialogButtonBox::accepted, w, [=] {
            if (!popupForm->validateAll()) return;
            Configs::dataManager->settingsRepo->warp_api_hosts = normalizeWarpApiHosts(tEdit->toPlainText());
            Configs::dataManager->settingsRepo->Save();
            w->accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

        w->resize(w->sizeHint().boundedTo(parent->screen()->availableGeometry().size()));
        w->exec();
        w->deleteLater();
    }
}

void DialogManageRoutes::reloadProfileItems() {
    if (chainList.empty()) {
        MessageBoxWarning(tr("Invalid state"), tr("The list of routing profiles is empty, this should be an unreachable state, crashes may occur now"));
        return;
    }

    QSignalBlocker blocker = QSignalBlocker(ui->route_prof); // currentIndexChanged during clear() crashes us
    ui->route_prof->clear();

    ui->route_profiles->clear();
    bool selectedChainGone = true;
    int i=0;
    for (const auto &item: chainList) {
        ui->route_prof->addItem(item->name);
        ui->route_profiles->addItem(item->name);
        if (item == currentRoute) {
            ui->route_prof->setCurrentIndex(i);
            selectedChainGone=false;
        }
        i++;
    }
    if (selectedChainGone) {
        currentRoute=chainList[0];
        ui->route_prof->setCurrentIndex(0);
    }
    blocker.unblock();
}

void DialogManageRoutes::show_predefined_dns_editor() {
    auto w = new QDialog(this);
    w->setWindowTitle(tr("Predefined DNS Answers"));
    w->setWindowModality(Qt::ApplicationModal);

    auto layout = new QGridLayout(w);

    auto enable = new QCheckBox(tr("Enable predefined answers"), w);
    enable->setChecked(predefined_dns_enabled);
    layout->addWidget(enable, 0, 0);

    auto tEdit = new QPlainTextEdit(w);
    tEdit->setPlaceholderText(tr("One entry per line, hosts-file syntax:\n\n"
                                 "127.0.0.1 localhost\n"
                                 "10.0.0.5 nas.lan files.lan\n"
                                 "::1 localhost6\n\n"
                                 "A domain listed with only one address family is answered "
                                 "NXDOMAIN for the other, so the override cannot be bypassed."));
    tEdit->setPlainText(predefined_dns_text);
    tEdit->setEnabled(predefined_dns_enabled);
    layout->addWidget(tEdit, 1, 0);

    connect(enable, &QCheckBox::stateChanged, tEdit, [=](int state) {
        tEdit->setEnabled(state != Qt::Unchecked);
    });

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
    layout->addWidget(buttons, 2, 0);

    auto popupForm = new Validation::Form(w);
    popupForm->attachStatus(buttons);
    popupForm->bindLines(tEdit, manageRoutesPredefinedError).label(w->windowTitle());
    popupForm->bind(tEdit, manageRoutesPredefinedWarning).label(w->windowTitle()).warning().raw();
    popupForm->markAll();

    connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
        if (!popupForm->validateAll()) return;
        predefined_dns_enabled = enable->isChecked();
        predefined_dns_text = tEdit->toPlainText();
        w->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

    w->resize(w->sizeHint().expandedTo(QSize(520, 360)).boundedTo(screen()->availableGeometry().size()));
    w->exec();
    w->deleteLater();
}

void DialogManageRoutes::show_dns_advanced_editor() {
    auto w = new QDialog(this);
    w->setWindowTitle(tr("Advanced DNS Settings"));
    w->setWindowModality(Qt::ApplicationModal);

    auto layout = new QFormLayout(w);

    auto cacheCap = new QLineEdit(Int2String(dns_advanced.cache_capacity), w);
    cacheCap->setValidator(QRegExpValidator_Number);
    cacheCap->setPlaceholderText(tr("%1 (default)").arg(manageRoutesDefaultCacheCapacity));
    layout->addRow(tr("Cache Capacity"), cacheCap);

    auto queryTimeout = new QLineEdit(dns_advanced.query_timeout, w);
    queryTimeout->setPlaceholderText(tr("10s (default)"));
    layout->addRow(tr("Query Timeout"), queryTimeout);

    auto optimistic = new QCheckBox(tr("Optimistic Cache"), w);
    optimistic->setToolTip(tr("<html><head/><body><p>Keep serving an expired answer while it is "
                              "refreshed in the background. Cannot be combined with Disable Cache "
                              "or Disable Expire.</p></body></html>"));
    optimistic->setChecked(dns_advanced.optimistic);
    layout->addRow(optimistic);

    auto optimisticTimeout = new QLineEdit(dns_advanced.optimistic_timeout, w);
    optimisticTimeout->setPlaceholderText(tr("3d (default)"));
    layout->addRow(tr("Optimistic Timeout"), optimisticTimeout);

    auto disableCache = new QCheckBox(tr("Disable Cache"), w);
    disableCache->setChecked(dns_advanced.disable_cache);
    layout->addRow(disableCache);

    auto disableExpire = new QCheckBox(tr("Disable Expire"), w);
    disableExpire->setChecked(dns_advanced.disable_expire);
    layout->addRow(disableExpire);

    auto persistCache = new QCheckBox(tr("Save Cache To File"), w);
    persistCache->setToolTip(tr("<html><head/><body><p>Write cached DNS answers and FakeIP mappings to the core's cache "
                                "file so they survive a restart. Off by default: each entry costs a disk write.</p></body></html>"));
    persistCache->setChecked(dns_advanced.persist_cache);
    layout->addRow(persistCache);

    auto reverseMapping = new QCheckBox(tr("Reverse Mapping"), w);
    reverseMapping->setChecked(dns_advanced.reverse_mapping);
    layout->addRow(reverseMapping);

    // The core rejects optimistic alongside either cache switch, so the UI keeps them exclusive.
    auto syncConflicts = [=] {
        const bool cacheOff = disableCache->isChecked() || disableExpire->isChecked();
        if (cacheOff) optimistic->setChecked(false);
        optimistic->setEnabled(!cacheOff);
        optimisticTimeout->setEnabled(optimistic->isChecked());
        disableCache->setEnabled(!optimistic->isChecked());
        disableExpire->setEnabled(!optimistic->isChecked());
        // Disable Cache short-circuits the store path, so the file would never be written.
        persistCache->setEnabled(!disableCache->isChecked());
    };
    connect(optimistic, &QCheckBox::toggled, w, syncConflicts);
    connect(disableCache, &QCheckBox::toggled, w, syncConflicts);
    connect(disableExpire, &QCheckBox::toggled, w, syncConflicts);
    syncConflicts();

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
    layout->addRow(buttons);

    // Empty and 0 mean the default.
    auto popupForm = new Validation::Form(w);
    popupForm->attachStatus(buttons);
    popupForm->bind(cacheCap, Validation::Rules::Int(0, INT_MAX));
    popupForm->bind(cacheCap, [](const QString &text) {
        const auto value = Validation::ToInt(text);
        return value && *value >= 1 && *value < 1024 ? tr("The core raises values below 1024 to 1024.") : QString();
    }).warning();
    popupForm->bind(queryTimeout, Validation::Rules::Duration());
    popupForm->bind(optimisticTimeout, Validation::Rules::Duration());
    popupForm->markAll();

    connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
        if (!popupForm->validateAll()) return;
        const auto capacity = cacheCap->text().trimmed();
        dns_advanced.cache_capacity = capacity.toInt() == 0 ? manageRoutesDefaultCacheCapacity : capacity.toInt();
        dns_advanced.query_timeout = queryTimeout->text().trimmed();
        dns_advanced.optimistic = optimistic->isChecked();
        dns_advanced.optimistic_timeout = optimisticTimeout->text().trimmed();
        dns_advanced.disable_cache = disableCache->isChecked();
        dns_advanced.disable_expire = disableExpire->isChecked();
        dns_advanced.persist_cache = persistCache->isChecked();
        dns_advanced.reverse_mapping = reverseMapping->isChecked();
        w->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

    w->resize(w->sizeHint().boundedTo(screen()->availableGeometry().size()));
    w->exec();
    w->deleteLater();
}

void DialogManageRoutes::show_dns_object_editor() {
    auto w = new QDialog(this);
    w->setWindowTitle(tr("DNS Object"));
    w->setWindowModality(Qt::ApplicationModal);

    auto layout = new QGridLayout(w);

    auto tEdit = new QPlainTextEdit(w);
    tEdit->setPlaceholderText(DecodeB64IfValid("ewogICJzZXJ2ZXJzIjogW10sCiAgInJ1bGVzIjogW10sCiAgImZpbmFsIjogIiIsCiAgInN0cmF0ZWd5IjogIiIsCiAgImRpc2FibGVfY2FjaGUiOiBmYWxzZSwKICAiZGlzYWJsZV9leHBpcmUiOiBmYWxzZSwKICAiaW5kZXBlbmRlbnRfY2FjaGUiOiBmYWxzZSwKICAicmV2ZXJzZV9tYXBwaW5nIjogZmFsc2UsCiAgImZha2VpcCI6IHt9Cn0="));
    tEdit->setPlainText(dns_object_text);
    layout->addWidget(tEdit, 0, 0, 1, 2);

    auto format = new QPushButton(tr("Format"), w);
    layout->addWidget(format, 1, 0);
    connect(format, &QPushButton::clicked, w, [=] {
        auto obj = QString2QJsonObject(tEdit->toPlainText());
        if (obj.isEmpty()) {
            MessageBoxWarning(tr("Invalid input"), tr("The DNS object is not a valid JSON object"));
            return;
        }
        tEdit->setPlainText(QJsonObject2QString(obj, false));
    });

    auto document = new QPushButton(tr("Document"), w);
    layout->addWidget(document, 1, 1);
    connect(document, &QPushButton::clicked, w, [=] {
        MessageBoxInfo("DNS", "https://sing-box.sagernet.org/configuration/dns/");
    });

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
    layout->addWidget(buttons, 2, 0, 1, 2);

    auto popupForm = new Validation::Form(w);
    popupForm->attachStatus(buttons);
    popupForm->bind(tEdit, Validation::Rules::JsonObject()).label(w->windowTitle()).raw();
    popupForm->markAll();

    connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
        if (!popupForm->validateAll()) return;
        dns_object_text = tEdit->toPlainText();
        w->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

    w->resize(w->sizeHint().expandedTo(QSize(560, 420)).boundedTo(screen()->availableGeometry().size()));
    w->exec();
    w->deleteLater();
    form->revalidate(ui->use_dns_object);
}

DialogManageRoutes::DialogManageRoutes(QWidget *parent) : QDialog(parent), ui(new Ui::DialogManageRoutes) {
    ui->setupUi(this);
    form = new Validation::Form(this);
    form->attachStatus(ui->buttonBox);
    auto profiles =Configs::dataManager->routesRepo->GetAllRouteProfiles();
    for (const auto &item: profiles) {
        chainList << item;
    }
    if (chainList.empty()) {
        auto defaultChain = Configs::RouteProfile::GetDefaultChain();
        Configs::dataManager->routesRepo->AddRouteProfile(defaultChain);
        chainList.append(defaultChain);
    }
    currentRoute = Configs::dataManager->routesRepo->GetRouteProfile(Configs::dataManager->settingsRepo->current_route_id);
    if (currentRoute == nullptr) currentRoute = chainList[0];

    // Both strategy pickers must stay on one shared item order.
    ui->default_domain_strategy->addItems(Configs::DomainStrategy::DomainStrategy);
    ui->domainStrategyCombo->addItems(Configs::DomainStrategy::DomainStrategy);
    ui->local_override->setText(Configs::dataManager->settingsRepo->core_box_underlying_dns);
    ui->enable_fakeip->setChecked(Configs::dataManager->settingsRepo->fake_dns);
    ui->fakeip_disable_ipv6->setChecked(Configs::dataManager->settingsRepo->fakeip_disable_ipv6);
    ui->fakeip_disable_ipv6->setEnabled(Configs::dataManager->settingsRepo->fake_dns);
    connect(ui->enable_fakeip, &QCheckBox::toggled, ui->fakeip_disable_ipv6, &QCheckBox::setEnabled);

    dns_advanced = {
        Configs::dataManager->settingsRepo->dns_cache_capacity,
        Configs::dataManager->settingsRepo->dns_disable_cache,
        Configs::dataManager->settingsRepo->dns_disable_expire,
        Configs::dataManager->settingsRepo->dns_persist_cache,
        Configs::dataManager->settingsRepo->dns_reverse_mapping,
        Configs::dataManager->settingsRepo->dns_optimistic,
        Configs::dataManager->settingsRepo->dns_optimistic_timeout,
        Configs::dataManager->settingsRepo->dns_query_timeout,
    };
    connect(ui->dns_advanced, &QPushButton::clicked, this, &DialogManageRoutes::show_dns_advanced_editor);

    dns_object_text = Configs::dataManager->settingsRepo->dns_object;
    connect(ui->dns_object_edit, &QPushButton::clicked, this, &DialogManageRoutes::show_dns_object_editor);
    connect(ui->use_dns_object, &QCheckBox::stateChanged, this, [=,this](int state) {
        auto useDNSObject = state == Qt::Checked;
        ui->simple_dns_box->setDisabled(useDNSObject);
        ui->dns_advanced->setDisabled(useDNSObject);
        ui->dns_object_edit->setDisabled(!useDNSObject);
    });
    ui->use_dns_object->stateChanged(Qt::Unchecked);
    ui->ruleset_mirror->setCurrentIndex(Configs::dataManager->settingsRepo->ruleset_mirror);
    ui->default_domain_strategy->setCurrentText(Configs::dataManager->settingsRepo->default_domain_strategy);
    ui->domainStrategyCombo->setCurrentText(Configs::dataManager->settingsRepo->resolve_domain_strategy);
    ui->use_dns_object->setChecked(Configs::dataManager->settingsRepo->use_dns_object);
    ui->remote_dns->setCurrentText(Configs::dataManager->settingsRepo->remote_dns);
    ui->remote_dns_disable_ipv6->setChecked(Configs::dataManager->settingsRepo->remote_dns_disable_ipv6);
    ui->direct_dns->setCurrentText(Configs::dataManager->settingsRepo->direct_dns);
    ui->direct_dns_disable_ipv6->setChecked(Configs::dataManager->settingsRepo->direct_dns_disable_ipv6);
    ui->dns_final_out->setCurrentText(Configs::dataManager->settingsRepo->dns_final_out);
    ui->enable_dns_routing->setChecked(Configs::dataManager->settingsRepo->enable_dns_routing);
    ui->respect_hosts->setChecked(Configs::dataManager->settingsRepo->dns_use_hosts);
    predefined_dns_enabled = Configs::dataManager->settingsRepo->dns_predefined_enable;
    predefined_dns_text = Configs::dataManager->settingsRepo->dns_predefined_rules.join("\n");
    connect(ui->predefined_dns, &QPushButton::clicked, this, &DialogManageRoutes::show_predefined_dns_editor);
    reloadProfileItems();

    connect(ui->route_profiles, &QListWidget::itemDoubleClicked, this, [=,this](const QListWidgetItem* item){
        on_edit_route_clicked();
    });

    connect(ui->route_prof, &QComboBox::currentIndexChanged, this, &DialogManageRoutes::updateCurrentRouteProfile);

    deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);

    connect(deleteShortcut, &QShortcut::activated, this, [=,this]{
        on_delete_route_clicked();
    });

    // Scoped to the list, or these would hijack copy/paste in the dialog's text fields.
    auto exportShortcut = new QShortcut(QKeySequence::Copy, ui->route_profiles);
    exportShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(exportShortcut, &QShortcut::activated, this, [=,this]{
        on_export_route_clicked();
    });

    auto importShortcut = new QShortcut(QKeySequence::Paste, ui->route_profiles);
    importShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(importShortcut, &QShortcut::activated, this, [=,this]{
        on_import_route_clicked();
    });

    ui->enable_warp->setChecked(Configs::dataManager->settingsRepo->enable_warp);
    ui->warp_mode->setCurrentIndex(Configs::dataManager->settingsRepo->warp_mode == "masque" ? 1 : 0);
    ui->warp_private_key->setText(Configs::dataManager->settingsRepo->warp_private_key);
    ui->warp_public_key->setText(Configs::dataManager->settingsRepo->warp_public_key);
    ui->warp_ifc_addrs->setText(Configs::dataManager->settingsRepo->warp_ifc_addrs.join(","));
    ui->warp_ep->setText(Configs::dataManager->settingsRepo->warp_ep);
    ui->warp_reserved->setText(Configs::dataManager->settingsRepo->warp_reserved.join(","));
    ui->warp_masque_ep->setText(Configs::dataManager->settingsRepo->warp_masque_ep);
    ui->warp_masque_private_key->setText(Configs::dataManager->settingsRepo->warp_masque_private_key);
    ui->warp_masque_peer_public_key->setText(Configs::dataManager->settingsRepo->warp_masque_peer_public_key);
    ui->warp_masque_ifc_addrs->setText(Configs::dataManager->settingsRepo->warp_masque_ifc_addrs.join(","));
    ui->warp_masque_sni->setText(Configs::dataManager->settingsRepo->warp_masque_sni);
    ui->warp_masque_http_mode->setCurrentIndex(std::clamp(Configs::dataManager->settingsRepo->warp_masque_http_mode, 0, 2));
    auto showWarpMode = [this](int index) {
        ui->warp_wg_box->setVisible(index == 0);
        ui->warp_masque_box->setVisible(index == 1);
    };
    showWarpMode(ui->warp_mode->currentIndex());
    connect(ui->warp_mode, &QComboBox::currentIndexChanged, this, showWarpMode);
    connect(ui->warp_autogen, &QPushButton::clicked, this, &DialogManageRoutes::generate_warp_config);
    connect(ui->warp_api_hosts_btn, &QPushButton::clicked, this, [this] { editWarpApiHosts(this); });

    form->bind(ui->remote_dns, manageRoutesDnsProblem).label(ui->label_9->text());
    form->bind(ui->direct_dns, manageRoutesDnsProblem).label(ui->label_8->text());
    form->bind(ui->local_override, manageRoutesLocalDnsProblem).label(ui->label_2->text());
    // buildDNSSection would emit an unparsable object as "dns": {}, silently dropping every DNS setting.
    form->addRule({ui->use_dns_object}, [this] {
        if (!ui->use_dns_object->isChecked()) return QString();
        if (dns_object_text.trimmed().isEmpty())
            return tr("It is on but empty: click Edit DNS Object to write one, or turn it off.");
        if (auto problem = Validation::JsonObject(dns_object_text); !problem.isEmpty()) return problem;
        if (QString2QJsonObject(dns_object_text).isEmpty()) return tr("The object is empty: add at least its \"servers\", or turn it off.");
        return QString();
    }, nullptr, Validation::Severity::Error, tr("DNS Object"));

    // The fields the build refuses to start without while WARP is on; it ignores them all while WARP is off.
    const auto warpField = [this](Validation::Rule rule) -> Validation::Rule {
        return [this, rule = std::move(rule)](const QString &text) {
            if (text.isEmpty()) return tr("Required while WARP is on: click Generate Warp Config, or fill it in.");
            return rule ? rule(text) : QString();
        };
    };
    const auto warpOn = [this] { return ui->enable_warp->isChecked(); };
    const auto addresses = Validation::Rules::List(Validation::Split::Comma, Validation::Rules::Cidr(Validation::Family::Any, true));
    form->bind(ui->warp_ep, warpField(Validation::Rules::HostPort(false))).when(warpOn);
    form->bind(ui->warp_private_key, warpField(Validation::Rules::Base64Key(32))).when(warpOn);
    form->bind(ui->warp_public_key, warpField(Validation::Rules::Base64Key(32))).when(warpOn);
    form->bind(ui->warp_ifc_addrs, warpField(addresses)).when(warpOn);
    form->bind(ui->warp_reserved, manageRoutesReservedProblem).when(warpOn);
    form->bind(ui->warp_masque_ep, warpField(Validation::Rules::HostPort(false))).when(warpOn);
    form->bind(ui->warp_masque_private_key, warpField({})).when(warpOn);
    form->bind(ui->warp_masque_peer_public_key, warpField({})).when(warpOn);
    form->bind(ui->warp_masque_ifc_addrs, warpField(addresses)).when(warpOn);
    const QList<QWidget *> warpFields = {ui->warp_ep, ui->warp_private_key, ui->warp_public_key, ui->warp_ifc_addrs, ui->warp_reserved,
                                         ui->warp_masque_ep, ui->warp_masque_private_key, ui->warp_masque_peer_public_key, ui->warp_masque_ifc_addrs};
    const auto revalidateWarp = [this, warpFields] {
        for (auto *field : warpFields) form->revalidate(field);
    };
    connect(ui->enable_warp, &QCheckBox::toggled, form, revalidateWarp);
    connect(ui->warp_mode, &QComboBox::currentIndexChanged, form, revalidateWarp);
    form->markAll();

    ADD_ASTERISK(this)
    // Frozen .ui geometry clips this dialog once a translation outgrows it.
    resize(sizeHint().expandedTo(size()).boundedTo(screen()->availableGeometry().size()));
}

void DialogManageRoutes::generate_warp_config() {
    if (!Configs_network::ConfirmWarpTerms(this)) return;

    const bool masque = ui->warp_mode->currentIndex() == 1;
    const auto originalText = ui->warp_autogen->text();
    ui->warp_autogen->setEnabled(false);
    ui->warp_autogen->setText(tr("Generating config..."));

    QPointer<DialogManageRoutes> self(this);
    runOnNewThread([self, masque, originalText] {
        QString error;
        const auto conf = Configs_network::RegisterWarp(masque ? "masque" : "wireguard", &error);
        runOnUiThread([self, masque, originalText, conf, error] {
            if (self == nullptr) return;
            auto *dialog = self.data();
            if (!conf) {
                dialog->ui->warp_autogen->setText(originalText);
                dialog->ui->warp_autogen->setEnabled(true);
                MessageBoxWarning(tr("Failed to generate warp config"), error);
                return;
            }
            QStringList addrs;
            if (!conf->ipv4.isEmpty()) addrs << conf->ipv4 + "/32";
            if (!conf->ipv6.isEmpty()) addrs << conf->ipv6 + "/128";
            if (masque) {
                dialog->ui->warp_masque_private_key->setText(conf->privateKey);
                dialog->ui->warp_masque_peer_public_key->setText(conf->peerPublicKey);
                dialog->ui->warp_masque_ep->setText(conf->endpoint);
                dialog->ui->warp_masque_ifc_addrs->setText(addrs.join(","));
            } else {
                dialog->ui->warp_private_key->setText(conf->privateKey);
                dialog->ui->warp_public_key->setText(conf->peerPublicKey);
                dialog->ui->warp_ep->setText(conf->endpoint);
                dialog->ui->warp_ifc_addrs->setText(addrs.join(","));
                dialog->ui->warp_reserved->setText(QListInt2QListString(conf->reserved).join(","));
            }
            dialog->ui->warp_autogen->setText(tr("Success!"));
            setTimeout([dialog, originalText] {
                dialog->ui->warp_autogen->setText(originalText);
                dialog->ui->warp_autogen->setEnabled(true);
            }, dialog, 2000);
        });
    });
}

void DialogManageRoutes::updateCurrentRouteProfile(int idx) {
    currentRoute = chainList[idx];
}

DialogManageRoutes::~DialogManageRoutes() {
    delete ui;
}

void DialogManageRoutes::accept() {
    if (!form->validateAll()) return;
    if (chainList.empty()) {
        MessageBoxInfo(tr("Invalid settings"), tr("Routing profile cannot be empty"));
        return;
    }

    Configs::dataManager->settingsRepo->ruleset_mirror = ui->ruleset_mirror->currentIndex();
    Configs::dataManager->settingsRepo->resolve_domain_strategy = ui->domainStrategyCombo->currentText();
    Configs::dataManager->settingsRepo->default_domain_strategy = ui->default_domain_strategy->currentText();
    Configs::dataManager->settingsRepo->use_dns_object = ui->use_dns_object->isChecked();
    Configs::dataManager->settingsRepo->dns_object = dns_object_text;
    Configs::dataManager->settingsRepo->remote_dns = ui->remote_dns->currentText().trimmed();
    Configs::dataManager->settingsRepo->remote_dns_disable_ipv6 = ui->remote_dns_disable_ipv6->isChecked();
    Configs::dataManager->settingsRepo->dns_cache_capacity = dns_advanced.cache_capacity;
    Configs::dataManager->settingsRepo->dns_disable_cache = dns_advanced.disable_cache;
    Configs::dataManager->settingsRepo->dns_disable_expire = dns_advanced.disable_expire;
    Configs::dataManager->settingsRepo->dns_persist_cache = dns_advanced.persist_cache;
    Configs::dataManager->settingsRepo->dns_reverse_mapping = dns_advanced.reverse_mapping;
    Configs::dataManager->settingsRepo->dns_optimistic = dns_advanced.optimistic;
    Configs::dataManager->settingsRepo->dns_optimistic_timeout = dns_advanced.optimistic_timeout;
    Configs::dataManager->settingsRepo->dns_query_timeout = dns_advanced.query_timeout;
    Configs::dataManager->settingsRepo->direct_dns = ui->direct_dns->currentText().trimmed();
    Configs::dataManager->settingsRepo->direct_dns_disable_ipv6 = ui->direct_dns_disable_ipv6->isChecked();
    Configs::dataManager->settingsRepo->core_box_underlying_dns = ui->local_override->text().trimmed();
    Configs::dataManager->settingsRepo->dns_final_out = ui->dns_final_out->currentText();
    Configs::dataManager->settingsRepo->fake_dns = ui->enable_fakeip->isChecked();
    Configs::dataManager->settingsRepo->fakeip_disable_ipv6 = ui->fakeip_disable_ipv6->isChecked();
    Configs::dataManager->settingsRepo->enable_dns_routing = ui->enable_dns_routing->isChecked();
    Configs::dataManager->settingsRepo->dns_use_hosts = ui->respect_hosts->isChecked();
    Configs::dataManager->settingsRepo->dns_predefined_enable = predefined_dns_enabled;
    QStringList predefinedRules;
    for (const auto &line : predefined_dns_text.split("\n")) {
        if (!line.trimmed().isEmpty()) predefinedRules.append(line.trimmed());
    }
    Configs::dataManager->settingsRepo->dns_predefined_rules = predefinedRules;

    Configs::dataManager->routesRepo->UpdateRouteProfiles(chainList);
    Configs::dataManager->settingsRepo->current_route_id = currentRoute->id;

    Configs::dataManager->settingsRepo->enable_warp = ui->enable_warp->isChecked();
    Configs::dataManager->settingsRepo->warp_ep = ui->warp_ep->text().trimmed();
    Configs::dataManager->settingsRepo->warp_ifc_addrs = manageRoutesWithPrefix(SplitAndTrim(ui->warp_ifc_addrs->text(), ",", false));
    Configs::dataManager->settingsRepo->warp_private_key = ui->warp_private_key->text().trimmed();
    Configs::dataManager->settingsRepo->warp_public_key = ui->warp_public_key->text().trimmed();
    Configs::dataManager->settingsRepo->warp_reserved = SplitAndTrim(ui->warp_reserved->text(), ",", false);
    Configs::dataManager->settingsRepo->warp_mode = ui->warp_mode->currentIndex() == 1 ? "masque" : "wireguard";
    Configs::dataManager->settingsRepo->warp_masque_ep = ui->warp_masque_ep->text().trimmed();
    Configs::dataManager->settingsRepo->warp_masque_private_key = ui->warp_masque_private_key->text().trimmed();
    Configs::dataManager->settingsRepo->warp_masque_peer_public_key = ui->warp_masque_peer_public_key->text().trimmed();
    Configs::dataManager->settingsRepo->warp_masque_ifc_addrs = manageRoutesWithPrefix(SplitAndTrim(ui->warp_masque_ifc_addrs->text(), ",", false));
    Configs::dataManager->settingsRepo->warp_masque_sni = ui->warp_masque_sni->text().trimmed();
    Configs::dataManager->settingsRepo->warp_masque_http_mode = ui->warp_masque_http_mode->currentIndex();

    MW_dialog_message(MwMessage::UpdateSettings, {MwArg::Route});

    QDialog::accept();
}

void DialogManageRoutes::on_new_route_clicked() {
    QMenu menu(this);
    menu.addAction(tr("Structured profile"));
    auto* rawAct = menu.addAction(tr("Raw profile"));
    auto* remoteAct = menu.addAction(tr("Remote profile"));
    auto* chosen = menu.exec(ui->new_route->mapToGlobal(QPoint(0, ui->new_route->height())));
    if (chosen == nullptr) return;

    auto newProfile = Configs::dataManager->routesRepo->NewRouteProfile();
    auto onCreated = [=, this](const std::shared_ptr<Configs::RouteProfile>& chain) {
        chainList << chain;
        reloadProfileItems();
    };
    if (chosen == rawAct) {
        newProfile->isRaw = true;
        auto* rawWidget = new RawRouteItem(this, newProfile);
        rawWidget->setWindowModality(Qt::ApplicationModal);
        rawWidget->show();
        connect(rawWidget, &RawRouteItem::settingsChanged, this, onCreated);
    } else {
        // Remote profiles are structured underneath; RouteItem grows a "Remote source" section when isRemote.
        if (chosen == remoteAct) newProfile->isRemote = true;
        routeChainWidget = new RouteItem(this, newProfile);
        routeChainWidget->setWindowModality(Qt::ApplicationModal);
        routeChainWidget->show();
        connect(routeChainWidget, &RouteItem::settingsChanged, this, onCreated);
    }
}

void DialogManageRoutes::on_export_route_clicked()
{
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    QString warnings;
    QApplication::clipboard()->setText(chainList[idx]->ToShareLink(&warnings));
    if (!warnings.isEmpty()) manageRoutesPlainBox(this, QMessageBox::Information, tr("Exported with warnings"), warnings.trimmed());

    QToolTip::showText(QCursor::pos(), tr("Copied!"), this);
    int r = ++tooltipID;
    QTimer::singleShot(1500, [=,this] {
        if (tooltipID != r) return;
        QToolTip::hideText();
    });
}

void DialogManageRoutes::applyImportedProfile(const std::shared_ptr<Configs::RouteProfile>& profile, bool wasOldArray)
{
    if (wasOldArray) {
        // A legacy rule array carries no name / default outbound, so it cannot be saved unedited.
        auto shell = Configs::dataManager->routesRepo->NewRouteProfile();
        shell->Rules = profile->Rules;
        routeChainWidget = new RouteItem(this, shell);
        routeChainWidget->setWindowModality(Qt::ApplicationModal);
        routeChainWidget->show();
        connect(routeChainWidget, &RouteItem::settingsChanged, this, [=, this](const std::shared_ptr<Configs::RouteProfile>& chain) {
            chainList << chain;
            reloadProfileItems();
        });
    } else {
        chainList << profile;
        currentRoute = profile;
        reloadProfileItems();
    }
}

bool DialogManageRoutes::tryImportRemoteRoutesLink(const QString& text)
{
    bool wasRemoteRouteLink = false;
    QString error;
    auto profiles = Configs::RouteProfile::FromRemoteRoutesLink(text, &wasRemoteRouteLink, &error);
    if (!wasRemoteRouteLink) return false;

    if (profiles.isEmpty()) {
        MessageBoxWarning(tr("Add remote routing profiles"),
                          error.isEmpty() ? tr("No valid remote routing profiles in the link.") : error);
        return true;
    }

    QString prompt = tr("Add these remote routing profiles?") + "\n";
    for (int i = 0; i < profiles.size(); ++i) {
        prompt += QString("\n%1. %2  (%3: %4)")
                      .arg(i + 1)
                      .arg(profiles[i]->remoteURL, tr("auto update"), profiles[i]->autoUpdate ? tr("On") : tr("Off"));
    }
    QMessageBox box(QMessageBox::Question, tr("Add remote routing profiles"), prompt, QMessageBox::Yes | QMessageBox::No, this);
    box.setTextFormat(Qt::PlainText);
    if (box.exec() != QMessageBox::Yes) {
        return true; // it was a remoteRoute link; the user declined
    }

    for (const auto& p : profiles) chainList << p;
    reloadProfileItems();
    updateRemoteProfiles(profiles);
    return true;
}

void DialogManageRoutes::on_import_route_clicked()
{
    const QString clip = QApplication::clipboard()->text().trimmed();
    if (tryImportRemoteRoutesLink(clip)) return;
    if (!clip.isEmpty()) {
        QString fatal, warnings;
        bool wasOldArray = false;
        if (auto profile = Configs::RouteProfile::FromShareInput(clip, &fatal, &warnings, &wasOldArray)) {
            const QString what = wasOldArray ? tr("a routing rule list")
                                             : tr("routing profile \"%1\"").arg(profile->name);
            QString prompt = tr("Import %1 from the clipboard?").arg(what);
            // endpoints are already created by the parse above, before this prompt
            if (!warnings.isEmpty()) prompt += "\n\n" + tr("Note:") + "\n" + warnings.trimmed();
            QMessageBox box(QMessageBox::Question, tr("Import from clipboard"), prompt,
                            QMessageBox::Yes | QMessageBox::No, this);
            box.setTextFormat(Qt::PlainText);
            if (box.exec() == QMessageBox::Yes) {
                applyImportedProfile(profile, wasOldArray);
                return;
            }
        }
    }

    auto w = new QDialog(this);
    w->setWindowTitle(tr("Import routing profile"));
    w->setWindowModality(Qt::ApplicationModal);

    auto layout = new QGridLayout(w);
    auto tEdit = new QTextEdit(w);
    tEdit->setPlaceholderText(tr("Paste a Throne route link, a remoteRoute link, a base64 blob, or a JSON rule array"));
    layout->addWidget(tEdit, 0, 0);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, w);
    layout->addWidget(buttons, 1, 0);

    connect(buttons, &QDialogButtonBox::accepted, w, [=, this] {
        if (tryImportRemoteRoutesLink(tEdit->toPlainText())) { w->accept(); return; }
        QString fatal, warnings;
        bool wasOldArray = false;
        auto profile = Configs::RouteProfile::FromShareInput(tEdit->toPlainText(), &fatal, &warnings, &wasOldArray);
        if (!profile) {
            MessageBoxWarning(tr("Invalid input"), tr("Could not import this routing profile:\n") + fatal);
            return;
        }
        if (!warnings.isEmpty()) manageRoutesPlainBox(w, QMessageBox::Information, tr("Imported with warnings"), warnings.trimmed());
        applyImportedProfile(profile, wasOldArray);
        w->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, w, &QDialog::reject);

    w->exec();
    w->deleteLater();
}

void DialogManageRoutes::on_clone_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    auto chainCopy = std::make_shared<Configs::RouteProfile>(*chainList[idx]);
    chainCopy->name = chainCopy->name + " clone";
    chainCopy->id = -1;
    chainList.append(chainCopy);
    reloadProfileItems();
}

void DialogManageRoutes::on_edit_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;

    auto onEdited = [=, this](const std::shared_ptr<Configs::RouteProfile>& chain) {
        if (currentRoute == chainList[idx]) currentRoute = chain;
        chainList[idx] = chain;
        reloadProfileItems();
    };

    if (chainList[idx]->isRaw) {
        auto* rawWidget = new RawRouteItem(this, chainList[idx]);
        rawWidget->setWindowModality(Qt::ApplicationModal);
        rawWidget->show();
        connect(rawWidget, &RawRouteItem::settingsChanged, this, onEdited);
    } else {
        routeChainWidget = new RouteItem(this, chainList[idx]);
        routeChainWidget->setWindowModality(Qt::ApplicationModal);
        routeChainWidget->show();
        connect(routeChainWidget, &RouteItem::settingsChanged, this, onEdited);
    }
}

void DialogManageRoutes::on_delete_route_clicked() {
    auto idx = ui->route_profiles->currentRow();
    if (idx < 0) return;
    if (chainList.size() == 1) {
        MessageBoxWarning(tr("Invalid operation"), tr("Routing Profiles cannot be empty, try adding another profile or editing this one"));
        return;
    }

    auto profileToDel = chainList[idx];
    chainList.removeAt(idx);
    if (profileToDel == currentRoute) {
        currentRoute = chainList[0];
    }
    reloadProfileItems();
}

void DialogManageRoutes::on_update_route_clicked() {
    if (routeUpdateRunning) {
        QMenu menu(this);
        auto* cancelAct = menu.addAction(tr("Cancel"));
        auto* chosen = menu.exec(ui->update_route->mapToGlobal(QPoint(0, ui->update_route->height())));
        // Guard against the batch having finished while the menu was open.
        if (chosen == cancelAct && routeUpdateRunning) {
            routeUpdateCancel = true;
            ui->update_route->setText(tr("Cancelling..."));
        }
        return;
    }

    const int idx = ui->route_profiles->currentRow();
    const bool selIsRemote = idx >= 0 && chainList[idx]->isRemote;

    QMenu menu(this);
    QAction* updateSelAct = selIsRemote ? menu.addAction(tr("Update selected")) : nullptr;
    auto* updateAllAct = menu.addAction(tr("Update all"));
    auto* chosen = menu.exec(ui->update_route->mapToGlobal(QPoint(0, ui->update_route->height())));
    if (chosen == nullptr) return;

    if (chosen == updateSelAct) {
        updateRemoteProfiles({chainList[idx]});
        return;
    }

    if (chosen == updateAllAct) {
        QList<std::shared_ptr<Configs::RouteProfile>> remotes;
        for (const auto& p : chainList) {
            if (p->isRemote && !p->remoteURL.trimmed().isEmpty()) remotes << p;
        }
        if (remotes.isEmpty()) {
            MessageBoxInfo(tr("No remote profiles"), tr("There are no remote routing profiles to update."));
            return;
        }
        updateRemoteProfiles(remotes);
    }
}

void DialogManageRoutes::updateRemoteProfiles(const QList<std::shared_ptr<Configs::RouteProfile>>& profiles) {
    if (routeUpdateRunning || profiles.isEmpty()) return;
    routeUpdateRunning = true;
    routeUpdateCancel = false;
    const int total = profiles.size();

    // The button stays enabled during the run so its click can offer Cancel (see the slot above).
    auto progressText = [total](int current) {
        return total <= 1 ? tr("Updating...") : tr("Updating (%1 / %2)").arg(current).arg(total);
    };
    ui->update_route->setText(progressText(1));

    runOnNewThread([=, this] {
        QStringList failures;
        QStringList warned;
        int ok = 0;
        for (int i = 0; i < profiles.size(); ++i) {
            if (routeUpdateCancel.load()) break;
            const int current = i + 1;
            runOnUiThread([=, this] {
                if (routeUpdateRunning && !routeUpdateCancel.load())
                    ui->update_route->setText(progressText(current));
            });
            QString warnings;
            const QString err = RouteUpdate::UpdateProfile(profiles[i], &warnings);
            if (err.isEmpty()) ok++;
            else failures << (profiles[i]->name + ": " + err);
            if (err.isEmpty() && !warnings.trimmed().isEmpty()) warned << (profiles[i]->name + ":\n" + warnings.trimmed());
        }
        const bool cancelled = routeUpdateCancel.load();
        runOnUiThread([=, this] {
            routeUpdateRunning = false;
            ui->update_route->setText(tr("Update"));
            reloadProfileItems();
            const QString details = warned.join("\n\n");
            const QString note = warned.isEmpty() ? QString()
                                                  : "\n\n" + tr("%n profile(s) were updated with warnings; see the details.", "", static_cast<int>(warned.size()));
            if (cancelled) {
                manageRoutesPlainBox(this, QMessageBox::Information, tr("Update cancelled"),
                                     tr("Cancelled: updated %1 of %2, %3 failed.").arg(ok).arg(total).arg(failures.size()) + note, details);
            } else if (failures.isEmpty()) {
                manageRoutesPlainBox(this, QMessageBox::Information, tr("Update complete"),
                                     tr("Updated %1 remote routing profile(s).").arg(ok) + note, details);
            } else {
                manageRoutesPlainBox(this, QMessageBox::Warning, tr("Update finished with errors"),
                                     tr("Updated %1, failed %2:\n%3").arg(ok).arg(failures.size()).arg(failures.join("\n")) + note, details);
            }
        });
    });
}
