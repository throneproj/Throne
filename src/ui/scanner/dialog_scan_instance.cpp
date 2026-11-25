#include "include/ui/scanner/dialog_scan_instance.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QCompleter>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
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
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QToolTip>
#include <algorithm>
#include <limits>
#include <type_traits>
#include <vector>

#include "include/configs/common/DnsTunnel.h"
#include "include/configs/common/ResolverSource.h"
#include "include/database/DatabaseManager.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/engine/services/ProfileService.h"
#include "include/global/Utils.hpp"
#include "include/global/Validation.hpp"
#include "include/scanner/IpListParse.h"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanManager.h"
#include "include/ui/mainwindow.h"
#include "include/ui/scanner/IpEntriesModel.h"
#include "include/ui/scanner/ScannerUi.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/utils/NoWheelComboBox.h"
#include "include/ui/utils/ValidationForm.h"
#include "include/ui/utils/NativeFileDialog.h"
#include "include/ui/widget/ResolverSourceWidget.h"

namespace {
    constexpr int kScanInstanceResultsPreview = 50000;

    const QStringList kScanInstanceConcurrencyPresets = {
        QStringLiteral("8"), QStringLiteral("16"), QStringLiteral("32"),
        QStringLiteral("64"), QStringLiteral("128"), QStringLiteral("256"),
    };

