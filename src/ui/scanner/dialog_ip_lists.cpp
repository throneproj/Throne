#include "include/ui/scanner/dialog_ip_lists.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QScrollBar>
#include <QShortcut>
#include <QStringConverter>
#include <QTimer>
#include <algorithm>
#include <climits>

#include "include/database/DatabaseManager.h"
#include "include/database/IpListsRepo.h"
#include "include/database/IpScansRepo.h"
#include "include/global/Utils.hpp"
#include "include/scanner/DefaultIpLists.h"
#include "include/scanner/IpListParse.h"
#include "include/scanner/IpListUpdater.h"
#include "include/scanner/ScanManager.h"
#include "include/ui/scanner/IpListItem.h"
#include "include/ui/scanner/dialog_edit_ip_list.h"
#include "include/ui/utils/ReorderListWidget.h"
#include "include/ui/utils/NativeFileDialog.h"

namespace {
    constexpr qint64 kIpListsMaxImportFileSize = 50 * 1024 * 1024;
    constexpr int kIpListsMaxShownProblems = 20;

    int ipListsMessage(QWidget *parent, const QMessageBox::Icon icon, const QString &title, const QString &text,
                       const QMessageBox::StandardButtons buttons = QMessageBox::Ok) {
        QMessageBox box(icon, title, text, buttons, parent);
        // List, scan and file names are foreign text; AutoText would render a tag-like one as HTML.
        box.setTextFormat(Qt::PlainText);
        return box.exec();
    }

    void ipListsNotice(QWidget *parent, const QMessageBox::Icon icon, const QString &title, const QString &text) {
        auto *box = new QMessageBox(icon, title, text, QMessageBox::Ok, parent);
        box->setTextFormat(Qt::PlainText);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setWindowModality(Qt::NonModal);
        box->show();
    }

    QString ipListsUniqueName(const QString &base, const QStringList &taken) {
        if (!taken.contains(base)) return base;
        for (int n = 2;; ++n) {
            const auto candidate = QStringLiteral("%1 (%2)").arg(base).arg(n);
            if (!taken.contains(candidate)) return candidate;
        }
    }

    QByteArray ipListsUtf8Bytes(const QByteArray &bytes) {
        if (const auto encoding = QStringConverter::encodingForData(bytes)) {
            QStringDecoder decoder(*encoding);
            const QString text = decoder(bytes);
            if (!decoder.hasError()) return text.toUtf8();
        }
        return bytes;
    }

    QString ipListsProblemsText(const QStringList &problems) {
        if (problems.size() <= kIpListsMaxShownProblems) return problems.join('\n');
        return problems.mid(0, kIpListsMaxShownProblems).join('\n') + '\n' +
               DialogIpLists::tr("…and %Ln more", nullptr, static_cast<int>(problems.size() - kIpListsMaxShownProblems));
    }
}

