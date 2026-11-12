#include "include/ui/scanner/dialog_scan_instance.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCompleter>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHostAddress>
#include <QInputDialog>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScopedValueRollback>
#include <QScreen>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>
#include <type_traits>
#include <vector>

#include "include/configs/common/DnsTunnel.h"
#include "include/configs/common/ResolverSource.h"
#include "include/configs/common/TLS.h"
#include "include/configs/sub/warp.h"
#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/global/RunningProfiles.hpp"
#include "include/global/Utils.hpp"
#include "include/global/Validation.hpp"
#include "include/scanner/DefaultIpLists.h"
#include "include/scanner/IpListParse.h"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanManager.h"
#include "include/scanner/ScanProfiles.h"
#include "include/scanner/TuneProfiles.h"
#include "include/scanner/WarpPresets.h"
#include "include/ui/mainwindow.h"
#include "include/ui/scanner/IpEntriesModel.h"
#include "include/ui/scanner/ScanItem.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/utils/NoWheelComboBox.h"
#include "include/ui/utils/ValidationForm.h"
#include "include/ui/utils/NativeFileDialog.h"
#include "include/ui/widget/ResolverSourceWidget.h"

namespace {
    constexpr int kScanInstanceResultsPreview = 50000;
    constexpr int kScanInstanceProtocolRole = Qt::UserRole + 1;

    const QStringList kScanInstanceCandidateExclusions = {
        QStringLiteral("custom"), QStringLiteral("extracore"), QStringLiteral("tailscale"),
        QStringLiteral("autoselector"), QStringLiteral("direct"), QStringLiteral("dnstt"), QStringLiteral("masterdns"),
    };

    const QStringList kScanInstanceTimeoutPresets = {
        QStringLiteral("1000"), QStringLiteral("2000"), QStringLiteral("3000"), QStringLiteral("5000"),
    };

    const QStringList kScanInstanceConcurrencyPresets = {
        QStringLiteral("8"), QStringLiteral("16"), QStringLiteral("32"),
        QStringLiteral("64"), QStringLiteral("128"), QStringLiteral("256"),
    };

    const QStringList kScanInstanceMtuPresets = {QStringLiteral("1280"), QStringLiteral("1420")};

    void scanInstanceSetupNumber(QLineEdit *edit, int minimum, int maximum, const QString &placeholder,
                                 const QStringList &presets = {}) {
        edit->setValidator(new QIntValidator(minimum, maximum, edit));
        edit->setPlaceholderText(placeholder);
        if (!presets.isEmpty()) edit->setCompleter(new QCompleter(presets, edit));
    }

    void scanInstanceSetupNumber(QLineEdit *edit, int minimum, int maximum, int defaultValue, const QStringList &presets = {}) {
        scanInstanceSetupNumber(edit, minimum, maximum, QString::number(defaultValue), presets);
    }

    // Parses with the validator's locale, the one that accepted the text.
    bool scanInstanceReadNumber(const QLineEdit *edit, int &value) {
        if (!edit->hasAcceptableInput()) return false;
        bool ok = false;
        const int parsed = edit->validator()->locale().toInt(edit->text(), &ok);
        if (ok) value = parsed;
        return ok;
    }

    QString scanInstanceNumberError(const QLineEdit *edit) {
        int value = 0;
        if (scanInstanceReadNumber(edit, value)) return {};
        const auto *validator = qobject_cast<const QIntValidator *>(edit->validator());
        return DialogScanInstance::tr("Must be from %1 to %2.").arg(validator->bottom()).arg(validator->top());
    }

    // The probe prefixes a '/' and reads it with Go's url.ParseRequestURI, then url.Parse; neither checks the raw query.
    QString scanInstancePathError(const QString &path) {
        const auto hex = [&path](qsizetype at) {
            if (at >= path.size()) return false;
            const char16_t c = path[at].unicode();
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        };
        const qsizetype query = path.indexOf(QLatin1Char('?'));
        const qsizetype fragment = path.indexOf(QLatin1Char('#'));
        const qsizetype queryEnd = query < 0 ? -1 : (fragment > query ? fragment : path.size());
        for (qsizetype i = 0; i < path.size(); ++i) {
            const char16_t c = path[i].unicode();
            if (c < 0x20 || c == 0x7f) return DialogScanInstance::tr("Must not contain control characters.");
            if (c != '%' || (i > query && i < queryEnd)) continue;
            if (!hex(i + 1) || !hex(i + 2))
                return DialogScanInstance::tr("Use % only in an escape such as %20 (write a literal % as %25), for example /generate_204.");
            i += 2;
        }
        return {};
    }

    QStringList scanInstanceAlpn(const QString &text) {
        QStringList alpn;
        for (const auto &token : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
            if (const auto value = token.trimmed(); !value.isEmpty()) alpn << value;
        }
        return alpn;
    }

    QString scanInstanceTlsRangeError(const QString &minVersion, const QString &maxVersion) {
        if (minVersion.isEmpty() || maxVersion.isEmpty() || minVersion <= maxVersion) return {};
        return DialogScanInstance::tr("The minimum TLS version is higher than the maximum, so every handshake would fail.");
    }

    // Without "h2" the server never agrees to HTTP/2 and every target fails.
    QString scanInstanceAlpnError(const QString &httpVersion, const QStringList &alpn) {
        if (httpVersion != QStringLiteral("2") || alpn.isEmpty() || alpn.contains(QStringLiteral("h2"))) return {};
        return DialogScanInstance::tr("HTTP/2 needs \"h2\" in the ALPN list; add it or leave ALPN empty.");
    }

    QString scanInstanceNeedsTlsError(bool tls, const QString &method, const QString &httpVersion) {
        if (tls) return {};
        if (method == QStringLiteral("NONE"))
            return DialogScanInstance::tr("The method \"None\" only checks the TLS handshake, so it needs TLS.");
        if (httpVersion == QStringLiteral("3")) return DialogScanInstance::tr("HTTP/3 needs TLS.");
        return {};
    }

    QString scanInstanceResolverLevelLabel(const QString &level) {
        if (level == QLatin1String("poll")) return DialogScanInstance::tr("Poll: relays one tunnel query");
        if (level == QLatin1String("qname")) return DialogScanInstance::tr("Query names: also relays names as long as the profile sends");
        if (level == QLatin1String("handshake")) return DialogScanInstance::tr("Handshake: also opens a tunnel session");
        if (level == QLatin1String("quick")) return DialogScanInstance::tr("Quick: relays the smallest packets");
        if (level == QLatin1String("full")) return DialogScanInstance::tr("Full: also measures the upload and download MTU");
        return level;
    }

    void scanInstanceSetRowVisible(QFormLayout *form, QWidget *field, bool visible) {
        if (auto *label = form->labelForField(field)) label->setVisible(visible);
        field->setVisible(visible);
    }

    void scanInstanceSelectData(QComboBox *combo, const QVariant &data) {
        const int index = combo->findData(data);
        combo->setCurrentIndex(index >= 0 ? index : 0);
    }

    QList<int> scanInstanceParsePorts(const QString &text, QString *bad) {
        QList<int> ports;
        const auto tokens = text.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
        for (const auto &token : tokens) {
            bool ok = false;
            const int port = token.toInt(&ok);
            if (!ok || !IsValidPort(port)) {
                if (bad != nullptr) *bad = token;
                return {};
            }
            if (!ports.contains(port)) ports.append(port);
        }
        return ports;
    }

    QString scanInstanceJoinPorts(const QList<int> &ports) {
        QStringList parts;
        for (const int port : ports) parts << QString::number(port);
        return parts.join(QStringLiteral(", "));
    }

    void scanInstanceAttachCompleter(QObject *owner, QComboBox *combo, QCompleter *completer) {
        auto *lineEdit = combo->lineEdit();
        completer->setWidget(lineEdit);
        auto *debounce = new QTimer(owner);
        debounce->setSingleShot(true);
        debounce->setInterval(300);
        QObject::connect(debounce, &QTimer::timeout, completer, [completer, lineEdit] {
            const QString text = lineEdit->text();
            if (text.isEmpty()) return;
            completer->setCompletionPrefix(text);
            completer->complete();
        });
        QObject::connect(lineEdit, &QLineEdit::textEdited, debounce, [debounce, completer](const QString &text) {
            if (text.isEmpty()) {
                debounce->stop();
                completer->popup()->hide();
                return;
            }
            debounce->start();
        });
        QObject::connect(completer, qOverload<const QString &>(&QCompleter::activated), lineEdit,
                         [combo, lineEdit](const QString &text) {
                             lineEdit->setText(text);
                             const int index = combo->findText(text, Qt::MatchExactly);
                             if (index >= 0) combo->setCurrentIndex(index);
                         });
    }

    bool scanInstanceSamePorts(QList<int> ports, QList<int> preset) {
        std::sort(ports.begin(), ports.end());
        std::sort(preset.begin(), preset.end());
        return ports == preset;
    }

    QByteArray scanInstanceAddressKey(const QString &cidr) {
        const auto slash = cidr.indexOf(QLatin1Char('/'));
        const QHostAddress host(slash < 0 ? cidr : cidr.left(slash));
        QByteArray key;
        if (host.protocol() == QAbstractSocket::IPv4Protocol) {
            const quint32 address = host.toIPv4Address();
            key.append('4');
            for (int shift = 24; shift >= 0; shift -= 8) key.append(static_cast<char>((address >> shift) & 0xff));
            key.append(static_cast<char>(slash < 0 ? 32 : cidr.mid(slash + 1).toInt()));
        } else if (host.protocol() == QAbstractSocket::IPv6Protocol) {
            const Q_IPV6ADDR address = host.toIPv6Address();
            key.append('6');
            key.append(reinterpret_cast<const char *>(address.c), 16);
            key.append(static_cast<char>(slash < 0 ? 128 : cidr.mid(slash + 1).toInt()));
        } else {
            key = QByteArray("9") + cidr.toUtf8();
        }
        return key;
    }

