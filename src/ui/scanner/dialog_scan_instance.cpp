#include "include/ui/scanner/dialog_scan_instance.h"

#include <QAbstractItemView>
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
#include <QHBoxLayout>
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

#include "include/configs/common/TLS.h"
#include "include/configs/sub/warp.h"
#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Utils.hpp"
#include "include/global/Validation.hpp"
#include "include/scanner/DefaultIpLists.h"
#include "include/scanner/IpListParse.h"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanManager.h"
#include "include/scanner/ScanProfiles.h"
#include "include/scanner/WarpPresets.h"
#include "include/ui/mainwindow.h"
#include "include/ui/scanner/IpEntriesModel.h"
#include "include/ui/scanner/ScanItem.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/utils/NoWheelComboBox.h"
#include "include/ui/utils/ValidationForm.h"

namespace {
    constexpr int kScanInstanceResultsPreview = 50000;

    const QStringList kScanInstanceCandidateExclusions = {
        QStringLiteral("custom"), QStringLiteral("extracore"), QStringLiteral("tailscale"),
        QStringLiteral("autoselector"), QStringLiteral("direct"),
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

    QLineEdit *scanInstanceNumberEdit(QWidget *parent, int minimum, int maximum, int defaultValue,
                                      const QStringList &presets = {}) {
        auto *edit = new QLineEdit(parent);
        scanInstanceSetupNumber(edit, minimum, maximum, QString::number(defaultValue), presets);
        return edit;
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

    QLabel *scanInstanceHint(const QString &text, QWidget *parent) {
        auto *label = new QLabel(text, parent);
        label->setWordWrap(true);
        label->setProperty("colorRole", QStringLiteral("muted"));
        return label;
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
    delete ui;
}

bool DialogScanInstance::isWarp() const {
    return scan->kind == Configs::IpScan::Kind::Warp;
}

void DialogScanInstance::setupTargets() {
    using PortMode = Configs::ScanConfig::PortMode;
    ui->port_mode->addItem(tr("Ignore the list's ports"), static_cast<int>(PortMode::Ignore));
    ui->port_mode->addItem(tr("Merge with the list's ports"), static_cast<int>(PortMode::Merge));
    ui->port_mode->addItem(tr("Only the list's ports"), static_cast<int>(PortMode::List));
    ui->port_mode->setItemData(
        2, tr("Your ports are ignored; entries without a port are probed on port 443 (80 for plain HTTP)."), Qt::ToolTipRole);
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
    ui->use_warp_ranges->setToolTip(
        tr("Selects an IP list of Cloudflare's WARP ranges for the chosen mode, creating it when missing, and sets the matching ports."));
}

void DialogScanInstance::buildPhases() {
    auto *layout = ui->phases_layout;
    buildIcmpPhase(layout);
    if (isWarp()) {
        buildWarpPhase(layout);
    } else {
        buildTcpPhase(layout);
        buildHttpPhase(layout);
        buildConfigPhase(layout);
    }
    layout->addStretch(1);
}

void DialogScanInstance::buildIcmpPhase(QVBoxLayout *layout) {
    icmpBox = new QGroupBox(tr("ICMP (ping)"), ui->phases_content);
    icmpBox->setCheckable(true);
    auto *form = new QFormLayout(icmpBox);
    icmpTimeout = scanInstanceNumberEdit(icmpBox, 100, 60000, defaults.icmp.timeoutMs, kScanInstanceTimeoutPresets);
    form->addRow(tr("Timeout (ms)"), icmpTimeout);
    icmpCount = scanInstanceNumberEdit(icmpBox, 1, 10, defaults.icmp.count);
    form->addRow(tr("Echo requests"), icmpCount);
    layout->addWidget(icmpBox);
}

void DialogScanInstance::buildTcpPhase(QVBoxLayout *layout) {
    tcpBox = new QGroupBox(tr("TCP connect"), ui->phases_content);
    tcpBox->setCheckable(true);
    auto *form = new QFormLayout(tcpBox);
    tcpTimeout = scanInstanceNumberEdit(tcpBox, 100, 60000, defaults.tcp.timeoutMs, kScanInstanceTimeoutPresets);
    form->addRow(tr("Timeout (ms)"), tcpTimeout);
    tcpAttempts = scanInstanceNumberEdit(tcpBox, 1, 10, defaults.tcp.attempts);
    form->addRow(tr("Attempts"), tcpAttempts);
    layout->addWidget(tcpBox);
}

void DialogScanInstance::buildHttpPhase(QVBoxLayout *layout) {
    httpBox = new QGroupBox(tr("TLS / HTTP"), ui->phases_content);
    httpBox->setCheckable(true);
    httpForm = new QFormLayout(httpBox);
    httpTls = new QCheckBox(tr("Use TLS"), httpBox);
    httpForm->addRow(httpTls);
    httpSni = new QLineEdit(httpBox);
    httpSni->setPlaceholderText(tr("The target address"));
    httpForm->addRow(tr("SNI"), httpSni);
    httpHost = new QLineEdit(httpBox);
    httpHost->setPlaceholderText(tr("The SNI, or the target address"));
    httpForm->addRow(tr("Host header"), httpHost);
    httpPath = new QLineEdit(httpBox);
    httpPath->setPlaceholderText(QStringLiteral("/"));
    httpForm->addRow(tr("Path"), httpPath);
    httpMethod = new NoWheelComboBox(httpBox);
    httpMethod->addItem(QStringLiteral("GET"), QStringLiteral("GET"));
    httpMethod->addItem(QStringLiteral("HEAD"), QStringLiteral("HEAD"));
    httpMethod->addItem(tr("None (TLS handshake only)"), QStringLiteral("NONE"));
    httpMethod->setToolTip(tr("A request passes only when the answer is 200 OK."));
    httpForm->addRow(tr("Method"), httpMethod);
    httpVersion = new NoWheelComboBox(httpBox);
    httpVersion->addItem(QStringLiteral("HTTP/1.1"), QStringLiteral("1.1"));
    httpVersion->addItem(QStringLiteral("HTTP/2"), QStringLiteral("2"));
    httpVersion->addItem(QStringLiteral("HTTP/3"), QStringLiteral("3"));
    httpForm->addRow(tr("HTTP version"), httpVersion);
    httpAlpn = new QLineEdit(httpBox);
    httpAlpn->setPlaceholderText(tr("From the HTTP version"));
    httpForm->addRow(tr("ALPN"), httpAlpn);
    httpMinTls = new NoWheelComboBox(httpBox);
    httpMaxTls = new NoWheelComboBox(httpBox);
    for (auto *combo : {httpMinTls, httpMaxTls}) {
        combo->addItem(tr("Default"), QString());
        for (const auto &version : {QStringLiteral("1.0"), QStringLiteral("1.1"), QStringLiteral("1.2"), QStringLiteral("1.3")})
            combo->addItem(QStringLiteral("TLS ") + version, version);
    }
    httpForm->addRow(tr("Minimum TLS version"), httpMinTls);
    httpForm->addRow(tr("Maximum TLS version"), httpMaxTls);
    httpFingerprint = new NoWheelComboBox(httpBox);
    for (const auto &fingerprint : Configs::tlsFingerprints) {
        // the probe's Default already is Go's own TLS stack
        if (fingerprint == Configs::tlsUnsafeFingerprint) continue;
        httpFingerprint->addItem(fingerprint.isEmpty() ? tr("Default") : fingerprint, fingerprint);
    }
    httpForm->addRow(tr("uTLS fingerprint"), httpFingerprint);
    httpInsecure = new QCheckBox(tr("Allow insecure certificates"), httpBox);
    httpForm->addRow(httpInsecure);
    httpDisableSni = new QCheckBox(tr("Disable SNI"), httpBox);
    httpForm->addRow(httpDisableSni);
    httpFragment = new QCheckBox(tr("Fragment the TLS handshake"), httpBox);
    httpForm->addRow(httpFragment);
    httpFragmentDelay = scanInstanceNumberEdit(httpBox, 0, 10000, defaults.http.fragmentFallbackDelayMs);
    httpForm->addRow(tr("Fragment fallback delay (ms)"), httpFragmentDelay);
    httpRecordFragment = new QCheckBox(tr("Fragment TLS records"), httpBox);
    httpForm->addRow(httpRecordFragment);
    httpMixedCaseSni = new QCheckBox(tr("Mixed-case SNI"), httpBox);
    httpForm->addRow(httpMixedCaseSni);
    httpTimeout = scanInstanceNumberEdit(httpBox, 100, 60000, defaults.http.timeoutMs, kScanInstanceTimeoutPresets);
    httpForm->addRow(tr("Timeout (ms)"), httpTimeout);
    layout->addWidget(httpBox);

    connect(httpBox, &QGroupBox::toggled, this, [this] { updateHttpControls(); });
    connect(httpTls, &QCheckBox::toggled, this, [this] { updateHttpControls(); });
    connect(httpFragment, &QCheckBox::toggled, this, [this] { updateHttpControls(); });
    connect(httpMethod, &QComboBox::currentIndexChanged, this, [this] { updateHttpControls(); });
}

void DialogScanInstance::buildConfigPhase(QVBoxLayout *layout) {
    configBox = new QGroupBox(tr("Config test"), ui->phases_content);
    configBox->setCheckable(true);
    auto *form = new QFormLayout(configBox);
    form->addRow(scanInstanceHint(
        tr("Candidates that pass the probes are tested through a copy of this profile pointed at each of them."), configBox));

    configProfile = new NoWheelComboBox(configBox);
    configProfile->setEditable(true);
    configProfile->setInsertPolicy(QComboBox::NoInsert);
    configProfile->setMaxCount(200);
    configProfile->setCompleter(nullptr);
    configProfileNames = new QStringListModel(this);
    auto *completer = new QCompleter(this);
    completer->setModel(configProfileNames);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    scanInstanceAttachCompleter(this, configProfile, completer);
    form->addRow(tr("Profile"), configProfile);

    configUrl = new QLineEdit(configBox);
    configUrl->setPlaceholderText(Configs::dataManager->settingsRepo->test_latency_url);
    form->addRow(tr("Test URL"), configUrl);
    configTimeout = scanInstanceNumberEdit(configBox, 100, 60000, defaults.config.timeoutMs, kScanInstanceTimeoutPresets);
    form->addRow(tr("Timeout (ms)"), configTimeout);
    configWarm = new QCheckBox(tr("Report warm latency (two requests per target)"), configBox);
    form->addRow(configWarm);
    layout->addWidget(configBox);
}

void DialogScanInstance::buildWarpPhase(QVBoxLayout *layout) {
    warpBox = new QGroupBox(tr("WARP test"), ui->phases_content);
    warpForm = new QFormLayout(warpBox);
    warpForm->addRow(scanInstanceHint(
        tr("Candidates that pass the probes are tested by connecting to WARP through them."), warpBox));

    warpMode = new NoWheelComboBox(warpBox);
    warpMode->addItem(QStringLiteral("WireGuard"), QStringLiteral("wireguard"));
    warpMode->addItem(QStringLiteral("AmneziaWG"), QStringLiteral("amneziawg"));
    warpMode->addItem(QStringLiteral("MASQUE"), QStringLiteral("masque"));
    warpForm->addRow(tr("Mode"), warpMode);

    warpHttpMode = new NoWheelComboBox(warpBox);
    warpHttpMode->addItem(tr("HTTP/3, falling back to HTTP/2"), 0);
    warpHttpMode->addItem(tr("HTTP/3 only"), 1);
    warpHttpMode->addItem(tr("HTTP/2 only"), 2);
    warpForm->addRow(tr("MASQUE transport"), warpHttpMode);

    warpIdentity = new NoWheelComboBox(warpBox);
    warpIdentity->addItem(tr("Dedicated for this scan (recommended)"), QStringLiteral("generated"));
    warpIdentity->addItem(tr("From a profile"), QStringLiteral("profile"));
    warpIdentity->addItem(tr("Built-in WARP settings"), QStringLiteral("builtin"));
    warpForm->addRow(tr("Identity"), warpIdentity);

    warpRegisterRow = new QWidget(warpBox);
    auto *registerLayout = new QHBoxLayout(warpRegisterRow);
    registerLayout->setContentsMargins(0, 0, 0, 0);
    warpIdentityStatus = new QLabel(warpRegisterRow);
    warpIdentityStatus->setTextFormat(Qt::PlainText);
    warpIdentityStatus->setWordWrap(true);
    registerLayout->addWidget(warpIdentityStatus, 1);
    warpRegister = new QPushButton(tr("Register"), warpRegisterRow);
    registerLayout->addWidget(warpRegister, 0);
    warpForm->addRow(tr("Device"), warpRegisterRow);
    if (auto *label = qobject_cast<QLabel *>(warpForm->labelForField(warpRegisterRow))) label->setBuddy(warpRegister);

    warpProfile = new NoWheelComboBox(warpBox);
    warpForm->addRow(tr("Profile"), warpProfile);

    warpMtu = scanInstanceNumberEdit(warpBox, 576, 9000, defaults.warp.mtu, kScanInstanceMtuPresets);
    warpForm->addRow(tr("MTU"), warpMtu);
    warpSni = new QLineEdit(warpBox);
    warpSni->setPlaceholderText(QStringLiteral("consumer-masque.cloudflareclient.com"));
    warpForm->addRow(tr("SNI"), warpSni);

    warpJc = scanInstanceNumberEdit(warpBox, 0, 128, defaults.warp.jc);
    warpForm->addRow(tr("Junk packets (Jc)"), warpJc);
    warpJmin = scanInstanceNumberEdit(warpBox, 0, 1280, defaults.warp.jmin);
    warpForm->addRow(tr("Junk minimum size (Jmin)"), warpJmin);
    warpJmax = scanInstanceNumberEdit(warpBox, 0, 1280, defaults.warp.jmax);
    warpForm->addRow(tr("Junk maximum size (Jmax)"), warpJmax);
    for (int i = 1; i <= 5; ++i) {
        auto *packet = new QLineEdit(warpBox);
        warpPackets.append(packet);
        warpForm->addRow(QStringLiteral("I%1").arg(i), packet);
    }

    warpUrl = new QLineEdit(warpBox);
    warpUrl->setPlaceholderText(Configs::dataManager->settingsRepo->test_latency_url);
    warpForm->addRow(tr("Test URL"), warpUrl);
    warpTimeout = scanInstanceNumberEdit(warpBox, 100, 60000, defaults.warp.timeoutMs, kScanInstanceTimeoutPresets);
    warpForm->addRow(tr("Timeout (ms)"), warpTimeout);
    warpWarm = new QCheckBox(tr("Report warm latency (two requests per target)"), warpBox);
    warpForm->addRow(warpWarm);
    layout->addWidget(warpBox);

    connect(warpMode, &QComboBox::currentIndexChanged, this, [this] {
        reloadWarpProfiles(warpProfile->currentData().toInt());
        updateWarpControls();
        matchWarpTargets();
    });
    connect(warpIdentity, &QComboBox::currentIndexChanged, this, [this] { updateWarpControls(); });
    connect(warpRegister, &QPushButton::clicked, this, [this] { registerWarpIdentity(); });
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
    for (QLineEdit *edit : {ui->concurrency, ui->spawn_interval, icmpTimeout, icmpCount, tcpTimeout, tcpAttempts, httpFragmentDelay,
                            httpTimeout, configTimeout, warpMtu, warpJc, warpJmin, warpJmax, warpTimeout}) {
        if (edit != nullptr) form->bind(edit, [edit](const QString &) { return scanInstanceNumberError(edit); });
    }

    if (httpBox != nullptr) {
        form->bind(httpSni, Rules::Host(Empty::Allowed));
        form->bind(httpHost, Rules::HostPort(false, Empty::Allowed));
        form->bind(httpPath, [](const QString &text) { return scanInstancePathError(text); });
        form->addRule({httpMinTls, httpMaxTls}, [this] {
            return scanInstanceTlsRangeError(httpMinTls->currentData().toString(), httpMaxTls->currentData().toString());
        }, this, Severity::Error, httpBox->title());
        form->addRule({httpAlpn, httpVersion}, [this] {
            if (!httpTls->isChecked() || httpMethod->currentData().toString() == QStringLiteral("NONE")) return QString();
            return scanInstanceAlpnError(httpVersion->currentData().toString(), scanInstanceAlpn(httpAlpn->text()));
        }, this, Severity::Error, httpBox->title());
        form->addRule({httpMethod, httpVersion, httpTls}, [this] {
            return scanInstanceNeedsTlsError(httpTls->isChecked(), httpMethod->currentData().toString(), httpVersion->currentData().toString());
        }, this, Severity::Error, httpBox->title());
    }
    if (configBox != nullptr) form->bind(configUrl, Rules::Url({QStringLiteral("http"), QStringLiteral("https")}));
    if (warpBox != nullptr) {
        form->bind(warpSni, Rules::Host(Empty::Allowed));
        form->bind(warpUrl, Rules::Url({QStringLiteral("http"), QStringLiteral("https")}));
        form->addRule({warpJmin, warpJmax}, [this] {
            int jmin = 0, jmax = 0;
            if (!scanInstanceReadNumber(warpJmin, jmin) || !scanInstanceReadNumber(warpJmax, jmax) || jmin <= jmax) return QString();
            return tr("Jmin must not be larger than Jmax.");
        }, this, Severity::Error, warpBox->title());
    }
}

void DialogScanInstance::setupResults() {
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

    resultsReload = new QTimer(this);
    resultsReload->setSingleShot(true);
    resultsReload->setInterval(1000);
    connect(resultsReload, &QTimer::timeout, this, [this] { reloadResults(); });
    connect(ui->tabs, &QTabWidget::currentChanged, this, [this] {
        if (resultsStale && ui->tabs->currentWidget() == ui->tab_results) reloadResults();
    });

    connect(ui->save_as_list, &QPushButton::clicked, this, [this] { saveResultsAsList(); });
    connect(ui->copy_results, &QPushButton::clicked, this, [this] { copyResults(); });
    connect(ui->export_results, &QPushButton::clicked, this, [this] { exportResults(); });
    connect(ui->remove_results, &QPushButton::clicked, this, [this] { removeSelectedResults(); });
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
    ui->kind->setText(isWarp() ? tr("WARP scan") : tr("Generic scan"));
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

    icmpBox->setChecked(config.icmp.enabled);
    icmpTimeout->setText(QString::number(config.icmp.timeoutMs));
    icmpCount->setText(QString::number(config.icmp.count));

    if (tcpBox != nullptr) {
        tcpBox->setChecked(config.tcp.enabled);
        tcpTimeout->setText(QString::number(config.tcp.timeoutMs));
        tcpAttempts->setText(QString::number(config.tcp.attempts));
    }

    if (httpBox != nullptr) {
        const auto &http = config.http;
        httpBox->setChecked(http.enabled);
        httpTls->setChecked(http.tls);
        httpSni->setText(http.serverName);
        httpHost->setText(http.host);
        httpPath->setText(http.path);
        scanInstanceSelectData(httpMethod, http.method.toUpper());
        scanInstanceSelectData(httpVersion, http.httpVersion);
        httpAlpn->setText(http.alpn.join(QStringLiteral(", ")));
        scanInstanceSelectData(httpMinTls, http.minVersion);
        scanInstanceSelectData(httpMaxTls, http.maxVersion);
        scanInstanceSelectData(httpFingerprint, http.fingerprint);
        httpInsecure->setChecked(http.insecure);
        httpDisableSni->setChecked(http.disableSni);
        httpFragment->setChecked(http.fragment);
        httpFragmentDelay->setText(QString::number(http.fragmentFallbackDelayMs));
        httpRecordFragment->setChecked(http.recordFragment);
        httpMixedCaseSni->setChecked(http.mixedCaseSni);
        httpTimeout->setText(QString::number(http.timeoutMs));
    }

    if (configBox != nullptr) {
        const auto &test = config.config;
        configBox->setChecked(test.enabled);
        reloadConfigProfiles(test.profileId);
        configUrl->setText(test.url);
        configTimeout->setText(QString::number(test.timeoutMs));
        configWarm->setChecked(test.warmLatency);
    }

    if (warpBox != nullptr) {
        const auto &warp = config.warp;
        scanInstanceSelectData(warpMode, warp.mode);
        scanInstanceSelectData(warpHttpMode, warp.httpMode);
        scanInstanceSelectData(warpIdentity, warp.identity);
        reloadWarpProfiles(warp.profileId);
        warpMtu->setText(QString::number(warp.mtu));
        warpSni->setText(warp.sni);
        warpJc->setText(QString::number(warp.jc));
        warpJmin->setText(QString::number(warp.jmin));
        warpJmax->setText(QString::number(warp.jmax));
        const QStringList packets = {warp.i1, warp.i2, warp.i3, warp.i4, warp.i5};
        for (int i = 0; i < warpPackets.size() && i < packets.size(); ++i) warpPackets[i]->setText(packets[i]);
        warpUrl->setText(warp.url);
        warpTimeout->setText(QString::number(warp.timeoutMs));
        warpWarm->setChecked(warp.warmLatency);
    }

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

    out.icmp.enabled = icmpBox->isChecked();
    if (!number(icmpTimeout, out.icmp.timeoutMs, out.icmp.enabled,
                tr("ICMP (ping): the timeout must be between %1 and %2 ms.")))
        return false;
    if (!number(icmpCount, out.icmp.count, out.icmp.enabled,
                tr("ICMP (ping): echo requests must be between %1 and %2.")))
        return false;

    // WARP scans have no TCP or TLS / HTTP phase.
    out.tcp.enabled = false;
    out.http.enabled = false;

    if (tcpBox != nullptr) {
        out.tcp.enabled = tcpBox->isChecked();
        if (!number(tcpTimeout, out.tcp.timeoutMs, out.tcp.enabled,
                    tr("TCP connect: the timeout must be between %1 and %2 ms.")))
            return false;
        if (!number(tcpAttempts, out.tcp.attempts, out.tcp.enabled,
                    tr("TCP connect: attempts must be between %1 and %2.")))
            return false;
    }

    if (httpBox != nullptr) {
        auto &http = out.http;
        http.enabled = httpBox->isChecked();
        http.tls = httpTls->isChecked();
        http.serverName = httpSni->text().trimmed();
        http.host = httpHost->text().trimmed();
        http.path = httpPath->text().trimmed();
        if (http.path.isEmpty()) http.path = QStringLiteral("/");
        http.method = httpMethod->currentData().toString();
        http.httpVersion = httpVersion->currentData().toString();
        http.alpn = scanInstanceAlpn(httpAlpn->text());
        http.minVersion = httpMinTls->currentData().toString();
        http.maxVersion = httpMaxTls->currentData().toString();
        http.fingerprint = httpFingerprint->currentData().toString();
        http.insecure = httpInsecure->isChecked();
        http.disableSni = httpDisableSni->isChecked();
        http.fragment = httpFragment->isChecked();
        if (!number(httpFragmentDelay, http.fragmentFallbackDelayMs, http.enabled && http.tls && http.fragment,
                    tr("TLS / HTTP: the fragment fallback delay must be between %1 and %2 ms.")))
            return false;
        http.recordFragment = httpRecordFragment->isChecked();
        http.mixedCaseSni = httpMixedCaseSni->isChecked();
        if (!number(httpTimeout, http.timeoutMs, http.enabled,
                    tr("TLS / HTTP: the timeout must be between %1 and %2 ms.")))
            return false;
        if (http.enabled) {
            if (const auto problem = scanInstanceNeedsTlsError(http.tls, http.method, http.httpVersion); !problem.isEmpty())
                return fail(QStringLiteral("%1: %2").arg(httpBox->title(), problem));
        }
    }

    if (configBox != nullptr) {
        auto &test = out.config;
        test.enabled = configBox->isChecked();
        test.profileId = selectedConfigProfile();
        test.url = configUrl->text().trimmed();
        if (!number(configTimeout, test.timeoutMs, test.enabled,
                    tr("Config test: the timeout must be between %1 and %2 ms.")))
            return false;
        test.warmLatency = configWarm->isChecked();
    }

    if (warpBox != nullptr) {
        auto &warp = out.warp;
        warp.mode = warpMode->currentData().toString();
        warp.httpMode = warpHttpMode->currentData().toInt();
        warp.identity = warpIdentity->currentData().toString();
        warp.profileId = warpProfile->currentData().toInt();
        if (!number(warpMtu, warp.mtu, true, tr("WARP test: the MTU must be between %1 and %2."))) return false;
        warp.sni = warpSni->text().trimmed();
        const bool amnezia = warp.mode == QStringLiteral("amneziawg");
        if (!number(warpJc, warp.jc, amnezia, tr("WARP test: Jc must be between %1 and %2."))) return false;
        if (!number(warpJmin, warp.jmin, amnezia, tr("WARP test: Jmin must be between %1 and %2."))) return false;
        if (!number(warpJmax, warp.jmax, amnezia, tr("WARP test: Jmax must be between %1 and %2."))) return false;
        QStringList packets;
        for (const auto *packet : warpPackets) packets << packet->text().trimmed();
        while (packets.size() < 5) packets << QString();
        warp.i1 = packets[0];
        warp.i2 = packets[1];
        warp.i3 = packets[2];
        warp.i4 = packets[3];
        warp.i5 = packets[4];
        warp.url = warpUrl->text().trimmed();
        if (!number(warpTimeout, warp.timeoutMs, true, tr("WARP test: the timeout must be between %1 and %2 ms.")))
            return false;
        warp.warmLatency = warpWarm->isChecked();
        if (amnezia && warp.jmin > warp.jmax) return fail(tr("WARP test: Jmin must not be larger than Jmax."));
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
    if (configProfile != nullptr) {
        const bool typing = configProfile->lineEdit()->isModified() && selectedConfigProfile() < 0;
        if (!typing) reloadConfigProfiles(selectedConfigProfile());
    }
    if (warpProfile != nullptr) reloadWarpProfiles(warpProfile->currentData().toInt());
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
    if (configProfile == nullptr) return;
    const QScopedValueRollback guard(loading, true);

    auto *profiles = Configs::dataManager->profilesRepo.get();
    QHash<int, QString> idToName;
    for (const auto &[id, profileName] : profiles->GetAllProfileIDNameMapped()) idToName.insert(id, profileName);
    QSet<int> excluded;
    for (const auto &type : kScanInstanceCandidateExclusions) {
        if (Scanner::IsScanBaseType(type)) continue;
        for (const int id : profiles->GetProfileIdsByType(type)) excluded.insert(id);
    }

    configProfile->clear();
    configProfileIds.clear();
    configProfile->addItem(tr("None"), -1);
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
            if (configProfile->count() < configProfile->maxCount()) configProfile->addItem(display, id);
        }
    }
    configProfileNames->setStringList(names);

    const int index = configProfile->findData(selectId);
    if (index >= 0) configProfile->setCurrentIndex(index);
    else if (!selectedName.isEmpty()) configProfile->setCurrentText(selectedName);
    else configProfile->setCurrentIndex(0);
}

int DialogScanInstance::selectedConfigProfile() const {
    if (configProfile == nullptr) return -1;
    const QString text = configProfile->currentText().trimmed();
    if (text.isEmpty() || text == tr("None")) return -1;
    const int index = configProfile->findText(text, Qt::MatchExactly);
    if (index >= 0) return configProfile->itemData(index).toInt();
    return configProfileIds.value(text, -1);
}

void DialogScanInstance::reloadWarpProfiles(int selectId) {
    if (warpProfile == nullptr) return;
    const QScopedValueRollback guard(loading, true);
    auto *profiles = Configs::dataManager->profilesRepo.get();
    const bool masque = currentWarpMode() == QStringLiteral("masque");
    const auto ids = profiles->GetProfileIdsByType(masque ? QStringLiteral("masque") : QStringLiteral("wireguard"));

    warpProfile->clear();
    warpProfile->addItem(tr("Select a profile…"), -1);
    for (const auto &[id, profileName] : profiles->GetProfileIDNameMappedBatch(ids)) warpProfile->addItem(profileName, id);
    scanInstanceSelectData(warpProfile, selectId);
}

QString DialogScanInstance::currentWarpMode() const {
    if (warpMode == nullptr) return config.warp.mode;
    return warpMode->currentData().toString();
}

void DialogScanInstance::updateHttpControls() {
    if (httpBox == nullptr) return;
    // setEnabled(true) on a child would override the unchecked group box's own disabling.
    const bool on = httpBox->isChecked();
    const bool tls = on && httpTls->isChecked();
    const QString method = httpMethod->currentData().toString();
    const bool request = on && method != QStringLiteral("NONE");
    for (QWidget *widget : {static_cast<QWidget *>(httpSni), static_cast<QWidget *>(httpAlpn),
                            static_cast<QWidget *>(httpMinTls), static_cast<QWidget *>(httpMaxTls),
                            static_cast<QWidget *>(httpFingerprint), static_cast<QWidget *>(httpInsecure),
                            static_cast<QWidget *>(httpDisableSni), static_cast<QWidget *>(httpFragment),
                            static_cast<QWidget *>(httpRecordFragment), static_cast<QWidget *>(httpMixedCaseSni)})
        widget->setEnabled(tls);
    httpFragmentDelay->setEnabled(tls && httpFragment->isChecked());
    for (QWidget *widget : {static_cast<QWidget *>(httpHost), static_cast<QWidget *>(httpPath),
                            static_cast<QWidget *>(httpVersion)})
        widget->setEnabled(request);
}

void DialogScanInstance::updateWarpControls() {
    if (warpBox == nullptr) return;
    const QString mode = currentWarpMode();
    const bool masque = mode == QStringLiteral("masque");
    const bool amnezia = mode == QStringLiteral("amneziawg");
    const QString identity = warpIdentity->currentData().toString();

    scanInstanceSetRowVisible(warpForm, warpHttpMode, masque);
    scanInstanceSetRowVisible(warpForm, warpSni, masque);
    for (QWidget *junk : {static_cast<QWidget *>(warpJc), static_cast<QWidget *>(warpJmin), static_cast<QWidget *>(warpJmax)})
        scanInstanceSetRowVisible(warpForm, junk, amnezia);
    for (auto *packet : warpPackets) scanInstanceSetRowVisible(warpForm, packet, amnezia);
    scanInstanceSetRowVisible(warpForm, warpRegisterRow, identity == QStringLiteral("generated"));
    scanInstanceSetRowVisible(warpForm, warpProfile, identity == QStringLiteral("profile"));
    updateWarpIdentityStatus();
}

void DialogScanInstance::updateWarpIdentityStatus() {
    if (warpIdentityStatus == nullptr) return;
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
    warpIdentityStatus->setText(text);
    warpRegister->setText(warp.generatedIdentity.isEmpty() ? tr("Register") : tr("Register again"));
    warpRegister->setEnabled(!registering);
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
    if (loading || warpBox == nullptr) return;
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
    const bool hasResults = !resultsBusy && resultCount > 0 && resultsModel->rowCount() > 0;
    ui->remove_results->setEnabled(!locked && hasResults);
    ui->save_as_list->setEnabled(hasResults);
    ui->copy_results->setEnabled(hasResults);
    ui->export_results->setEnabled(hasResults);
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
    QString summary;
    if (count == 0) summary = tr("No results yet.");
    else if (entries.size() < count) summary = tr("Showing the first %1 of %2 results").arg(entries.size()).arg(count);
    else summary = tr("%n result(s)", nullptr, count);
    ui->results_summary->setText(summary);
    refreshRun();
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
    const auto path = QFileDialog::getSaveFileName(this, tr("Export results"), QDir::homePath() + QLatin1Char('/') + fileName + QStringLiteral(".txt"),
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
    if (resultCount <= 0) {
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
