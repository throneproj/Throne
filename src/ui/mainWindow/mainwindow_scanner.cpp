#include "include/ui/mainwindow.h"

#include <QMessageBox>

#include "include/database/DatabaseManager.h"
#include "include/database/IpScansRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/EditLocks.h"
#include "include/engine/services/ProfileService.h"
#include "include/scanner/ScanManager.h"
#include "include/scanner/ScanProfiles.h"
#include "include/ui/scanner/dialog_ip_lists.h"
#include "include/ui/scanner/dialog_scanner.h"
#include "include/ui/scanner/dialog_warp_finder.h"

namespace {
    void mainWindowScannerNotice(QWidget *parent, const QString &title, const QString &text) {
        auto *box = new QMessageBox(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);
        box->setTextFormat(Qt::PlainText);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    }
}

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

std::shared_ptr<Configs::Profile> MainWindow::warpFinderTarget() {
    const auto *selection = ui->profilesTableView->selectionModel();
    if (selection == nullptr) return nullptr;
    // Whole-row selection: one range one row high is exactly one profile.
    const auto ranges = selection->selection();
    if (ranges.size() != 1 || ranges.constFirst().height() != 1) return nullptr;
    const int id = ranges.constFirst().topLeft().data(ProfilesTableModel::ProfileIdRole).toInt();
    const auto profile = Configs::dataManager->profilesRepo->GetProfile(id);
    return profile != nullptr && Scanner::IsWarpScanProfile(*profile) ? profile : nullptr;
}

void MainWindow::on_menu_find_warp_endpoint_triggered() {
    const auto profile = warpFinderTarget();
    if (profile == nullptr) return;
    const auto title = tr("Find WARP endpoint");
    // Checked up front too, so a refusal does not come after the whole search.
    if (Engine::IsBeingEdited(Engine::EditTarget::Profile, profile->id)) {
        mainWindowScannerNotice(this, title, tr("\"%1\" is open in an editor; close it and try again.").arg(profile->outbound->name));
        return;
    }
    QString error;
    const auto base = Scanner::CloneWarpScanBase(profile, &error);
    if (base == nullptr) {
        mainWindowScannerNotice(this, title, error);
        return;
    }
    auto *finder = new DialogWarpFinder(base, DialogWarpFinder::OverrideOrigin(profile), this);
    connect(finder, &DialogWarpFinder::endpointChosen, this, [this, id = profile->id](const Configs::IpListEntry &entry, bool junk, bool http2) {
        applyFoundWarpEndpoint(id, entry, junk, http2);
    });
    finder->show();
}

void MainWindow::applyFoundWarpEndpoint(int profileId, const Configs::IpListEntry &entry, bool junk, bool http2) {
    const auto title = tr("Find WARP endpoint");
    const auto profile = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (profile == nullptr || profile->outbound == nullptr) {
        mainWindowScannerNotice(this, title, tr("The profile was deleted meanwhile."));
        return;
    }
    const QString name = profile->outbound->name;
    // The editor would overwrite the change with its own copy on OK.
    if (Engine::IsBeingEdited(Engine::EditTarget::Profile, profileId)) {
        mainWindowScannerNotice(this, title, tr("\"%1\" is open in an editor; close it and try again.").arg(name));
        return;
    }
    if (!Scanner::ApplyWarpEndpoint(*profile, entry, junk, http2)) {
        mainWindowScannerNotice(this, title, tr("\"%1\" is no longer a WARP profile.").arg(name));
        return;
    }
    profile->ClearTestResults();
    if (Engine::ProfileService::Save({profile})) mainWindowScannerNotice(this, title, tr("The profile was deleted meanwhile."));
}