    // Must mirror IpEntriesModel::SortRole and the proxy's stable sort; copy and export follow the view.
    void scanInstanceSortEntries(QList<Configs::IpListEntry> &entries, int column, Qt::SortOrder order) {
        const auto sortBy = [&entries, order](const auto &keyOf) {
            using Key = std::decay_t<decltype(keyOf(entries.first()))>;
            std::vector<std::pair<Key, qsizetype>> keyed;
            keyed.reserve(entries.size());
            for (qsizetype i = 0; i < entries.size(); ++i) keyed.emplace_back(keyOf(entries.at(i)), i);
            std::stable_sort(keyed.begin(), keyed.end(), [order](const auto &a, const auto &b) {
                return order == Qt::AscendingOrder ? a.first < b.first : b.first < a.first;
            });
            QList<Configs::IpListEntry> sorted;
            sorted.reserve(entries.size());
            for (const auto &item : keyed) sorted.append(entries.at(item.second));
            entries = std::move(sorted);
        };
        if (entries.size() < 2) return;
        switch (column) {
            case IpEntriesModel::AddressColumn:
                sortBy([](const Configs::IpListEntry &entry) { return scanInstanceAddressKey(entry.cidr); });
                break;
            case IpEntriesModel::PortColumn:
                sortBy([](const Configs::IpListEntry &entry) { return entry.port; });
                break;
            case IpEntriesModel::LatencyColumn:
                sortBy([](const Configs::IpListEntry &entry) {
                    return entry.latencyMs > 0 ? entry.latencyMs : std::numeric_limits<int>::max();
                });
                break;
            case IpEntriesModel::MaxQNameColumn:
                sortBy([](const Configs::IpListEntry &entry) { return entry.maxQName; });
                break;
            case IpEntriesModel::UploadMtuColumn:
                sortBy([](const Configs::IpListEntry &entry) { return entry.uploadMtu; });
                break;
            case IpEntriesModel::DownloadMtuColumn:
                sortBy([](const Configs::IpListEntry &entry) { return entry.downloadMtu; });
                break;
            default: break;
        }
    }

    void scanInstanceSyncFoundCount(int scanId) {
        if (Scanner::ScanManager::instance()->IsRunning(scanId)) return;
        const auto row = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
        if (row == nullptr) return;
        const int found = row->result_list_id >= 0 ? Configs::dataManager->ipListsRepo->EntryCount(row->result_list_id) : 0;
        Configs::dataManager->ipScansRepo->SaveProgress(scanId, row->mode, row->ActiveCursor(), row->ActiveTotal(), found,
                                                        row->removed);
        Scanner::ScanManager::instance()->NotifyScansChanged();
        Scanner::IpListUpdater::instance()->NotifyListsChanged();
    }

    void scanInstanceNotice(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text) {
        auto *box = new QMessageBox(icon, title, text, QMessageBox::Ok, parent);
        box->setTextFormat(Qt::PlainText);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setWindowModality(Qt::NonModal);
        box->show();
    }
}

QHash<int, QPointer<DialogScanInstance>> &DialogScanInstance::openWindows() {
    static QHash<int, QPointer<DialogScanInstance>> windows;
    return windows;
}

void DialogScanInstance::Open(int scanId) {
    auto &windows = openWindows();
    if (DialogScanInstance *existing = windows.value(scanId)) {
        ActivateWindow(existing);
        return;
    }
    auto row = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
    if (row == nullptr) return;
    auto *window = new DialogScanInstance(GetMainWindow(), row);
    windows.insert(scanId, window);
    ActivateWindow(window);
}

void DialogScanInstance::Discard(int scanId) {
    if (DialogScanInstance *window = openWindows().value(scanId)) {
        window->discarding = true;
        window->close();
    }
}

bool DialogScanInstance::SaveIfDirty(int scanId) {
    DialogScanInstance *window = openWindows().value(scanId);
    if (window == nullptr || window->saveIfDirty()) return true;
    ActivateWindow(window);
    return false;
}

DialogScanInstance::DialogScanInstance(QWidget *parent, std::shared_ptr<Configs::IpScan> scan_)
    : QDialog(parent), ui(new Ui::DialogScanInstance), scan(std::move(scan_)), scanId(scan->id), config(scan->config),
      defaults(Configs::IpScan::DefaultConfig(scan->kind)) {
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    form = new Validation::Form(this);

    {
        const QScopedValueRollback guard(loading, true);
        setupTargets();
        buildPhases();
        bindValidation();
        setupResults();
        setupRunPanel();
        loadForm();
    }
    connect(ui->name, &QLineEdit::textChanged, this, [this] { markDirty(); });
    watchEdits(ui->targets_content);
    watchEdits(ui->phases_content);
    // QDialog makes the first auto-default button the default, so Enter in any field would click "Manage".
    for (auto *button : findChildren<QPushButton *>()) button->setAutoDefault(false);

    auto *manager = Scanner::ScanManager::instance();
    connect(manager, &Scanner::ScanManager::progressChanged, this, [this](int id) {
        if (id == scanId) refreshRun();
    });
    connect(manager, &Scanner::ScanManager::statusChanged, this, [this](int id) {
        if (id != scanId) return;
        reloadRow();
        refreshRun();
        scheduleResultsReload();
    });
    connect(manager, &Scanner::ScanManager::resultsChanged, this, [this](int id) {
        if (id == scanId) scheduleResultsReload();
    });
    connect(manager, &Scanner::ScanManager::scansChanged, this, [this] {
        if (Configs::dataManager->ipScansRepo->GetIpScan(scanId) != nullptr) return;
        discarding = true;
        close();
    });

    auto *updater = Scanner::IpListUpdater::instance();
    connect(updater, &Scanner::IpListUpdater::listsChanged, this,
            [this] { reloadBaseLists(ui->base_list->currentData().toInt()); }, Qt::QueuedConnection);
    connect(updater, &Scanner::IpListUpdater::listUpdated, this,
            [this](int, const QString &) { reloadBaseLists(ui->base_list->currentData().toInt()); },
            Qt::QueuedConnection);
    connect(themeManager(), &ThemeManager::themeChanged, this, [this] { refreshRun(); });

    resultCount = ScannerUi::ResultCount(*scan);
    refreshRun();
    form->markAll();

    const auto *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const QSize wanted = sizeHint().expandedTo(QSize(640, 600));
    resize(scr != nullptr ? wanted.boundedTo(scr->availableGeometry().size()) : wanted);
}

DialogScanInstance::~DialogScanInstance() {
    auto &windows = openWindows();
    if (windows.value(scanId) == this || windows.value(scanId) == nullptr) windows.remove(scanId);
    if (watching) ScannerUi::Watch(scanId, false);
    delete phasesUi;
    delete tuneUi;
    delete ui;
}

bool DialogScanInstance::isGeneric() const {
    return scan->kind == Configs::IpScan::Kind::Generic;
}

bool DialogScanInstance::isWarp() const {
    return scan->kind == Configs::IpScan::Kind::Warp;
}

bool DialogScanInstance::isResolver() const {
    return scan->kind == Configs::IpScan::Kind::Resolver;
}

bool DialogScanInstance::isTune() const {
    return scan->kind == Configs::IpScan::Kind::Tune;
}

void DialogScanInstance::setupTargets() {
    using PortMode = Configs::ScanConfig::PortMode;
    ui->port_mode->addItem(tr("Ignore the list's ports"), static_cast<int>(PortMode::Ignore));
    ui->port_mode->addItem(tr("Merge with the list's ports"), static_cast<int>(PortMode::Merge));
    ui->port_mode->addItem(tr("Only the list's ports"), static_cast<int>(PortMode::List));
    ui->port_mode->setItemData(
        2, tr("Entries without a port use 443 (80 for plain HTTP)."), Qt::ToolTipRole);
    connect(ui->port_mode, &QComboBox::currentIndexChanged, this, [this] {
        const bool ownPorts = ui->port_mode->currentData().toInt() != static_cast<int>(PortMode::List);
        ui->ports->setEnabled(ownPorts);
        ui->port_presets->setEnabled(ownPorts);
    });
    auto *portPresets = new QMenu(ui->port_presets);
    ui->port_presets->setMenu(portPresets);
    ui->port_presets->setVisible(isWarp());
    connect(portPresets, &QMenu::aboutToShow, this, [this, portPresets] { fillPortPresets(portPresets); });
    scanInstanceSetupNumber(ui->stop_after, 0, 1000000, tr("No limit"));
    scanInstanceSetupNumber(ui->concurrency, 1, 1000, QString::number(defaults.concurrency),
                            kScanInstanceConcurrencyPresets);
    scanInstanceSetupNumber(ui->spawn_interval, 0, 1000, QString::number(defaults.spawnIntervalMs));
    connect(ui->manage_lists, &QPushButton::clicked, this, [this] {
        if (auto *window = GetMainWindow()) window->showIpListsDialog(ui->base_list->currentData().toInt());
    });
    connect(ui->use_warp_ranges, &QPushButton::clicked, this, [this] { useBuiltinWarpRanges(); });
    ui->use_warp_ranges->setVisible(isWarp());
    ui->use_warp_ranges->setToolTip(tr("Use Cloudflare's WARP ranges and ports for this mode."));
    if (isResolver()) {
        for (QWidget *widget : {static_cast<QWidget *>(ui->ports_label), static_cast<QWidget *>(ui->ports),
                                static_cast<QWidget *>(ui->port_mode)})
            widget->setVisible(false);
        ui->base_list->setToolTip(tr("Each address is tried as a resolver; the default port is 53."));
    }
    if (isTune()) {
        ui->tabs->setTabVisible(ui->tabs->indexOf(ui->tab_targets), false);
        ui->tabs->setTabText(ui->tabs->indexOf(ui->tab_phases), tr("Settings"));
        ui->tabs->setCurrentWidget(ui->tab_phases);
    }
}

void DialogScanInstance::buildPhases() {
    auto *layout = ui->phases_layout;
    if (isTune()) {
        buildTunePhase(layout);
        layout->addStretch(1);
        return;
    }
    phasesUi = new Ui::ScanPhases;
    auto *page = new QWidget(ui->phases_content);
    phasesUi->setupUi(page);
    layout->addWidget(page);
    layout->addStretch(1);

    phasesUi->icmp_box->setVisible(!isResolver());
    for (auto *box : {phasesUi->tcp_box, phasesUi->http_box, phasesUi->config_box}) box->setVisible(isGeneric());
    phasesUi->warp_box->setVisible(isWarp());
    phasesUi->resolver_box->setVisible(isResolver());
    if (!isResolver()) buildIcmpPhase();
    if (isGeneric()) {
        buildTcpPhase();
        buildHttpPhase();
        buildConfigPhase();
    }
    if (isWarp()) buildWarpPhase();
    if (isResolver()) buildResolverPhase();
}

void DialogScanInstance::buildIcmpPhase() {
    scanInstanceSetupNumber(phasesUi->icmp_timeout, 100, 60000, defaults.icmp.timeoutMs, kScanInstanceTimeoutPresets);
    scanInstanceSetupNumber(phasesUi->icmp_count, 1, 10, defaults.icmp.count);
}

void DialogScanInstance::buildTcpPhase() {
    scanInstanceSetupNumber(phasesUi->tcp_timeout, 100, 60000, defaults.tcp.timeoutMs, kScanInstanceTimeoutPresets);
    scanInstanceSetupNumber(phasesUi->tcp_attempts, 1, 10, defaults.tcp.attempts);
}

