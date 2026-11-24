#include "include/ui/mainwindow.h"

#include "include/database/DatabaseManager.h"
#include "include/database/IpScansRepo.h"
#include "include/scanner/ScanManager.h"
#include "include/ui/scanner/dialog_ip_lists.h"
#include "include/ui/scanner/dialog_scanner.h"

void MainWindow::on_menu_scanner_triggered() {
    showScannerDialog();
}

void MainWindow::showScannerDialog() {
    if (m_scannerDialog == nullptr) {
        m_scannerDialog = new DialogScanner(this);
        m_scannerDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    ActivateWindow(m_scannerDialog);
}

void MainWindow::showIpListsDialog(int selectListId) {
    if (m_ipListsDialog == nullptr) {
        m_ipListsDialog = new DialogIpLists(this);
        m_ipListsDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    ActivateWindow(m_ipListsDialog);
    if (selectListId >= 0) m_ipListsDialog->selectList(selectListId);
}

void MainWindow::refreshScannerDataView(bool force) {
    const auto *manager = Scanner::ScanManager::instance();
    QList<DataViewHtmlGenerator::ScannerPanelItem> items;
    for (const int id : manager->RunningScans()) {
        if (manager->IsWatched(id)) continue;
        auto name = m_scannerNames.constFind(id);
        if (name == m_scannerNames.constEnd()) {
            const auto scan = Configs::dataManager->ipScansRepo->GetIpScan(id);
            name = m_scannerNames.insert(id, scan != nullptr ? scan->name : QString());
        }
        const auto live = manager->LiveState(id);
        items.append({name.value(), live.tested, live.total, live.found});
    }
    const bool wasShown = m_scannerPanelShown;
    m_scannerPanelShown = !items.isEmpty();
    if (!wasShown && !m_scannerPanelShown && !force) return;
    dataViewHtmlGenerator_.setScannerPanel(items);
    // The throttle drops updates without a trailing flush, so the one removing the panel is forced.
    UpdateDataView(force || (wasShown && !m_scannerPanelShown));
}
