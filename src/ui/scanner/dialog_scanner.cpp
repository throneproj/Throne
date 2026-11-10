#include "include/ui/scanner/dialog_scanner.h"

#include <QCheckBox>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>

#include "include/database/DatabaseManager.h"
#include "include/database/IpScansRepo.h"
#include "include/global/Utils.hpp"
#include "include/scanner/ScanManager.h"
#include "include/ui/mainwindow.h"
#include "include/ui/scanner/ScanItem.h"
#include "include/ui/scanner/dialog_new_scan.h"
#include "include/ui/scanner/dialog_scan_instance.h"

DialogScanner::DialogScanner(QWidget *parent) : QDialog(parent), ui(new Ui::DialogScanner) {
    ui->setupUi(this);
    ui->scan_list->setSelectionMode(QAbstractItemView::SingleSelection);
    // QDialog makes the first auto-default button the default, so Enter on a row would click "New scan".
    for (auto *button : findChildren<QPushButton *>()) button->setAutoDefault(false);

    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(ui->new_scan, &QPushButton::clicked, this, [this] { newScan(); });
    connect(ui->ip_lists, &QPushButton::clicked, this, [] {
        if (auto *window = GetMainWindow()) window->showIpListsDialog();
    });
    // Covers Enter and double-click; connecting itemDoubleClicked as well would open twice.
    connect(ui->scan_list, &QListWidget::itemActivated, this, [this](const QListWidgetItem *item) {
        const int row = ui->scan_list->row(item);
        if (row >= 0 && row < scans.size()) DialogScanInstance::Open(scans.at(row)->id);
    });
    connect(ui->scan_list, &ReorderListWidget::reorderRequested, this, &DialogScanner::moveScan);
    const auto applyRowActions = [this] {
        const int hovered = ui->scan_list->HoveredRow();
        for (int row = 0; row < ui->scan_list->count(); ++row) {
            auto *item = ui->scan_list->item(row);
            if (auto *widget = qobject_cast<ScanItem *>(ui->scan_list->itemWidget(item)))
                widget->SetActionsVisible(row == hovered || item->isSelected());
        }
    };
    connect(ui->scan_list, &ReorderListWidget::hoveredRowChanged, this, applyRowActions);
    connect(ui->scan_list, &QListWidget::itemSelectionChanged, this, applyRowActions);

    auto *manager = Scanner::ScanManager::instance();
    connect(manager, &Scanner::ScanManager::progressChanged, this, [this](int id) { refreshItem(id, false); });
    connect(manager, &Scanner::ScanManager::statusChanged, this, [this](int id) { refreshItem(id, true); });
    connect(manager, &Scanner::ScanManager::scansChanged, this, [this] { reloadScans(); });

    reloadScans();

    const auto *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const QSize wanted = sizeHint().expandedTo(QSize(560, 400));
    resize(scr != nullptr ? wanted.boundedTo(scr->availableGeometry().size()) : wanted);
}

DialogScanner::~DialogScanner() {
    showing = false;
    syncWatched();
    delete ui;
}

void DialogScanner::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    showing = true;
    syncWatched();
}

void DialogScanner::hideEvent(QHideEvent *event) {
    QDialog::hideEvent(event);
    showing = false;
    syncWatched();
}

void DialogScanner::syncWatched() {
    QSet<int> wanted;
    if (showing) {
        for (const auto &scan : scans) wanted.insert(scan->id);
    }
    for (const int id : watchedIds) {
        if (!wanted.contains(id)) ScannerUi::Watch(id, false);
    }
    for (const int id : wanted) {
        if (!watchedIds.contains(id)) ScannerUi::Watch(id, true);
    }
    watchedIds = wanted;
}