void DialogScanInstance::buildHttpPhase() {
    phasesUi->http_method->addItem(QStringLiteral("GET"), QStringLiteral("GET"));
    phasesUi->http_method->addItem(QStringLiteral("HEAD"), QStringLiteral("HEAD"));
    phasesUi->http_method->addItem(tr("None (TLS handshake only)"), QStringLiteral("NONE"));
    for (const auto &version : {QStringLiteral("1.1"), QStringLiteral("2"), QStringLiteral("3")})
        phasesUi->http_version->addItem(QStringLiteral("HTTP/") + version, version);
    for (auto *combo : {phasesUi->http_min_tls, phasesUi->http_max_tls}) {
        combo->addItem(tr("Default"), QString());
        for (const auto &version : {QStringLiteral("1.0"), QStringLiteral("1.1"), QStringLiteral("1.2"), QStringLiteral("1.3")})
            combo->addItem(QStringLiteral("TLS ") + version, version);
    }
    for (const auto &fingerprint : Configs::tlsFingerprints) {
        // the probe's Default already is Go's own TLS stack
        if (fingerprint == Configs::tlsUnsafeFingerprint) continue;
        phasesUi->http_fingerprint->addItem(fingerprint.isEmpty() ? tr("Default") : fingerprint, fingerprint);
    }
    scanInstanceSetupNumber(phasesUi->http_fragment_delay, 0, 10000, defaults.http.fragmentFallbackDelayMs);
    scanInstanceSetupNumber(phasesUi->http_timeout, 100, 60000, defaults.http.timeoutMs, kScanInstanceTimeoutPresets);

    connect(phasesUi->http_box, &QGroupBox::toggled, this, [this] { updateHttpControls(); });
    connect(phasesUi->http_tls, &QCheckBox::toggled, this, [this] { updateHttpControls(); });
    connect(phasesUi->http_fragment, &QCheckBox::toggled, this, [this] { updateHttpControls(); });
    connect(phasesUi->http_method, &QComboBox::currentIndexChanged, this, [this] { updateHttpControls(); });
}

void DialogScanInstance::buildConfigPhase() {
    configProfileNames = new QStringListModel(this);
    setupProfilePicker(phasesUi->config_profile, configProfileNames);
    phasesUi->config_url->setPlaceholderText(Configs::dataManager->settingsRepo->test_latency_url);
    scanInstanceSetupNumber(phasesUi->config_timeout, 100, 60000, defaults.config.timeoutMs, kScanInstanceTimeoutPresets);
}

void DialogScanInstance::buildWarpPhase() {
    phasesUi->warp_mode->addItem(QStringLiteral("WireGuard"), QStringLiteral("wireguard"));
    phasesUi->warp_mode->addItem(QStringLiteral("AmneziaWG"), QStringLiteral("amneziawg"));
    phasesUi->warp_mode->addItem(QStringLiteral("MASQUE"), QStringLiteral("masque"));
    phasesUi->warp_http_mode->addItem(tr("HTTP/3, falling back to HTTP/2"), 0);
    phasesUi->warp_http_mode->addItem(tr("HTTP/3 only"), 1);
    phasesUi->warp_http_mode->addItem(tr("HTTP/2 only"), 2);
    phasesUi->warp_identity->addItem(tr("Dedicated for this scan (recommended)"), QStringLiteral("generated"));
    phasesUi->warp_identity->addItem(tr("From a profile"), QStringLiteral("profile"));
    phasesUi->warp_identity->addItem(tr("Built-in WARP settings"), QStringLiteral("builtin"));
    scanInstanceSetupNumber(phasesUi->warp_mtu, 576, 9000, defaults.warp.mtu, kScanInstanceMtuPresets);
    scanInstanceSetupNumber(phasesUi->warp_jc, 0, 128, defaults.warp.jc);
    scanInstanceSetupNumber(phasesUi->warp_jmin, 0, 1280, defaults.warp.jmin);
    scanInstanceSetupNumber(phasesUi->warp_jmax, 0, 1280, defaults.warp.jmax);
    warpPackets = {phasesUi->warp_i1, phasesUi->warp_i2, phasesUi->warp_i3, phasesUi->warp_i4, phasesUi->warp_i5};
    phasesUi->warp_url->setPlaceholderText(Configs::dataManager->settingsRepo->test_latency_url);
    scanInstanceSetupNumber(phasesUi->warp_timeout, 100, 60000, defaults.warp.timeoutMs, kScanInstanceTimeoutPresets);

    connect(phasesUi->warp_mode, &QComboBox::currentIndexChanged, this, [this] {
        reloadWarpProfiles(phasesUi->warp_profile->currentData().toInt());
        updateWarpControls();
        matchWarpTargets();
    });
    connect(phasesUi->warp_identity, &QComboBox::currentIndexChanged, this, [this] { updateWarpControls(); });
    connect(phasesUi->warp_register, &QPushButton::clicked, this, [this] { registerWarpIdentity(); });
}

void DialogScanInstance::buildResolverPhase() {
    phasesUi->resolver_timeout->setValidator(new QIntValidator(1000, 300000, phasesUi->resolver_timeout));
    connect(phasesUi->resolver_profile, &QComboBox::currentIndexChanged, this, [this] {
        if (!loading) reloadResolverLevels(phasesUi->resolver_level->currentData().toString());
    });
    connect(phasesUi->resolver_level, &QComboBox::currentIndexChanged, this, [this] { updateResolverTimeoutHint(); });
}

// Editable: the first items are listed and the completer searches every name.
void DialogScanInstance::setupProfilePicker(QComboBox *combo, QStringListModel *names) {
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->setMaxCount(200);
    combo->setCompleter(nullptr);
    auto *completer = new QCompleter(this);
    completer->setModel(names);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    scanInstanceAttachCompleter(this, combo, completer);
}

void DialogScanInstance::bindValidation() {
    using namespace Validation;
    form->bind(ui->name, Rules::Required());
    form->bind(ui->ports, [](const QString &text) {
        QString bad;
        scanInstanceParsePorts(text, &bad);
        return bad.isEmpty() ? QString() : tr("\"%1\" is not a valid port (1-65535).").arg(bad);
    });
    form->bind(ui->stop_after, [this](const QString &text) { return text.isEmpty() ? QString() : scanInstanceNumberError(ui->stop_after); });
    QList<QLineEdit *> numbers = {ui->concurrency, ui->spawn_interval};
    if (isGeneric() || isWarp()) numbers << phasesUi->icmp_timeout << phasesUi->icmp_count;
    if (isGeneric()) {
        numbers << phasesUi->tcp_timeout << phasesUi->tcp_attempts << phasesUi->http_fragment_delay << phasesUi->http_timeout
                << phasesUi->config_timeout;
    }
    if (isWarp()) numbers << phasesUi->warp_mtu << phasesUi->warp_jc << phasesUi->warp_jmin << phasesUi->warp_jmax << phasesUi->warp_timeout;
    for (QLineEdit *edit : numbers) form->bind(edit, [edit](const QString &) { return scanInstanceNumberError(edit); });

    if (isGeneric()) {
        form->bind(phasesUi->http_sni, Rules::Host(Empty::Allowed));
        form->bind(phasesUi->http_host, Rules::HostPort(false, Empty::Allowed));
        form->bind(phasesUi->http_path, [](const QString &text) { return scanInstancePathError(text); });
        form->addRule({phasesUi->http_min_tls, phasesUi->http_max_tls}, [this] {
            return scanInstanceTlsRangeError(phasesUi->http_min_tls->currentData().toString(), phasesUi->http_max_tls->currentData().toString());
        }, this, Severity::Error, phasesUi->http_box->title());
        form->addRule({phasesUi->http_alpn, phasesUi->http_version}, [this] {
            if (!phasesUi->http_tls->isChecked() || phasesUi->http_method->currentData().toString() == QStringLiteral("NONE")) return QString();
            return scanInstanceAlpnError(phasesUi->http_version->currentData().toString(), scanInstanceAlpn(phasesUi->http_alpn->text()));
        }, this, Severity::Error, phasesUi->http_box->title());
        form->addRule({phasesUi->http_method, phasesUi->http_version, phasesUi->http_tls}, [this] {
            return scanInstanceNeedsTlsError(phasesUi->http_tls->isChecked(), phasesUi->http_method->currentData().toString(), phasesUi->http_version->currentData().toString());
        }, this, Severity::Error, phasesUi->http_box->title());
    }
    if (isGeneric()) form->bind(phasesUi->config_url, Rules::Url({QStringLiteral("http"), QStringLiteral("https")}));
    if (isResolver()) {
        form->bind(phasesUi->resolver_timeout,
                   [this](const QString &text) { return text.isEmpty() ? QString() : scanInstanceNumberError(phasesUi->resolver_timeout); });
    }
    if (isWarp()) {
        form->bind(phasesUi->warp_sni, Rules::Host(Empty::Allowed));
        form->bind(phasesUi->warp_url, Rules::Url({QStringLiteral("http"), QStringLiteral("https")}));
        form->addRule({phasesUi->warp_jmin, phasesUi->warp_jmax}, [this] {
            int jmin = 0, jmax = 0;
            if (!scanInstanceReadNumber(phasesUi->warp_jmin, jmin) || !scanInstanceReadNumber(phasesUi->warp_jmax, jmax) || jmin <= jmax) return QString();
            return tr("Jmin must not be larger than Jmax.");
        }, this, Severity::Error, phasesUi->warp_box->title());
    }
    if (tuneUi != nullptr) {
        for (QLineEdit *edit : {tuneUi->mtu_min, tuneUi->mtu_step, tuneUi->mtu_bytes, tuneUi->attempts, tuneUi->per_minute,
                                tuneUi->max_attempts, tuneUi->max_minutes, tuneUi->timeout})
            form->bind(edit, [edit](const QString &) { return scanInstanceNumberError(edit); });
        auto *mtuMinEdit = tuneUi->mtu_min;
        auto *mtuMaxEdit = tuneUi->mtu_max;
        form->bind(mtuMaxEdit, [mtuMaxEdit](const QString &text) { return text.isEmpty() ? QString() : scanInstanceNumberError(mtuMaxEdit); });
        form->addRule({mtuMinEdit, mtuMaxEdit}, [mtuMinEdit, mtuMaxEdit] {
            int mtuMin = 0, mtuMax = 0;
            if (!mtuMaxEdit->isEnabled() || !scanInstanceReadNumber(mtuMinEdit, mtuMin) || !scanInstanceReadNumber(mtuMaxEdit, mtuMax) ||
                mtuMax == 0 || mtuMax > mtuMin)
                return QString();
            return tr("The largest MTU must be above the smallest; leave it empty for the profile's own.");
        }, this, Severity::Error, tuneUi->try_box->title());
        bindTuneValidation();
    }
}