    void scanInstanceSetupNumber(QLineEdit *edit, int minimum, int maximum, const QString &placeholder,
                                 const QStringList &presets = {}) {
        edit->setValidator(new QIntValidator(minimum, maximum, edit));
        edit->setPlaceholderText(placeholder);
        if (!presets.isEmpty()) edit->setCompleter(new QCompleter(presets, edit));
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

    void scanInstanceSelectData(QComboBox *combo, const QVariant &data) {
        const int index = combo->findData(data);
        combo->setCurrentIndex(index >= 0 ? index : 0);
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
            case IpEntriesModel::LossColumn:
                sortBy([](const Configs::IpListEntry &entry) {
                    return entry.lossPct >= 0 ? entry.lossPct : std::numeric_limits<int>::max();
                });
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
    if (row == nullptr || row->kind != Configs::IpScan::Kind::Resolver) return;
    auto *window = new DialogScanInstance(GetMainWindow(), row);
    windows.insert(scanId, window);
    ActivateWindow(window);
}

DialogScanInstance::DialogScanInstance(QWidget *parent, std::shared_ptr<Configs::IpScan> scan_)
    : QDialog(parent), ui(new Ui::DialogScanInstance), scan(std::move(scan_)), scanId(scan->id), config(scan->config),
      defaults(Configs::IpScan::DefaultConfig(scan->kind)) {
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    form = new Validation::Form(this);

    {
        const QScopedValueRollback guard(loading, true);
        setupSettings();
        bindValidation();
        setupResults();
        setupRunPanel();
        loadForm();
    }
    watchEdits(ui->settings);
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
        if (Configs::dataManager->ipScansRepo->GetIpScan(scanId) == nullptr) {
            discarding = true;
            close();
            return;
        }
        reloadRow();
        updateTitle();
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

void DialogScanInstance::setupSettings() {
    connect(ui->manage_lists, &QPushButton::clicked, this, [this] {
        if (auto *window = GetMainWindow()) window->showIpListsDialog(ui->base_list->currentData().toInt());
    });
    scanInstanceSetupNumber(ui->stop_after, 0, 1000000, tr("No limit"));
    scanInstanceSetupNumber(ui->concurrency, 1, 1000, QString::number(defaults.concurrency), kScanInstanceConcurrencyPresets);
}

void DialogScanInstance::bindValidation() {
    form->bind(ui->stop_after, [this](const QString &text) { return text.isEmpty() ? QString() : scanInstanceNumberError(ui->stop_after); });
    form->bind(ui->concurrency, [this](const QString &) { return scanInstanceNumberError(ui->concurrency); });
}

void DialogScanInstance::setupResults() {
    resultsReload = new QTimer(this);
    resultsReload->setSingleShot(true);
    resultsReload->setInterval(1000);
    connect(resultsReload, &QTimer::timeout, this, [this] { reloadResults(); });

    resultsModel = new IpEntriesModel(this);
    resultsProxy = new QSortFilterProxyModel(this);
    resultsProxy->setSourceModel(resultsModel);
    resultsProxy->setSortRole(IpEntriesModel::SortRole);
    ui->results_view->setModel(resultsProxy);
    auto *header = ui->results_view->horizontalHeader();
    // Unsorted rows keep the list's best-first order; a third click on a column header returns to it.
    header->setSortIndicatorClearable(true);
    ui->results_view->sortByColumn(-1, Qt::AscendingOrder);
    header->setSectionResizeMode(IpEntriesModel::AddressColumn, QHeaderView::Stretch);
    for (const int column : {IpEntriesModel::PortColumn, IpEntriesModel::LossColumn, IpEntriesModel::LatencyColumn})
        header->setSectionResizeMode(column, QHeaderView::ResizeToContents);

    connect(ui->save_as_list, &QPushButton::clicked, this, [this] { saveResultsAsList(); });
    connect(ui->copy_results, &QPushButton::clicked, this, [this] { copyResults(); });
    connect(ui->export_results, &QPushButton::clicked, this, [this] { exportResults(); });
    connect(ui->remove_results, &QPushButton::clicked, this, [this] { removeSelectedResults(); });
    connect(ui->use_as_resolvers, &QPushButton::clicked, this, [this] { useResultsAsResolvers(); });
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
}

void DialogScanInstance::markDirty() {
    if (loading) return;
    dirty = true;
}

void DialogScanInstance::loadForm() {
    const QScopedValueRollback guard(loading, true);

    updateTitle();
    reloadBaseLists(scan->base_list_id);
    ui->stop_after->setText(config.stopAfter > 0 ? QString::number(config.stopAfter) : QString());
    ui->concurrency->setText(QString::number(config.concurrency));
    dirty = false;
}

void DialogScanInstance::updateTitle() {
    const int profileId = scan->config.resolver.profileId;
    const auto profile = profileId >= 0 ? Configs::dataManager->profilesRepo->GetProfile(profileId) : nullptr;
    const QString name = profile != nullptr && profile->outbound != nullptr ? profile->outbound->name : scan->name;
    setWindowTitle(tr("%1 - Find resolvers").arg(name));
}

bool DialogScanInstance::collect(Configs::ScanConfig &out, int &baseListId, QString *error) const {
    const auto fail = [error](const QString &text) {
        if (error != nullptr) *error = text;
        return false;
    };
    const auto number = [&fail](const QLineEdit *edit, int &value, const QString &rangeError) {
        if (scanInstanceReadNumber(edit, value)) return true;
        const auto *validator = qobject_cast<const QIntValidator *>(edit->validator());
        return fail(rangeError.arg(validator->bottom()).arg(validator->top()));
    };

    out = config;
    baseListId = ui->base_list->currentData().toInt();
    out.stopAfter = 0;
    if (!ui->stop_after->text().trimmed().isEmpty() &&
        !number(ui->stop_after, out.stopAfter, tr("Stop after must be between %1 and %2 results; leave it empty for no limit.")))
        return false;
    return number(ui->concurrency, out.concurrency, tr("Concurrency must be between %1 and %2."));
}

bool DialogScanInstance::save(QString *error) {
    Configs::ScanConfig collected;
    int baseListId = -1;
    if (!collect(collected, baseListId, error)) {
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
    refreshRun();
    return true;
}

bool DialogScanInstance::saveIfDirty() {
    if (!dirty) return true;
    if (!form->validateAll()) return false;
    QString error;
    if (save(&error)) return true;
    scanInstanceNotice(this, QMessageBox::Warning, tr("Find resolvers"), error);
    return false;
}

void DialogScanInstance::reject() {
    if (!discarding && dirty) {
        QString error;
        if (!save(&error)) {
            const auto answer = QMessageBox::question(
                this, tr("Find resolvers"), tr("The settings could not be saved:\n%1\n\nClose without saving?").arg(error));
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
    if (resultsStale) reloadResults();
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

void DialogScanInstance::reloadBaseLists(int selectId) {
    const QScopedValueRollback guard(loading, true);
    auto *combo = ui->base_list;
    combo->clear();
    combo->addItem(tr("Select an IP list…"), -1);
    for (const auto &list : Configs::dataManager->ipListsRepo->GetAllIpLists()) {
        // "Rescan current results" covers the scan's own list.
        if (list->role == Configs::IpList::Role::ScanResult && list->related_test_id == scanId) continue;
        combo->addItem(tr("%1 (%n entries)", nullptr, list->entryCount).arg(list->name), list->id);
    }
    scanInstanceSelectData(combo, selectId);
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
    const bool idle = !running && !resultsBusy;
    ui->start_button->setEnabled(idle);
    ui->pause_button->setText(resumable ? tr("Resume") : tr("Pause"));
    ui->pause_button->setEnabled(running ? !live.stopping : idle && resumable);
    applyLock(running);
}

void DialogScanInstance::applyLock(bool locked) {
    ui->settings->setEnabled(!locked);
    const bool hasResults = !resultsBusy && resultCount > 0 && resultsModel->rowCount() > 0;
    ui->remove_results->setEnabled(!locked && hasResults);
    ui->save_as_list->setEnabled(hasResults);
    ui->copy_results->setEnabled(hasResults);
    ui->export_results->setEnabled(hasResults);
    ui->use_as_resolvers->setEnabled(hasResults);
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
    if (!isVisible()) return;
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
        if (Engine::ProfileService::Save({profile})) {
            source->ipListId = previous;
            scanInstanceNotice(this, QMessageBox::Warning, title, tr("Failed to store the profile."));
            return;
        }
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
    if (!ScannerUi::ConfirmStartOver(this, *scan)) return;
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