void DialogScanner::reloadScans() {
    scans = Configs::dataManager->ipScansRepo->GetAllIpScans();

    const int scroll = ui->scan_list->verticalScrollBar() ? ui->scan_list->verticalScrollBar()->value() : 0;
    ui->scan_list->clear();
    for (const auto &scan : scans) {
        auto *item = new QListWidgetItem(ui->scan_list);
        auto *widget = new ScanItem(ui->scan_list, scan, item);
        const int id = scan->id;
        connect(widget, &ScanItem::startRequested, this, [this, id] { startOrPause(id); });
        connect(widget, &ScanItem::pauseRequested, this, [this, id] { startOrPause(id); });
        connect(widget, &ScanItem::openRequested, this, [id] { DialogScanInstance::Open(id); });
        connect(widget, &ScanItem::deleteRequested, this, [this, id] { deleteScan(id); });
        ui->scan_list->setItemWidget(item, widget);
    }
    if (ui->scan_list->verticalScrollBar()) ui->scan_list->verticalScrollBar()->setValue(scroll);
    ui->empty_hint->setVisible(scans.isEmpty());
    ui->scan_list->RefreshHover();

    syncWatched();
}

ScanItem *DialogScanner::itemFor(int scanId) const {
    for (int row = 0; row < ui->scan_list->count(); ++row) {
        auto *widget = qobject_cast<ScanItem *>(ui->scan_list->itemWidget(ui->scan_list->item(row)));
        if (widget != nullptr && widget->ScanId() == scanId) return widget;
    }
    return nullptr;
}

void DialogScanner::refreshItem(int scanId, bool reload) const {
    auto *widget = itemFor(scanId);
    if (widget == nullptr) return;
    widget->Refresh(reload);
    ui->scan_list->RefreshHover();
}

void DialogScanner::startOrPause(int scanId) {
    auto *manager = Scanner::ScanManager::instance();
    if (manager->IsRunning(scanId)) {
        manager->Pause(scanId);
        refreshItem(scanId, false);
        return;
    }

    // Unsaved edits in the scan's own window must reach the row before the run snapshots it.
    if (!DialogScanInstance::SaveIfDirty(scanId)) return;
    const auto scan = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
    if (scan == nullptr) return;
    if (ScannerUi::IsResumable(*scan)) {
        ScannerUi::StartScan(this, scanId, Scanner::StartMode::Resume);
    } else if (ScannerUi::ConfirmWipe(this, *scan)) {
        ScannerUi::StartScan(this, scanId, Scanner::StartMode::FromInitial);
    }
    refreshItem(scanId, true);
}

void DialogScanner::deleteScan(int scanId) {
    const auto scan = Configs::dataManager->ipScansRepo->GetIpScan(scanId);
    if (scan == nullptr) return;
    if (Scanner::ScanManager::instance()->IsRunning(scanId)) {
        MessageBoxWarning(tr("IP Scanner"), tr("Pause the scan first."));
        return;
    }

    QMessageBox box(QMessageBox::Question, tr("Confirmation"), tr("Delete the scan \"%1\"?").arg(scan->name),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setTextFormat(Qt::PlainText);
    // A settings search keeps its trials in the scan row, so it has no result list.
    QCheckBox *alsoResults = nullptr;
    if (scan->kind != Configs::IpScan::Kind::Tune) {
        alsoResults = new QCheckBox(tr("Also delete its result list"), &box);
        box.setCheckBox(alsoResults);
    }
    box.setDefaultButton(QMessageBox::No);
    box.exec();
    if (box.clickedButton() != box.button(QMessageBox::Yes)) return;

    const auto error = Scanner::ScanManager::instance()->DeleteScan(scanId, alsoResults != nullptr && alsoResults->isChecked());
    if (!error.isEmpty()) {
        MessageBoxWarning(tr("IP Scanner"), error);
        return;
    }
    DialogScanInstance::Discard(scanId);
}

void DialogScanner::moveScan(int from, int to) {
    if (from < 0 || from >= scans.size() || to < 0 || to >= scans.size() || from == to) return;

    scans.move(from, to);
    QList<int> ids;
    ids.reserve(scans.size());
    for (const auto &scan : scans) ids.append(scan->id);
    Configs::dataManager->ipScansRepo->UpdateIpScansOrder(ids);

    reloadScans();
}

void DialogScanner::newScan() {
    DialogNewScan dialog(this);
    if (dialog.exec() != QDialog::Accepted || dialog.CreatedScanId() < 0) return;
    DialogScanInstance::Open(dialog.CreatedScanId());
}