void DialogScanInstance::setupResults() {
    resultsReload = new QTimer(this);
    resultsReload->setSingleShot(true);
    resultsReload->setInterval(1000);
    connect(resultsReload, &QTimer::timeout, this, [this] { reloadResults(); });
    connect(ui->tabs, &QTabWidget::currentChanged, this, [this] {
        if (resultsStale && ui->tabs->currentWidget() == ui->tab_results) reloadResults();
    });
    ui->apply_result->setVisible(isTune());
    ui->save_result_as_profile->setVisible(isTune());
    if (isTune()) {
        setupTuneResults();
        return;
    }

    resultsModel = new IpEntriesModel(this);
    resultsProxy = new QSortFilterProxyModel(this);
    resultsProxy->setSourceModel(resultsModel);
    resultsProxy->setSortRole(IpEntriesModel::SortRole);
    ui->results_view->setModel(resultsProxy);
    ui->results_view->sortByColumn(IpEntriesModel::LatencyColumn, Qt::AscendingOrder);
    auto *header = ui->results_view->horizontalHeader();
    header->setSectionResizeMode(IpEntriesModel::AddressColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(IpEntriesModel::PortColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(IpEntriesModel::LatencyColumn, QHeaderView::ResizeToContents);
    for (const int column : {IpEntriesModel::MaxQNameColumn, IpEntriesModel::UploadMtuColumn, IpEntriesModel::DownloadMtuColumn})
        header->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    applyResultColumns();

    connect(ui->save_as_list, &QPushButton::clicked, this, [this] { saveResultsAsList(); });
    connect(ui->copy_results, &QPushButton::clicked, this, [this] { copyResults(); });
    connect(ui->export_results, &QPushButton::clicked, this, [this] { exportResults(); });
    connect(ui->remove_results, &QPushButton::clicked, this, [this] { removeSelectedResults(); });
    if (isResolver()) {
        useAsResolvers = new QPushButton(tr("Use as resolvers"), ui->tab_results);
        useAsResolvers->setToolTip(tr("Add this list to the profile's resolvers."));
        ui->layout_results_buttons->insertWidget(0, useAsResolvers);
        connect(useAsResolvers, &QPushButton::clicked, this, [this] { useResultsAsResolvers(); });
    }
}

void DialogScanInstance::setupRunPanel() {
    const auto fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    for (auto *label : {ui->log_0, ui->log_1, ui->log_2}) label->setFont(fixed);

    connect(ui->start_button, &QPushButton::clicked, this, [this] { startClicked(); });
    connect(ui->pause_button, &QPushButton::clicked, this, [this] { pauseClicked(); });
    // OK, the title-bar X and Escape all end in reject(), which saves the edits.
    connect(ui->ok_button, &QPushButton::clicked, this, &QWidget::close);
}

void DialogScanInstance::watchEdits(QWidget *root) {
    const auto dirtyNow = [this] { markDirty(); };
    for (auto *edit : root->findChildren<QLineEdit *>()) connect(edit, &QLineEdit::textChanged, this, dirtyNow);
    for (auto *combo : root->findChildren<QComboBox *>()) connect(combo, &QComboBox::currentIndexChanged, this, dirtyNow);
    for (auto *check : root->findChildren<QCheckBox *>()) connect(check, &QCheckBox::toggled, this, dirtyNow);
    for (auto *group : root->findChildren<QGroupBox *>()) {
        if (group->isCheckable()) connect(group, &QGroupBox::toggled, this, dirtyNow);
    }
}

void DialogScanInstance::markDirty() {
    if (loading) return;
    dirty = true;
}

void DialogScanInstance::loadForm() {
    const QScopedValueRollback guard(loading, true);

    ui->name->setText(scan->name);
    ui->kind->setText(isWarp()       ? tr("WARP scan")
                      : isResolver() ? tr("DNS resolver scan")
                      : isTune()     ? tr("Find working settings")
                                     : tr("Generic scan"));
    setWindowTitle(tr("%1 - IP Scanner").arg(scan->name));

    reloadBaseLists(scan->base_list_id);
    ui->ports->setText(scanInstanceJoinPorts(config.ports));
    ui->port_mode->setCurrentIndex(qMax(0, ui->port_mode->findData(static_cast<int>(config.portMode))));
    ui->ports->setEnabled(config.portMode != Configs::ScanConfig::PortMode::List);
    ui->port_presets->setEnabled(config.portMode != Configs::ScanConfig::PortMode::List);
    ui->shuffle->setChecked(config.shuffle);
    ui->scan_ipv6->setChecked(config.scanIPv6);
    ui->stop_after->setText(config.stopAfter > 0 ? QString::number(config.stopAfter) : QString());
    ui->concurrency->setText(QString::number(config.concurrency));
    ui->spawn_interval->setText(QString::number(config.spawnIntervalMs));

    if (isGeneric() || isWarp()) {
        phasesUi->icmp_box->setChecked(config.icmp.enabled);
        phasesUi->icmp_timeout->setText(QString::number(config.icmp.timeoutMs));
        phasesUi->icmp_count->setText(QString::number(config.icmp.count));
    }

    if (isResolver()) {
        const auto &resolver = config.resolver;
        reloadResolverProfiles(resolver.profileId);
        reloadResolverLevels(resolver.level);
        phasesUi->resolver_timeout->setText(resolver.timeoutMs > 0 ? QString::number(resolver.timeoutMs) : QString());
    }

    if (isGeneric()) {
        phasesUi->tcp_box->setChecked(config.tcp.enabled);
        phasesUi->tcp_timeout->setText(QString::number(config.tcp.timeoutMs));
        phasesUi->tcp_attempts->setText(QString::number(config.tcp.attempts));
    }

    if (isGeneric()) {
        const auto &http = config.http;
        phasesUi->http_box->setChecked(http.enabled);
        phasesUi->http_tls->setChecked(http.tls);
        phasesUi->http_sni->setText(http.serverName);
        phasesUi->http_host->setText(http.host);
        phasesUi->http_path->setText(http.path);
        scanInstanceSelectData(phasesUi->http_method, http.method.toUpper());
        scanInstanceSelectData(phasesUi->http_version, http.httpVersion);
        phasesUi->http_alpn->setText(http.alpn.join(QStringLiteral(", ")));
        scanInstanceSelectData(phasesUi->http_min_tls, http.minVersion);
        scanInstanceSelectData(phasesUi->http_max_tls, http.maxVersion);
        scanInstanceSelectData(phasesUi->http_fingerprint, http.fingerprint);
        phasesUi->http_insecure->setChecked(http.insecure);
        phasesUi->http_disable_sni->setChecked(http.disableSni);
        phasesUi->http_fragment->setChecked(http.fragment);
        phasesUi->http_fragment_delay->setText(QString::number(http.fragmentFallbackDelayMs));
        phasesUi->http_record_fragment->setChecked(http.recordFragment);
        phasesUi->http_mixed_case_sni->setChecked(http.mixedCaseSni);
        phasesUi->http_timeout->setText(QString::number(http.timeoutMs));
    }

    if (isGeneric()) {
        const auto &test = config.config;
        phasesUi->config_box->setChecked(test.enabled);
        reloadConfigProfiles(test.profileId);
        phasesUi->config_url->setText(test.url);
        phasesUi->config_timeout->setText(QString::number(test.timeoutMs));
        phasesUi->config_warm->setChecked(test.warmLatency);
    }

    if (isWarp()) {
        const auto &warp = config.warp;
        scanInstanceSelectData(phasesUi->warp_mode, warp.mode);
        scanInstanceSelectData(phasesUi->warp_http_mode, warp.httpMode);
        scanInstanceSelectData(phasesUi->warp_identity, warp.identity);
        reloadWarpProfiles(warp.profileId);
        phasesUi->warp_mtu->setText(QString::number(warp.mtu));
        phasesUi->warp_sni->setText(warp.sni);
        phasesUi->warp_quic_opener->setChecked(warp.quicOpener);
        phasesUi->warp_jc->setText(QString::number(warp.jc));
        phasesUi->warp_jmin->setText(QString::number(warp.jmin));
        phasesUi->warp_jmax->setText(QString::number(warp.jmax));
        const QStringList packets = {warp.i1, warp.i2, warp.i3, warp.i4, warp.i5};
        for (int i = 0; i < warpPackets.size() && i < packets.size(); ++i) warpPackets[i]->setText(packets[i]);
        phasesUi->warp_url->setText(warp.url);
        phasesUi->warp_timeout->setText(QString::number(warp.timeoutMs));
        phasesUi->warp_warm->setChecked(warp.warmLatency);
    }

    if (tuneUi != nullptr) loadTuneForm();

    updateHttpControls();
    updateWarpControls();
    dirty = false;
}

bool DialogScanInstance::collect(Configs::ScanConfig &out, int &baseListId, QString &name, QString *error) const {
    const auto fail = [error](const QString &text) {
        if (error != nullptr) *error = text;
        return false;
    };
    // A field that is switched off keeps its stored value while it holds no valid number.
    const auto number = [&fail](const QLineEdit *edit, int &value, bool inUse, const QString &rangeError) {
        if (scanInstanceReadNumber(edit, value) || !inUse) return true;
        const auto *validator = qobject_cast<const QIntValidator *>(edit->validator());
        return fail(rangeError.arg(validator->bottom()).arg(validator->top()));
    };

    name = ui->name->text().trimmed();
    if (name.isEmpty()) return fail(tr("Enter a name for the scan."));

    out = config;
    QString badPort;
    out.ports = scanInstanceParsePorts(ui->ports->text(), &badPort);
    out.portMode = static_cast<Configs::ScanConfig::PortMode>(ui->port_mode->currentData().toInt());
    if (!badPort.isEmpty()) return fail(tr("\"%1\" is not a valid port (1-65535).").arg(badPort));
    baseListId = ui->base_list->currentData().toInt();
    out.shuffle = ui->shuffle->isChecked();
    out.scanIPv6 = ui->scan_ipv6->isChecked();
    out.stopAfter = 0;
    if (!ui->stop_after->text().trimmed().isEmpty() &&
        !number(ui->stop_after, out.stopAfter, true,
                tr("Stop after must be between %1 and %2 results; leave it empty for no limit.")))
        return false;
    if (!number(ui->concurrency, out.concurrency, true, tr("Concurrency must be between %1 and %2."))) return false;
    if (!number(ui->spawn_interval, out.spawnIntervalMs, true, tr("Spawn interval must be between %1 and %2 ms.")))
        return false;

    out.icmp.enabled = false;
    if (isGeneric() || isWarp()) {
        out.icmp.enabled = phasesUi->icmp_box->isChecked();
        if (!number(phasesUi->icmp_timeout, out.icmp.timeoutMs, out.icmp.enabled,
                    tr("ICMP (ping): the timeout must be between %1 and %2 ms.")))
            return false;
        if (!number(phasesUi->icmp_count, out.icmp.count, out.icmp.enabled,
                    tr("ICMP (ping): echo requests must be between %1 and %2.")))
            return false;
    }

    if (isResolver()) {
        // Every entry is probed on its own port, 53 when it has none.
        out.ports.clear();
        out.portMode = Configs::ScanConfig::PortMode::List;
        auto &resolver = out.resolver;
        resolver.profileId = phasesUi->resolver_profile->currentData().toInt();
        resolver.level = phasesUi->resolver_level->currentData().toString();
        resolver.timeoutMs = 0;
        if (!phasesUi->resolver_timeout->text().trimmed().isEmpty() &&
            !number(phasesUi->resolver_timeout, resolver.timeoutMs, true, tr("DNS tunnel test: the timeout must be between %1 and %2 ms.")))
            return false;
    }

    // WARP scans have no TCP or TLS / HTTP phase.
    out.tcp.enabled = false;
    out.http.enabled = false;

    if (isGeneric()) {
        out.tcp.enabled = phasesUi->tcp_box->isChecked();
        if (!number(phasesUi->tcp_timeout, out.tcp.timeoutMs, out.tcp.enabled,
                    tr("TCP connect: the timeout must be between %1 and %2 ms.")))
            return false;
        if (!number(phasesUi->tcp_attempts, out.tcp.attempts, out.tcp.enabled,
                    tr("TCP connect: attempts must be between %1 and %2.")))
            return false;
    }

    if (isGeneric()) {
        auto &http = out.http;
        http.enabled = phasesUi->http_box->isChecked();
        http.tls = phasesUi->http_tls->isChecked();
        http.serverName = phasesUi->http_sni->text().trimmed();
        http.host = phasesUi->http_host->text().trimmed();
        http.path = phasesUi->http_path->text().trimmed();
        if (http.path.isEmpty()) http.path = QStringLiteral("/");
        http.method = phasesUi->http_method->currentData().toString();
        http.httpVersion = phasesUi->http_version->currentData().toString();
        http.alpn = scanInstanceAlpn(phasesUi->http_alpn->text());
        http.minVersion = phasesUi->http_min_tls->currentData().toString();
        http.maxVersion = phasesUi->http_max_tls->currentData().toString();
        http.fingerprint = phasesUi->http_fingerprint->currentData().toString();
        http.insecure = phasesUi->http_insecure->isChecked();
        http.disableSni = phasesUi->http_disable_sni->isChecked();
        http.fragment = phasesUi->http_fragment->isChecked();
        if (!number(phasesUi->http_fragment_delay, http.fragmentFallbackDelayMs, http.enabled && http.tls && http.fragment,
                    tr("TLS / HTTP: the fragment fallback delay must be between %1 and %2 ms.")))
            return false;
        http.recordFragment = phasesUi->http_record_fragment->isChecked();
        http.mixedCaseSni = phasesUi->http_mixed_case_sni->isChecked();
        if (!number(phasesUi->http_timeout, http.timeoutMs, http.enabled,
                    tr("TLS / HTTP: the timeout must be between %1 and %2 ms.")))
            return false;
        if (http.enabled) {
            if (const auto problem = scanInstanceNeedsTlsError(http.tls, http.method, http.httpVersion); !problem.isEmpty())
                return fail(QStringLiteral("%1: %2").arg(phasesUi->http_box->title(), problem));
        }
    }

    if (isGeneric()) {
        auto &test = out.config;
        test.enabled = phasesUi->config_box->isChecked();
        test.profileId = selectedConfigProfile();
        test.url = phasesUi->config_url->text().trimmed();
        if (!number(phasesUi->config_timeout, test.timeoutMs, test.enabled,
                    tr("Config test: the timeout must be between %1 and %2 ms.")))
            return false;
        test.warmLatency = phasesUi->config_warm->isChecked();
    }

    if (isWarp()) {
        auto &warp = out.warp;
        warp.mode = phasesUi->warp_mode->currentData().toString();
        warp.httpMode = phasesUi->warp_http_mode->currentData().toInt();
        warp.identity = phasesUi->warp_identity->currentData().toString();
        warp.profileId = phasesUi->warp_profile->currentData().toInt();
        if (!number(phasesUi->warp_mtu, warp.mtu, true, tr("WARP test: the MTU must be between %1 and %2."))) return false;
        warp.sni = phasesUi->warp_sni->text().trimmed();
        warp.quicOpener = phasesUi->warp_quic_opener->isChecked();
        const bool amnezia = warp.mode == QStringLiteral("amneziawg");
        if (!number(phasesUi->warp_jc, warp.jc, amnezia, tr("WARP test: Jc must be between %1 and %2."))) return false;
        if (!number(phasesUi->warp_jmin, warp.jmin, amnezia, tr("WARP test: Jmin must be between %1 and %2."))) return false;
        if (!number(phasesUi->warp_jmax, warp.jmax, amnezia, tr("WARP test: Jmax must be between %1 and %2."))) return false;
        QStringList packets;
        for (const auto *packet : warpPackets) packets << packet->text().trimmed();
        while (packets.size() < 5) packets << QString();
        warp.i1 = packets[0];
        warp.i2 = packets[1];
        warp.i3 = packets[2];
        warp.i4 = packets[3];
        warp.i5 = packets[4];
        warp.url = phasesUi->warp_url->text().trimmed();
        if (!number(phasesUi->warp_timeout, warp.timeoutMs, true, tr("WARP test: the timeout must be between %1 and %2 ms.")))
            return false;
        warp.warmLatency = phasesUi->warp_warm->isChecked();
        if (amnezia && warp.jmin > warp.jmax) return fail(tr("WARP test: Jmin must not be larger than Jmax."));
    }

    if (tuneUi != nullptr) {
        auto &tune = out.tune;
        // A control disabled for the selected profile, or by its own switch, keeps its stored value.
        const auto active = [this](const QWidget *control) { return control->isEnabledTo(ui->phases_content); };
        const auto field = [&number, &active](const QLineEdit *edit, int &value, const QString &rangeError) {
            return !active(edit) || number(edit, value, true, rangeError);
        };
        tune.profileId = selectedTuneProfile();
        if (active(tuneUi->fingerprints)) tune.fingerprints = tuneUi->fingerprints->isChecked();
        if (active(tuneUi->fingerprint_list)) tune.fingerprintList = tuneUi->fingerprint_list->text().trimmed();
        if (active(tuneUi->fragment)) tune.fragment = tuneUi->fragment->isChecked();
        if (active(tuneUi->fragment_delays)) tune.fragmentDelays = tuneUi->fragment_delays->text().trimmed();
        if (active(tuneUi->record_fragment)) tune.recordFragment = tuneUi->record_fragment->isChecked();
        if (active(tuneUi->mixed_case_sni)) tune.mixedCaseSni = tuneUi->mixed_case_sni->isChecked();
        if (active(tuneUi->quic_opener)) tune.quicOpener = tuneUi->quic_opener->isChecked();
        if (active(tuneUi->mtu)) tune.mtu = tuneUi->mtu->isChecked();
        if (!field(tuneUi->mtu_min, tune.mtuMin, tr("MTU: the smallest MTU must be between %1 and %2."))) return false;
        if (active(tuneUi->mtu_max) && tuneUi->mtu_max->text().trimmed().isEmpty()) {
            tune.mtuMax = 0;
        } else if (!field(tuneUi->mtu_max, tune.mtuMax,
                          tr("MTU: the largest MTU must be between %1 and %2; leave it empty for the profile's own."))) {
            return false;
        }
        if (!field(tuneUi->mtu_step, tune.mtuStep, tr("MTU: the step must be between %1 and %2."))) return false;
        if (active(tuneUi->mtu_url)) tune.mtuUrl = tuneUi->mtu_url->text().trimmed();
        if (!field(tuneUi->mtu_bytes, tune.mtuBytes, tr("MTU: the download size must be between %1 and %2 bytes."))) return false;
        if (!field(tuneUi->attempts, tune.attempts, tr("Pacing: attempts per variant must be between %1 and %2."))) return false;
        if (!field(tuneUi->per_minute, tune.perMinute, tr("Pacing: attempts per minute must be between %1 and %2."))) return false;
        if (!field(tuneUi->max_attempts, tune.maxAttempts, tr("Pacing: the attempt limit must be between %1 and %2."))) return false;
        if (!field(tuneUi->max_minutes, tune.maxMinutes, tr("Pacing: the time limit must be between %1 and %2 minutes."))) return false;
        tune.url = tuneUi->url->text().trimmed();
        if (!field(tuneUi->timeout, tune.timeoutMs, tr("Pacing: the timeout must be between %1 and %2 ms."))) return false;
        if (const auto problem = Scanner::Tune::CheckOptions(tune); !problem.isEmpty())
            return fail(QStringLiteral("%1: %2").arg(tuneUi->try_box->title(), problem));
    }
    return true;
}

bool DialogScanInstance::save(QString *error) {
    Configs::ScanConfig collected;
    int baseListId = -1;
    QString name;
    if (!collect(collected, baseListId, name, error)) {
        form->markAll();
        return false;
    }
    for (const auto &issue : form->issues()) {
        if (issue.severity != Validation::Severity::Error) continue;
        form->markAll();
        if (error != nullptr) *error = issue.label.isEmpty() ? issue.message : QStringLiteral("%1: %2").arg(issue.label, issue.message);
        return false;
    }
    if (Scanner::ScanManager::instance()->IsRunning(scanId)) {
        if (error != nullptr) *error = tr("Pause the scan before changing its settings.");
        return false;
    }

    // Re-read, so the progress columns written by the last run are kept.
    auto row = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
    if (row == nullptr) {
        if (error != nullptr) *error = tr("The scan no longer exists.");
        return false;
    }
    row->base_list_id = baseListId;
    row->config = collected;
    if (!Configs::dataManager->ipScansRepo->Save(row)) {
        if (error != nullptr) *error = tr("Failed to store the scan.");
        return false;
    }
    config = collected;
    scan = row;
    dirty = false;
    if (row->name != name) {
        Scanner::ScanManager::instance()->RenameScan(scanId, name);
        reloadRow();
    }
    setWindowTitle(tr("%1 - IP Scanner").arg(scan->name));
    refreshRun();
    return true;
}

bool DialogScanInstance::saveIfDirty() {
    if (!dirty) return true;
    if (!form->validateAll()) return false;
    QString error;
    if (save(&error)) return true;
    scanInstanceNotice(this, QMessageBox::Warning, tr("IP Scanner"), error);
    return false;
}

void DialogScanInstance::reject() {
    if (!discarding && dirty) {
        QString error;
        if (!save(&error)) {
            const auto answer = QMessageBox::question(
                this, tr("IP Scanner"), tr("The settings could not be saved:\n%1\n\nClose without saving?").arg(error));
            if (answer != QMessageBox::Yes) return;
            dirty = false;
        }
    }
    QDialog::reject();
}

void DialogScanInstance::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    if (!watching) {
        watching = true;
        ScannerUi::Watch(scanId, true);
    }
    refreshRun();
}

void DialogScanInstance::hideEvent(QHideEvent *event) {
    QDialog::hideEvent(event);
    if (watching) {
        watching = false;
        ScannerUi::Watch(scanId, false);
    }
}

void DialogScanInstance::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    applyLogLines();
}