DialogIpLists::DialogIpLists(QWidget *parent) : QDialog(parent), ui(new Ui::DialogIpLists) {
    ui->setupUi(this);
    // The list view ignores Return after emitting activated, so QDialog would click an auto-default "New".
    for (auto *button : findChildren<QPushButton *>()) button->setAutoDefault(false);

    auto *importMenu = new QMenu(this);
    importMenu->addAction(tr("From text…"), this, [this] {
        createList(Configs::IpList::SourceKind::Manual, QGuiApplication::clipboard()->text());
    });
    importMenu->addAction(tr("From file…"), this, [this] { importFromFiles(); });
    importMenu->addAction(tr("From URL…"), this, [this] { createList(Configs::IpList::SourceKind::Url); });
    importMenu->addAction(tr("From rule-set…"), this, [this] { createList(Configs::IpList::SourceKind::RuleSet); });
    importMenu->addSeparator();
    importMenu->addAction(tr("Add default lists"), this, [this] {
        if (Scanner::DefaultIpLists::EnsureAll() == 0) {
            ipListsMessage(this, QMessageBox::Information, tr("IP Lists"), tr("Every default list is already present."));
            return;
        }
        Scanner::IpListUpdater::instance()->NotifyListsChanged();
    });
    ui->import_list->setMenu(importMenu);

    auto *pasteShortcut = new QShortcut(QKeySequence::Paste, this);
    pasteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteShortcut, &QShortcut::activated, this, [this] {
        createList(Configs::IpList::SourceKind::Manual, QGuiApplication::clipboard()->text());
    });

    connect(ui->lists, &ReorderListWidget::reorderRequested, this, &DialogIpLists::moveList);
    const auto applyRowActions = [this] {
        const int hovered = ui->lists->HoveredRow();
        for (int row = 0; row < ui->lists->count(); ++row) {
            auto *item = ui->lists->item(row);
            if (auto *widget = qobject_cast<IpListItem *>(ui->lists->itemWidget(item)))
                widget->SetActionsVisible(row == hovered || item->isSelected());
        }
    };
    connect(ui->lists, &ReorderListWidget::hoveredRowChanged, this, applyRowActions);
    connect(ui->lists, &QListWidget::itemSelectionChanged, this, applyRowActions);
    connect(ui->lists, &QListWidget::itemSelectionChanged, this, [this] { updateButtons(); });
    // Double-click and Enter both arrive here; itemDoubleClicked as well would open the editor twice.
    connect(ui->lists, &QListWidget::itemActivated, this, [this](const QListWidgetItem *item) {
        const int id = item->data(Qt::UserRole).toInt();
        // Leave the view's event handler before a nested exec() rebuilds the rows under it.
        QTimer::singleShot(0, this, [this, id] { editList(id); });
    });

    reloadTimer = new QTimer(this);
    reloadTimer->setSingleShot(true);
    reloadTimer->setInterval(50);
    connect(reloadTimer, &QTimer::timeout, this, [this] { reloadLists(true); });

    auto *updater = Scanner::IpListUpdater::instance();
    connect(updater, &Scanner::IpListUpdater::listsChanged, this, [this] { reloadTimer->start(); },
            Qt::QueuedConnection);
    connect(updater, &Scanner::IpListUpdater::listUpdated, this, [this] { reloadTimer->start(); },
            Qt::QueuedConnection);

    // Throttled, not debounced: a running scan changes its result list on every chunk.
    auto *scanReloadTimer = new QTimer(this);
    scanReloadTimer->setSingleShot(true);
    scanReloadTimer->setInterval(1000);
    connect(scanReloadTimer, &QTimer::timeout, this, [this] { reloadTimer->start(); });
    const auto scheduleScanReload = [scanReloadTimer] {
        if (!scanReloadTimer->isActive()) scanReloadTimer->start();
    };
    auto *scans = Scanner::ScanManager::instance();
    connect(scans, &Scanner::ScanManager::resultsChanged, this, scheduleScanReload, Qt::QueuedConnection);
    connect(scans, &Scanner::ScanManager::statusChanged, this, scheduleScanReload, Qt::QueuedConnection);

    auto *busyTimer = new QTimer(this);
    busyTimer->setInterval(1000);
    connect(busyTimer, &QTimer::timeout, this, [this] {
        refreshBusyStates();
        if (rebuildPending && QGuiApplication::mouseButtons() == Qt::NoButton) reloadTimer->start();
    });
    busyTimer->start();

    reloadLists();

    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    if (scr != nullptr) resize(sizeHint().expandedTo(QSize(620, 440)).boundedTo(scr->availableGeometry().size()));
}

DialogIpLists::~DialogIpLists() {
    delete ui;
}

void DialogIpLists::selectList(const int id) {
    pendingSelectId = selectRow(id) ? -1 : id;
}

bool DialogIpLists::selectRow(const int id) {
    for (int row = 0; row < ui->lists->count(); ++row) {
        auto *item = ui->lists->item(row);
        if (item->data(Qt::UserRole).toInt() != id) continue;
        ui->lists->setCurrentItem(item);
        ui->lists->scrollToItem(item);
        return true;
    }
    return false;
}