void DialogScanInstance::changeEvent(QEvent *event) {
    QDialog::changeEvent(event);
    if (event->type() != QEvent::ActivationChange) return;
    if (!isActiveWindow()) {
        profilesStale = true;
        return;
    }
    if (!profilesStale || settingsLocked) return;
    profilesStale = false;
    refreshProfilePickers();
}

void DialogScanInstance::refreshProfilePickers() {
    if (isGeneric()) {
        const bool typing = phasesUi->config_profile->lineEdit()->isModified() && selectedConfigProfile() < 0;
        if (!typing) reloadConfigProfiles(selectedConfigProfile());
    }
    if (isWarp()) reloadWarpProfiles(phasesUi->warp_profile->currentData().toInt());
    if (isResolver()) reloadResolverProfiles(phasesUi->resolver_profile->currentData().toInt());
    if (tuneUi != nullptr) {
        const bool typing = tuneUi->profile->lineEdit()->isModified() && selectedTuneProfile() < 0;
        if (!typing) reloadTuneProfiles(selectedTuneProfile());
    }
}

void DialogScanInstance::reloadBaseLists(int selectId) {
    const QScopedValueRollback guard(loading, true);
    auto *combo = ui->base_list;
    combo->clear();
    baseListNames.clear();
    combo->addItem(tr("Select an IP list…"), -1);
    for (const auto &list : Configs::dataManager->ipListsRepo->GetAllIpLists()) {
        // Scanning its own results would wipe them at the start of the pass.
        if (list->role == Configs::IpList::Role::ScanResult && list->related_test_id == scanId) continue;
        combo->addItem(tr("%1 (%n entries)", nullptr, list->entryCount).arg(list->name), list->id);
        if (list->role == Configs::IpList::Role::User) baseListNames.insert(list->id, list->name);
    }
    scanInstanceSelectData(combo, selectId);
}

void DialogScanInstance::reloadConfigProfiles(int selectId) {
    if (!isGeneric()) return;
    const QScopedValueRollback guard(loading, true);

    auto *profiles = Configs::dataManager->profilesRepo.get();
    QHash<int, QString> idToName;
    for (const auto &[id, profileName] : profiles->GetAllProfileIDNameMapped()) idToName.insert(id, profileName);
    QSet<int> excluded;
    for (const auto &type : kScanInstanceCandidateExclusions) {
        if (Scanner::IsScanBaseType(type)) continue;
        for (const int id : profiles->GetProfileIdsByType(type)) excluded.insert(id);
    }
    const auto xdnsCandidates = profiles->GetProfileIdsByTypeContaining(QStringLiteral("xrayvless"), QStringLiteral("xdns"));
    for (const auto &profile : profiles->GetProfileBatch(xdnsCandidates)) {
        if (!Scanner::IsGenericScanBase(*profile)) excluded.insert(profile->id);
    }

    phasesUi->config_profile->clear();
    configProfileIds.clear();
    phasesUi->config_profile->addItem(tr("None"), -1);
    QStringList names;
    QString selectedName;
    for (const int gid : Configs::dataManager->groupsRepo->GetGroupsTabOrder()) {
        const auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (group == nullptr) continue;
        const QString prefix = QStringLiteral("[") + group->name + QStringLiteral("] ");
        for (const int id : group->profiles) {
            if (excluded.contains(id)) continue;
            const auto found = idToName.constFind(id);
            if (found == idToName.constEnd()) continue;
            const QString display = prefix + found.value();
            names << display;
            if (!configProfileIds.contains(display)) configProfileIds.insert(display, id);
            if (id == selectId) selectedName = display;
            if (phasesUi->config_profile->count() < phasesUi->config_profile->maxCount()) phasesUi->config_profile->addItem(display, id);
        }
    }
    configProfileNames->setStringList(names);

    const int index = phasesUi->config_profile->findData(selectId);
    if (index >= 0) phasesUi->config_profile->setCurrentIndex(index);
    else if (!selectedName.isEmpty()) phasesUi->config_profile->setCurrentText(selectedName);
    else phasesUi->config_profile->setCurrentIndex(0);
}

int DialogScanInstance::selectedConfigProfile() const {
    if (!isGeneric()) return -1;
    const QString text = phasesUi->config_profile->currentText().trimmed();
    if (text.isEmpty() || text == tr("None")) return -1;
    const int index = phasesUi->config_profile->findText(text, Qt::MatchExactly);
    if (index >= 0) return phasesUi->config_profile->itemData(index).toInt();
    return configProfileIds.value(text, -1);
}

void DialogScanInstance::reloadWarpProfiles(int selectId) {
    if (!isWarp()) return;
    const QScopedValueRollback guard(loading, true);
    auto *profiles = Configs::dataManager->profilesRepo.get();
    const bool masque = currentWarpMode() == QStringLiteral("masque");
    const auto ids = profiles->GetProfileIdsByType(masque ? QStringLiteral("masque") : QStringLiteral("wireguard"));

    phasesUi->warp_profile->clear();
    phasesUi->warp_profile->addItem(tr("Select a profile…"), -1);
    for (const auto &[id, profileName] : profiles->GetProfileIDNameMappedBatch(ids)) phasesUi->warp_profile->addItem(profileName, id);
    scanInstanceSelectData(phasesUi->warp_profile, selectId);
}

QString DialogScanInstance::currentWarpMode() const {
    if (!isWarp()) return config.warp.mode;
    return phasesUi->warp_mode->currentData().toString();
}

void DialogScanInstance::reloadResolverProfiles(int selectId) {
    if (!isResolver()) return;
    const QScopedValueRollback guard(loading, true);
    const QString level = phasesUi->resolver_level->currentData().toString();
    auto *profiles = Configs::dataManager->profilesRepo.get();
    QHash<int, QString> protocols;
    for (const auto &type : {QStringLiteral("dnstt"), QStringLiteral("masterdns")}) {
        for (const int id : profiles->GetProfileIdsByType(type)) protocols.insert(id, type);
    }
    const auto xdnsCandidates = profiles->GetProfileIdsByTypeContaining(QStringLiteral("xrayvless"), QStringLiteral("xdns"));
    for (const auto &profile : profiles->GetProfileBatch(xdnsCandidates)) {
        if (const auto protocol = Scanner::DnsTunnelProtocol(*profile); !protocol.isEmpty()) protocols.insert(profile->id, protocol);
    }
    auto ids = protocols.keys();
    std::sort(ids.begin(), ids.end());

    phasesUi->resolver_profile->clear();
    phasesUi->resolver_profile->addItem(tr("Select a profile…"), -1);
    for (const auto &[id, profileName] : profiles->GetProfileIDNameMappedBatch(ids)) {
        phasesUi->resolver_profile->addItem(profileName, id);
        phasesUi->resolver_profile->setItemData(phasesUi->resolver_profile->count() - 1, protocols.value(id), kScanInstanceProtocolRole);
    }
    scanInstanceSelectData(phasesUi->resolver_profile, selectId);
    reloadResolverLevels(level);
}

void DialogScanInstance::reloadResolverLevels(const QString &selectLevel) {
    if (!isResolver()) return;
    const QScopedValueRollback guard(loading, true);
    const QString protocol = currentResolverProtocol();
    const auto levels = Scanner::ResolverScanLevels(protocol);
    phasesUi->resolver_level->clear();
    for (const auto &level : levels) phasesUi->resolver_level->addItem(scanInstanceResolverLevelLabel(level), level);
    scanInstanceSelectData(phasesUi->resolver_level, levels.contains(selectLevel) ? selectLevel : Scanner::ResolverScanDefaultLevel(protocol));
    phasesUi->resolver_level->setEnabled(!levels.isEmpty());
    updateResolverTimeoutHint();
}

QString DialogScanInstance::currentResolverProtocol() const {
    return isResolver() ? phasesUi->resolver_profile->currentData(kScanInstanceProtocolRole).toString() : QString();
}

void DialogScanInstance::updateResolverTimeoutHint() {
    if (!isResolver()) return;
    const QString protocol = currentResolverProtocol();
    phasesUi->resolver_timeout->setPlaceholderText(
        protocol.isEmpty() ? QString()
                           : QString::number(Scanner::ResolverScanDefaultTimeoutMs(protocol, phasesUi->resolver_level->currentData().toString())));
}

void DialogScanInstance::updateHttpControls() {
    if (!isGeneric()) return;
    // setEnabled(true) on a child would override the unchecked group box's own disabling.
    const bool on = phasesUi->http_box->isChecked();
    const bool tls = on && phasesUi->http_tls->isChecked();
    const QString method = phasesUi->http_method->currentData().toString();
    const bool request = on && method != QStringLiteral("NONE");
    for (QWidget *widget : {static_cast<QWidget *>(phasesUi->http_sni), static_cast<QWidget *>(phasesUi->http_alpn),
                            static_cast<QWidget *>(phasesUi->http_min_tls), static_cast<QWidget *>(phasesUi->http_max_tls),
                            static_cast<QWidget *>(phasesUi->http_fingerprint), static_cast<QWidget *>(phasesUi->http_insecure),
                            static_cast<QWidget *>(phasesUi->http_disable_sni), static_cast<QWidget *>(phasesUi->http_fragment),
                            static_cast<QWidget *>(phasesUi->http_record_fragment), static_cast<QWidget *>(phasesUi->http_mixed_case_sni)})
        widget->setEnabled(tls);
    phasesUi->http_fragment_delay->setEnabled(tls && phasesUi->http_fragment->isChecked());
    for (QWidget *widget : {static_cast<QWidget *>(phasesUi->http_host), static_cast<QWidget *>(phasesUi->http_path),
                            static_cast<QWidget *>(phasesUi->http_version)})
        widget->setEnabled(request);
}