void DialogIpLists::reloadLists(const bool deferRebuild) {
    reloadTimer->stop();
    const auto fresh = Configs::dataManager->ipListsRepo->GetAllIpLists();

    QHash<int, QString> scanNames;
    const bool anyResult = std::any_of(fresh.cbegin(), fresh.cend(), [](const auto &list) {
        return list->role == Configs::IpList::Role::ScanResult;
    });
    if (anyResult) {
        for (const auto &scan : Configs::dataManager->ipScansRepo->GetAllIpScans()) scanNames.insert(scan->id, scan->name);
    }

    bool sameRows = fresh.size() == ui->lists->count();
    for (int row = 0; sameRows && row < fresh.size(); ++row)
        sameRows = ui->lists->item(row)->data(Qt::UserRole).toInt() == fresh.at(row)->id;

    if (sameRows) {
        for (int row = 0; row < fresh.size(); ++row) {
            const auto &list = fresh.at(row);
            const auto owner = list->role == Configs::IpList::Role::ScanResult ? scanNames.value(list->related_test_id) : QString();
            if (auto *widget = qobject_cast<IpListItem *>(ui->lists->itemWidget(ui->lists->item(row))))
                widget->SetList(list, owner);
        }
        lists = fresh;
        refreshBusyStates();
    } else if (deferRebuild && QGuiApplication::mouseButtons() != Qt::NoButton) {
        // Rebuilding deletes the row widgets under a held hover button or a running drag; the busy timer retries.
        rebuildPending = true;
        updateButtons();
        return;
    } else {
        rebuildRows(fresh, scanNames);
    }

    rebuildPending = false;
    if (pendingSelectId >= 0) {
        selectRow(pendingSelectId);
        pendingSelectId = -1;
    }
    updateButtons();
}

void DialogIpLists::rebuildRows(const QList<std::shared_ptr<Configs::IpList>> &fresh, const QHash<int, QString> &scanNames) {
    const auto previous = selectedList();
    const int selectedId = previous ? previous->id : -1;
    const int scroll = ui->lists->verticalScrollBar()->value();

    lists = fresh;
    auto *updater = Scanner::IpListUpdater::instance();
    ui->lists->setUpdatesEnabled(false);
    ui->lists->clear();
    for (const auto &list : lists) {
        auto *item = new QListWidgetItem(ui->lists);
        item->setData(Qt::UserRole, list->id);
        const auto owner = list->role == Configs::IpList::Role::ScanResult ? scanNames.value(list->related_test_id) : QString();
        auto *widget = new IpListItem(ui->lists, list, item, owner);
        const int id = list->id;
        // Queued: a reload inside the nested exec() would otherwise delete the row whose button is still on the stack.
        connect(widget, &IpListItem::editRequested, this, [this, id] { editList(id); }, Qt::QueuedConnection);
        connect(widget, &IpListItem::refreshRequested, this, [this, id] { refreshList(id); });
        connect(widget, &IpListItem::deleteRequested, this, [this, id] {
            if (const auto current = listById(id)) deleteList(current);
        }, Qt::QueuedConnection);
        widget->SetRefreshing(updater->IsRefreshing(id));
        ui->lists->setItemWidget(item, widget);
        if (id == selectedId) ui->lists->setCurrentItem(item);
    }
    ui->lists->setUpdatesEnabled(true);
    ui->lists->verticalScrollBar()->setValue(scroll);
    ui->lists->RefreshHover();
}

void DialogIpLists::refreshBusyStates() const {
    auto *updater = Scanner::IpListUpdater::instance();
    for (int row = 0; row < ui->lists->count(); ++row) {
        if (auto *widget = qobject_cast<IpListItem *>(ui->lists->itemWidget(ui->lists->item(row))))
            widget->SetRefreshing(updater->IsRefreshing(widget->ListId()));
    }
    ui->lists->RefreshHover();
}

std::shared_ptr<Configs::IpList> DialogIpLists::listById(const int id) const {
    for (const auto &list : lists) {
        if (list->id == id) return list;
    }
    return nullptr;
}

std::shared_ptr<Configs::IpList> DialogIpLists::selectedList() const {
    const auto selected = ui->lists->selectedItems();
    if (selected.isEmpty()) return nullptr;
    return listById(selected.first()->data(Qt::UserRole).toInt());
}

void DialogIpLists::updateButtons() const {
    const bool hasSelection = selectedList() != nullptr;
    ui->duplicate->setEnabled(hasSelection);
    ui->export_list->setEnabled(hasSelection);
    ui->import_list->setEnabled(!importing);
    ui->import_list->setText(importing ? tr("Importing…") : tr("Import"));
}

void DialogIpLists::createList(const Configs::IpList::SourceKind kind, const QString &prefillText) {
    DialogEditIpList dialog(this, -1, kind, prefillText);
    if (dialog.exec() != QDialog::Accepted) return;
    reloadLists();
    selectList(dialog.ListId());
}

void DialogIpLists::editList(const int id) {
    DialogEditIpList dialog(this, id);
    if (dialog.exec() != QDialog::Accepted) return;
    reloadLists();
    selectList(id);
}

void DialogIpLists::refreshList(const int id) {
    Scanner::IpListUpdater::instance()->Refresh(id);
    refreshBusyStates();
}