void DialogScanInstance::updateWarpControls() {
    if (!isWarp()) return;
    const QString mode = currentWarpMode();
    const bool masque = mode == QStringLiteral("masque");
    const bool amnezia = mode == QStringLiteral("amneziawg");
    const QString identity = phasesUi->warp_identity->currentData().toString();

    scanInstanceSetRowVisible(phasesUi->warp_form, phasesUi->warp_http_mode, masque);
    scanInstanceSetRowVisible(phasesUi->warp_form, phasesUi->warp_sni, masque);
    for (QWidget *junk : {static_cast<QWidget *>(phasesUi->warp_jc), static_cast<QWidget *>(phasesUi->warp_jmin), static_cast<QWidget *>(phasesUi->warp_jmax)})
        scanInstanceSetRowVisible(phasesUi->warp_form, junk, amnezia);
    for (auto *packet : warpPackets) scanInstanceSetRowVisible(phasesUi->warp_form, packet, amnezia);
    scanInstanceSetRowVisible(phasesUi->warp_form, phasesUi->warp_register_row, identity == QStringLiteral("generated"));
    scanInstanceSetRowVisible(phasesUi->warp_form, phasesUi->warp_profile, identity == QStringLiteral("profile"));
    updateWarpIdentityStatus();
}

void DialogScanInstance::updateWarpIdentityStatus() {
    if (!isWarp()) return;
    const auto &warp = config.warp;
    const QString transport = Scanner::WarpTransportOf(currentWarpMode());
    QString text;
    if (registering) {
        text = tr("Registering a WARP device…");
    } else if (warp.generatedIdentity.isEmpty()) {
        text = tr("No device is registered for this scan yet.");
    } else if (warp.generatedMode != transport) {
        const QString registered = warp.generatedMode == QStringLiteral("masque") ? QStringLiteral("MASQUE")
                                                                                 : QStringLiteral("WireGuard");
        text = tr("The registered device is for %1; register a new one for this mode.").arg(registered);
    } else {
        text = tr("A WARP device is registered for this scan.");
    }
    phasesUi->warp_identity_status->setText(text);
    phasesUi->warp_register->setText(warp.generatedIdentity.isEmpty() ? tr("Register") : tr("Register again"));
    phasesUi->warp_register->setEnabled(!registering);
}

void DialogScanInstance::useBuiltinWarpRanges() {
    const bool masque = currentWarpMode() == QStringLiteral("masque");
    const int listId = Scanner::DefaultIpLists::Ensure(masque ? Scanner::DefaultIpLists::kWarpMasqueName
                                                              : Scanner::DefaultIpLists::kWarpWireGuardName);
    if (listId < 0) {
        MessageBoxWarning(tr("IP Scanner"), tr("Failed to store the IP list."));
        return;
    }
    Scanner::IpListUpdater::instance()->NotifyListsChanged();

    reloadBaseLists(listId);
    ui->ports->setText(scanInstanceJoinPorts(masque ? Scanner::WarpPresets::kMasquePorts : Scanner::WarpPresets::kWireGuardPorts));
    markDirty();
}

void DialogScanInstance::matchWarpTargets() {
    if (loading || !isWarp()) return;
    const bool masque = currentWarpMode() == QStringLiteral("masque");
    const QString listName = baseListNames.value(ui->base_list->currentData().toInt());
    const auto ports = scanInstanceParsePorts(ui->ports->text(), nullptr);

    // Only targets still at the other mode's presets are swapped.
    const bool stale = masque ? listName == Scanner::DefaultIpLists::kWarpWireGuardName &&
                                    (scanInstanceSamePorts(ports, Scanner::WarpPresets::kWireGuardPorts) ||
                                     scanInstanceSamePorts(ports, Scanner::WarpPresets::kWireGuardAllPorts))
                              : listName == Scanner::DefaultIpLists::kWarpMasqueName &&
                                    scanInstanceSamePorts(ports, Scanner::WarpPresets::kMasquePorts);
    if (stale) useBuiltinWarpRanges();
}

void DialogScanInstance::fillPortPresets(QMenu *menu) {
    menu->clear();
    const auto addPreset = [this, menu](const QString &label, const QList<int> &ports) {
        connect(menu->addAction(label), &QAction::triggered, this,
                [this, ports] { ui->ports->setText(scanInstanceJoinPorts(ports)); });
    };
    if (currentWarpMode() == QStringLiteral("masque")) {
        addPreset(tr("MASQUE: %1").arg(scanInstanceJoinPorts(Scanner::WarpPresets::kMasquePorts)),
                  Scanner::WarpPresets::kMasquePorts);
        return;
    }
    addPreset(tr("WireGuard: %1").arg(scanInstanceJoinPorts(Scanner::WarpPresets::kWireGuardPorts)),
              Scanner::WarpPresets::kWireGuardPorts);
    addPreset(tr("WireGuard, all %n known ports", nullptr, static_cast<int>(Scanner::WarpPresets::kWireGuardAllPorts.size())),
              Scanner::WarpPresets::kWireGuardAllPorts);
}

void DialogScanInstance::registerWarpIdentity() {
    if (registering) return;
    if (!Configs_network::ConfirmWarpTerms(this)) return;

    const QString transport = Scanner::WarpTransportOf(currentWarpMode());
    registering = true;
    updateWarpIdentityStatus();
    refreshRun();

    QPointer<DialogScanInstance> self(this);
    runOnNewThread([self, transport] {
        QString error;
        const auto identity = Configs_network::RegisterWarp(transport, &error);
        runOnUiThread([self, transport, identity, error] {
            if (self == nullptr) return;
            self->applyWarpRegistration(transport, identity, error);
        });
    });
}

void DialogScanInstance::applyWarpRegistration(const QString &transport,
                                               const std::shared_ptr<Configs_network::WarpIdentity> &identity,
                                               const QString &error) {
    registering = false;
    if (identity == nullptr) {
        updateWarpIdentityStatus();
        refreshRun();
        MessageBoxWarning(tr("WARP registration failed"), error.isEmpty() ? tr("Unknown error") : error);
        return;
    }

    config.warp.generatedIdentity = Scanner::WarpIdentityToBeanJson(transport, *identity);
    config.warp.generatedMode = transport;
    dirty = true;
    updateWarpIdentityStatus();
    if (!Scanner::ScanManager::instance()->IsRunning(scanId)) {
        QString saveError;
        if (!save(&saveError)) scanInstanceNotice(this, QMessageBox::Warning, tr("IP Scanner"), saveError);
    }
    refreshRun();
}

void DialogScanInstance::reloadRow() {
    if (auto row = Configs::dataManager->ipScansRepo->GetIpScan(scanId)) scan = row;
    resultCount = ScannerUi::ResultCount(*scan);
}

void DialogScanInstance::refreshRun() {
    auto *manager = Scanner::ScanManager::instance();
    const bool running = manager->IsRunning(scanId);
    const auto live = running ? manager->LiveState(scanId) : ScannerUi::RowState(*scan);

    if (running && live.total == 0) {
        ui->progress->setRange(0, 0);
    } else {
        ui->progress->setRange(0, 1000);
        ui->progress->setValue(live.total > 0 ? static_cast<int>(qMin<quint64>(1000, live.tested * 1000 / live.total)) : 0);
    }

    bool error = false;
    ui->status->setText(ScannerUi::StatusLine(*scan, live, true, &error));
    const QString sheet = error ? QStringLiteral("color: %1;").arg(themeManager()->tokens.danger.name()) : QString();
    // setStyleSheet repolishes even when the sheet is unchanged.
    if (ui->status->styleSheet() != sheet) ui->status->setStyleSheet(sheet);

    // A stopped scan has no live log; the last lines stay up until the next run.
    if (running || !live.log.isEmpty()) {
        logLines = live.log;
        applyLogLines();
    }

    const bool resumable = !running && ScannerUi::IsResumable(*scan);
    const bool idle = !running && !registering && !resultsBusy;
    ui->start_button->setEnabled(idle);
    ui->pause_button->setText(resumable ? tr("Resume") : tr("Pause"));
    ui->pause_button->setEnabled(running ? !live.stopping : idle && resumable);
    applyLock(running);
}

void DialogScanInstance::applyLock(bool locked) {
    settingsLocked = locked;
    ui->name->setEnabled(!locked);
    ui->targets_content->setEnabled(!locked);
    ui->phases_content->setEnabled(!locked);
    if (isTune()) {
        updateTuneActions();
        return;
    }
    const bool hasResults = !resultsBusy && resultCount > 0 && resultsModel->rowCount() > 0;
    ui->remove_results->setEnabled(!locked && hasResults);
    ui->save_as_list->setEnabled(hasResults);
    ui->copy_results->setEnabled(hasResults);
    ui->export_results->setEnabled(hasResults);
    if (useAsResolvers != nullptr) useAsResolvers->setEnabled(hasResults);
}

void DialogScanInstance::applyLogLines() {
    const QList<QLabel *> labels = {ui->log_0, ui->log_1, ui->log_2};
    const qsizetype first = qMax<qsizetype>(0, logLines.size() - labels.size());
    for (qsizetype i = 0; i < labels.size(); ++i) {
        auto *label = labels.at(i);
        const qsizetype index = first + i;
        const QString text = index < logLines.size() ? logLines.at(index) : QString();
        // QLabel clips rather than elides.
        label->setText(label->fontMetrics().elidedText(text, Qt::ElideRight, label->width()));
        label->setToolTip(text);
    }
}

void DialogScanInstance::scheduleResultsReload() {
    resultsStale = true;
    if (ui->tabs->currentWidget() != ui->tab_results) return;
    if (!resultsReload->isActive()) resultsReload->start();
}

void DialogScanInstance::reloadResults() {
    resultsReload->stop();
    resultsStale = false;
    if (isTune()) {
        reloadTuneResults();
        return;
    }
    ++resultsGeneration;
    // The load in flight sees the newer generation when it lands and starts this one then.
    if (resultsLoading) return;
    if (auto row = Configs::dataManager->ipScansRepo->GetIpScan(scanId)) scan = row;

    const int listId = scan->result_list_id;
    const bool running = Scanner::ScanManager::instance()->IsRunning(scanId);
    const int knownCount = running && listId == resultsLoadedListId ? resultsLoadedCount : -1;
    const quint64 generation = resultsGeneration;
    resultsLoading = true;
    QPointer<DialogScanInstance> self(this);
    runOnNewThread([self, generation, listId, knownCount] {
        auto *repo = Configs::dataManager->ipListsRepo.get();
        int count = listId >= 0 ? repo->EntryCount(listId) : 0;
        const bool unchanged = count == knownCount;
        QList<Configs::IpListEntry> entries;
        if (!unchanged && count > 0) {
            entries = repo->GetEntries(listId, 0, kScanInstanceResultsPreview);
            const auto loaded = static_cast<int>(entries.size());
            count = loaded < kScanInstanceResultsPreview ? loaded : qMax(count, loaded);
        }
        runOnUiThread([self, generation, listId, count, unchanged, entries] {
            if (self == nullptr) return;
            self->resultsLoading = false;
            if (self->resultsGeneration != generation) {
                self->reloadResults();
                return;
            }
            if (!unchanged) self->applyResults(listId, count, entries);
        });
    });
}

void DialogScanInstance::applyResults(int listId, int count, const QList<Configs::IpListEntry> &entries) {
    resultsLoadedListId = listId;
    resultsLoadedCount = count;
    resultCount = count;
    resultsModel->setEntries(entries);
    applyResultColumns();
    QString summary;
    if (count == 0) summary = tr("No results yet.");
    else if (entries.size() < count) summary = tr("Showing the first %1 of %2 results").arg(entries.size()).arg(count);
    else summary = tr("%n result(s)", nullptr, count);
    ui->results_summary->setText(summary);
    refreshRun();
}

void DialogScanInstance::applyResultColumns() {
    for (const int column : {IpEntriesModel::MaxQNameColumn, IpEntriesModel::UploadMtuColumn, IpEntriesModel::DownloadMtuColumn})
        ui->results_view->setColumnHidden(column, !resultsModel->columnHasData(column));
}

void DialogScanInstance::runResultsTask(const std::function<void()> &work, const std::function<void()> &done) {
    resultsBusy = true;
    refreshRun();
    QPointer<DialogScanInstance> self(this);
    runOnNewThread([self, work, done] {
        work();
        runOnUiThread([self, done] {
            if (self == nullptr) return;
            self->resultsBusy = false;
            done();
            if (self != nullptr) self->refreshRun();
        });
    });
}

QList<Configs::IpListEntry> DialogScanInstance::selectedResults() const {
    QList<int> rows;
    for (const auto &index : ui->results_view->selectionModel()->selectedRows()) rows << index.row();
    std::sort(rows.begin(), rows.end());
    QList<Configs::IpListEntry> entries;
    entries.reserve(rows.size());
    for (const int row : rows) {
        const auto source = resultsProxy->mapToSource(resultsProxy->index(row, 0));
        if (source.isValid()) entries << resultsModel->entries().at(source.row());
    }
    return entries;
}

void DialogScanInstance::saveResultsAsList() {
    if (scan->result_list_id < 0 || resultCount == 0 || resultsBusy) return;
    bool ok = false;
    const auto name = QInputDialog::getText(this, tr("Save as IP list"), tr("Name of the new IP list:"), QLineEdit::Normal,
                                            tr("%1 results").arg(scan->name), &ok)
                          .trimmed();
    if (!ok || name.isEmpty() || resultsBusy) return;

    const int listId = scan->result_list_id;
    const int expected = resultCount;
    auto saved = std::make_shared<int>(-1);
    runResultsTask(
        [listId, name, saved] {
            if (const auto copy = Configs::dataManager->ipListsRepo->CopyAsUserList(listId, name)) *saved = copy->entryCount;
        },
        [this, name, saved, expected] {
            if (*saved < 0) {
                scanInstanceNotice(this, QMessageBox::Warning, tr("Save as IP list"), tr("Failed to store the IP list."));
                return;
            }
            Scanner::IpListUpdater::instance()->NotifyListsChanged();
            scanInstanceNotice(this, QMessageBox::Information, tr("Save as IP list"),
                               tr("Saved %n entries to the IP list \"%1\".", nullptr, *saved > 0 ? *saved : expected).arg(name));
        });
}

void DialogScanInstance::useResultsAsResolvers() {
    reloadRow();
    const int listId = scan->result_list_id;
    if (listId < 0 || resultCount == 0) return;
    const auto title = tr("Use as resolvers");
    const int profileId = scan->config.resolver.profileId;
    const auto profile = profileId >= 0 ? Configs::dataManager->profilesRepo->GetProfile(profileId) : nullptr;
    auto *source = profile != nullptr && profile->outbound != nullptr ? Configs::ProfileResolverSource(*profile) : nullptr;
    if (source == nullptr) {
        scanInstanceNotice(this, QMessageBox::Warning, title, tr("The scan's profile was deleted or is no longer a DNS tunnel profile."));
        return;
    }
    const auto list = Configs::dataManager->ipListsRepo->GetIpListHeader(listId);
    const QString listName = list != nullptr ? list->name : QString::number(listId);
    const QString profileName = profile->outbound->name;

    if (Engine::IsBeingEdited(Engine::EditTarget::Profile, profileId)) {
        // The open editor saves its own list choice back on OK, so the choice goes to the editor.
        int editors = 0;
        for (auto *widget : QApplication::allWidgets()) {
            auto *sourceWidget = qobject_cast<ResolverSourceWidget *>(widget);
            if (sourceWidget == nullptr || sourceWidget->profileId() != profileId) continue;
            sourceWidget->setIpList(listId);
            ++editors;
        }
        if (editors == 0) {
            scanInstanceNotice(this, QMessageBox::Warning, title, tr("\"%1\" is open in an editor; close it and try again.").arg(profileName));
            return;
        }
        scanInstanceNotice(this, QMessageBox::Information, title,
                           tr("The open editor of \"%1\" now takes resolvers from the IP list \"%2\"; press OK there to save it.")
                               .arg(profileName, listName));
        return;
    }

    if (source->ipListId != listId) {
        const int previous = source->ipListId;
        source->ipListId = listId;
        if (!Configs::dataManager->profilesRepo->Save(profile)) {
            source->ipListId = previous;
            scanInstanceNotice(this, QMessageBox::Warning, title, tr("Failed to store the profile."));
            return;
        }
        QStringList args;
        if (Configs::dataManager->settingsRepo->started_id == profileId || Configs::RunningUsesProfile(profileId))
            args << MwArg::RestartProxy;
        Configs::RepositionLater(QList<std::shared_ptr<Configs::Profile>>{profile}, Configs::SortInput::Config);
        MW_dialog_message(MwMessage::ProfileChanged, args);
    }
    scanInstanceNotice(this, QMessageBox::Information, title,
                       tr("\"%1\" now takes resolvers from the IP list \"%2\".").arg(profileName, listName));
}

void DialogScanInstance::copyResults() {
    const auto selected = selectedResults();
    if (!selected.isEmpty()) {
        QStringList lines;
        lines.reserve(selected.size());
        for (const auto &entry : selected) lines << Scanner::FormatEntry(entry);
        QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
        QToolTip::showText(QCursor::pos(), tr("Copied %n entries", nullptr, static_cast<int>(selected.size())), ui->copy_results);
        return;
    }
    if (scan->result_list_id < 0 || resultCount == 0 || resultsBusy) return;

    const int listId = scan->result_list_id;
    const int column = resultsProxy->sortColumn();
    const Qt::SortOrder order = resultsProxy->sortOrder();
    auto text = std::make_shared<QString>();
    auto copied = std::make_shared<int>(0);
    runResultsTask(
        [listId, column, order, text, copied] {
            auto entries = Configs::dataManager->ipListsRepo->GetEntries(listId);
            scanInstanceSortEntries(entries, column, order);
            QStringList lines;
            lines.reserve(entries.size());
            for (const auto &entry : entries) lines << Scanner::FormatEntry(entry);
            *text = lines.join(QLatin1Char('\n'));
            *copied = static_cast<int>(entries.size());
        },
        [this, text, copied] {
            if (*copied == 0) return;
            QGuiApplication::clipboard()->setText(*text);
            QToolTip::showText(QCursor::pos(), tr("Copied %n entries", nullptr, *copied), ui->copy_results);
        });
}

void DialogScanInstance::exportResults() {
    if (scan->result_list_id < 0 || resultCount == 0 || resultsBusy) return;
    QString fileName = scan->name;
    fileName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const auto path = NativeFileDialog::getSaveFileName(this, tr("Export results"), QDir::homePath() + QLatin1Char('/') + fileName + QStringLiteral(".txt"),
                                                        tr("Text files (*.txt);;All files (*)"));
    if (path.isEmpty() || resultsBusy) return;

    const int listId = scan->result_list_id;
    const int column = resultsProxy->sortColumn();
    const Qt::SortOrder order = resultsProxy->sortOrder();
    auto failed = std::make_shared<bool>(false);
    runResultsTask(
        [listId, column, order, path, failed] {
            auto entries = Configs::dataManager->ipListsRepo->GetEntries(listId);
            scanInstanceSortEntries(entries, column, order);
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                *failed = true;
                return;
            }
            for (const auto &entry : entries) file.write((Scanner::FormatEntry(entry) + QLatin1Char('\n')).toUtf8());
            *failed = !file.flush() || file.error() != QFileDevice::NoError;
            file.close();
        },
        [this, path, failed] {
            if (*failed)
                scanInstanceNotice(this, QMessageBox::Warning, tr("Export results"), tr("Cannot write to: %1").arg(path));
        });
}

void DialogScanInstance::removeSelectedResults() {
    if (scan->result_list_id < 0 || resultsBusy || Scanner::ScanManager::instance()->IsRunning(scanId)) return;
    const auto entries = selectedResults();
    if (entries.isEmpty()) return;
    const int listId = scan->result_list_id;
    const int id = scanId;
    runResultsTask(
        [listId, id, entries] {
            Configs::dataManager->ipListsRepo->RemoveEntries(listId, entries);
            runOnUiThread([id] { scanInstanceSyncFoundCount(id); });
        },
        [this] { reloadResults(); });
}

void DialogScanInstance::startClicked() {
    reloadRow();
    // A settings search has nothing to rescan, so Start always starts over.
    if (resultCount <= 0 || isTune()) {
        startFromBeginning();
        return;
    }
    // A popup, not exec(): a nested loop would outlive this window if its scan were deleted meanwhile.
    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu->addAction(tr("Scan from the beginning")), &QAction::triggered, this, [this] { startFromBeginning(); });
    connect(menu->addAction(tr("Rescan current results")), &QAction::triggered, this, [this] { rescanClicked(); });
    menu->popup(ui->start_button->mapToGlobal(QPoint(0, ui->start_button->height())));
}

void DialogScanInstance::startFromBeginning() {
    if (!saveIfDirty()) return;
    reloadRow();
    if (!ScannerUi::ConfirmWipe(this, *scan)) return;
    ScannerUi::StartScan(this, scanId, Scanner::StartMode::FromInitial);
    reloadRow();
    refreshRun();
}

void DialogScanInstance::pauseClicked() {
    auto *manager = Scanner::ScanManager::instance();
    if (manager->IsRunning(scanId)) {
        manager->Pause(scanId);
        refreshRun();
        return;
    }
    if (!saveIfDirty()) return;
    reloadRow();
    if (ScannerUi::IsResumable(*scan)) ScannerUi::StartScan(this, scanId, Scanner::StartMode::Resume);
    reloadRow();
    refreshRun();
}

void DialogScanInstance::rescanClicked() {
    if (!saveIfDirty()) return;
    ScannerUi::StartScan(this, scanId, Scanner::StartMode::RescanResults);
    reloadRow();
    refreshRun();
}