void DialogIpLists::deleteList(const std::shared_ptr<Configs::IpList> &list) {
    const auto title = tr("Delete IP list");
    if (Scanner::IpListUpdater::instance()->IsRefreshing(list->id)) {
        ipListsMessage(this, QMessageBox::Warning, title,
                       tr("\"%1\" is being refreshed. Delete it when the refresh has finished.").arg(list->name));
        return;
    }

    QString question = tr("Delete the IP list \"%1\"?").arg(list->name);
    const auto scans = Configs::dataManager->ipScansRepo->GetAllIpScans();
    QStringList targetOf;
    for (const auto &scan : scans) {
        if (scan->base_list_id == list->id) targetOf << scan->name;
        if (list->role != Configs::IpList::Role::ScanResult || scan->id != list->related_test_id) continue;
        if (Scanner::ScanManager::instance()->IsRunning(scan->id)) {
            ipListsMessage(this, QMessageBox::Warning, title,
                           tr("Scan \"%1\" is running and stores its results in this list. Pause it first.").arg(scan->name));
            return;
        }
        question += "\n\n" + tr("It holds the results of scan \"%1\", which creates a new result list the next time it runs.")
                                 .arg(scan->name);
    }
    if (!targetOf.isEmpty())
        question += "\n\n" + tr("These scans use it as their targets: %1").arg(targetOf.join(QStringLiteral(", ")));

    if (ipListsMessage(this, QMessageBox::Question, title, question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Detach before the slow delete, or a scan started meanwhile would adopt this result list.
    Configs::dataManager->ipListsRepo->DetachIpList(list->id);
    reloadLists(true);
    QPointer<DialogIpLists> self(this);
    runOnNewThread([self, id = list->id] {
        Configs::dataManager->ipListsRepo->DeleteIpList(id, true);
        Scanner::IpListUpdater::instance()->NotifyListsChanged();
        runOnUiThread([self] {
            if (self) self->reloadLists(true);
        });
    });
}

void DialogIpLists::moveList(const int from, const int to) {
    if (from < 0 || from >= lists.size() || to < 0 || to >= lists.size() || from == to) return;

    lists.move(from, to);
    QList<int> ids;
    ids.reserve(lists.size());
    for (const auto &list : lists) ids.append(list->id);
    Configs::dataManager->ipListsRepo->UpdateIpListsOrder(ids);

    reloadLists();
    Scanner::IpListUpdater::instance()->NotifyListsChanged();
}

void DialogIpLists::on_new_list_clicked() {
    createList(Configs::IpList::SourceKind::Manual);
}

void DialogIpLists::on_duplicate_clicked() {
    const auto list = selectedList();
    if (!list) return;

    bool ok = false;
    const auto suggested = tr("%1 copy").arg(list->name);
    auto name = QInputDialog::getText(this, tr("Duplicate IP list"), tr("Name of the copy:"), QLineEdit::Normal,
                                      suggested, &ok).trimmed();
    if (!ok) return;
    if (name.isEmpty()) name = suggested;

    QPointer<DialogIpLists> self(this);
    ui->duplicate->setEnabled(false);
    runOnNewThread([self, id = list->id, name] {
        const auto copy = Configs::dataManager->ipListsRepo->CopyAsUserList(id, name);
        if (copy) Scanner::IpListUpdater::instance()->NotifyListsChanged();
        runOnUiThread([self, copyId = copy ? copy->id : -1] {
            if (!self) return;
            self->updateButtons();
            if (copyId < 0) {
                ipListsNotice(self, QMessageBox::Warning, tr("Duplicate IP list"), tr("Failed to copy the IP list."));
                return;
            }
            self->reloadLists(true);
            self->selectList(copyId);
        });
    });
}

void DialogIpLists::on_export_list_clicked() {
    const auto list = selectedList();
    if (!list) return;

    static const QRegularExpression unsafeFileChars(QStringLiteral(R"([\\/:*?"<>|])"));
    auto fileName = list->name;
    fileName.replace(unsafeFileChars, QStringLiteral("_"));
    if (fileName.trimmed().isEmpty()) fileName = QStringLiteral("ip-list");

    const auto title = tr("Export IP list");
    const auto path = NativeFileDialog::getSaveFileName(this, title, QDir::homePath() + "/" + fileName + ".txt",
                                                        tr("Text files (*.txt);;All files (*)"));
    if (path.isEmpty()) return;

    const auto entries = Configs::dataManager->ipListsRepo->GetEntries(list->id);
    QByteArray out;
    out.reserve(entries.size() * 20);
    for (const auto &entry : entries) {
        out += Scanner::FormatEntry(entry).toUtf8();
        out += '\n';
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(out) != out.size() || !file.commit()) {
        ipListsMessage(this, QMessageBox::Warning, title, tr("Cannot write to: %1").arg(path));
        return;
    }
    ipListsMessage(this, QMessageBox::Information, title,
                   tr("Exported %Ln entries to:\n%1", nullptr, static_cast<int>(entries.size())).arg(path));
}

void DialogIpLists::importFromFiles() {
    if (importing) return;
    const auto paths = NativeFileDialog::getOpenFileNames(
        this, tr("Import IP lists"), QDir::homePath(),
        tr("All files (*);;Text and CSV (*.txt *.csv *.lst *.list);;sing-box rule-sets (*.srs *.json)"));
    if (paths.isEmpty()) return;

    importing = true;
    updateButtons();

    QPointer<DialogIpLists> self(this);
    runOnNewThread([self, paths] {
        auto *repo = Configs::dataManager->ipListsRepo.get();
        QStringList taken;
        for (const auto &list : repo->GetAllIpLists()) taken << list->name;

        int created = 0;
        qint64 imported = 0;
        int lastId = -1;
        QStringList problems;
        for (const auto &path : paths) {
            const QFileInfo info(path);
            const auto fileName = info.fileName();
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                problems << tr("%1: cannot be opened").arg(fileName);
                continue;
            }
            const auto tooLarge = tr("%1: larger than 50 MB, skipped").arg(fileName);
            if (file.size() > kIpListsMaxImportFileSize) {
                problems << tooLarge;
                continue;
            }
            // Bounded: a pipe or device reports size 0 and may never end.
            const auto raw = file.read(kIpListsMaxImportFileSize + 1);
            file.close();
            if (raw.size() > kIpListsMaxImportFileSize) {
                problems << tooLarge;
                continue;
            }
            const auto bytes = ipListsUtf8Bytes(raw);

            const auto outcome = Scanner::ParseImportBytes(bytes);
            if (!outcome.error.isEmpty()) {
                problems << QStringLiteral("%1: %2").arg(fileName, outcome.error);
                continue;
            }
            if (outcome.entries.isEmpty()) {
                problems << tr("%1: no addresses found").arg(fileName);
                continue;
            }

            auto list = Configs::IpListsRepo::NewIpList();
            list->name = ipListsUniqueName(info.completeBaseName().isEmpty() ? fileName : info.completeBaseName(), taken);
            list->entries = outcome.entries;
            list->entryCount = static_cast<int>(outcome.entries.size());
            list->entriesLoaded = true;
            if (!repo->AddIpList(list)) {
                problems << tr("%1: could not be stored").arg(fileName);
                continue;
            }
            taken << list->name;
            ++created;
            imported += list->entryCount;
            lastId = list->id;
            if (outcome.rejected > 0) {
                QString problem = tr("%1: %Ln unreadable entries skipped", nullptr, outcome.rejected).arg(fileName);
                if (!outcome.samples.isEmpty()) problem += QStringLiteral(" (") + outcome.samples.join(QStringLiteral(", ")) + ")";
                problems << problem;
            }
        }

        runOnUiThread([self, created, imported, lastId, problems] {
            if (created > 0) Scanner::IpListUpdater::instance()->NotifyListsChanged();
            if (self) {
                self->importing = false;
                self->reloadLists(true);
                if (lastId >= 0) self->selectList(lastId);
            }

            // Non-modal: this runs from a posted callback, where a nested exec() is unsafe (#1874).
            QWidget *parent = self ? static_cast<QWidget *>(self.data()) : GetMessageBoxParent();
            const auto title = tr("Import IP lists");
            if (created == 0) {
                ipListsNotice(parent, QMessageBox::Warning, title,
                              tr("Nothing could be imported:") + "\n" + ipListsProblemsText(problems));
                return;
            }
            QString summary = tr("Imported %Ln list(s)", nullptr, created) + " " +
                              tr("with %Ln entries.", nullptr, static_cast<int>(std::min<qint64>(imported, INT_MAX)));
            if (!problems.isEmpty()) summary += "\n\n" + ipListsProblemsText(problems);
            ipListsNotice(parent, QMessageBox::Information, title, summary);
        });
    });
}
