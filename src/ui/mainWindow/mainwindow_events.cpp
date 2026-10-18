#include "include/ui/mainwindow.h"

#include <QApplication>
#include <QCursor>
#include <QLineEdit>
#include <QMimeData>
#include <QResizeEvent>
#include <QTimer>

#include "include/engine/connection/ConnectionManager.h"
#include "include/ui/widget/TrayOtpCodes.hpp"
#include "include/ui/widget/TrayProfileSelector.hpp"

void MainWindow::trayClickEvent() {
    constexpr qint64 recentlyActiveMs = 350;
    const bool wasRecentlyActive = isActiveWindow() ||
        (sinceWindowDeactivated.isValid() && sinceWindowDeactivated.elapsed() <= recentlyActiveMs);

    if (isVisible() && !isMinimized() && wasRecentlyActive) {
        HideWindow(this);
    } else {
        ActivateWindow(this);
        refresh_proxy_list_column_size();
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (tray->isVisible()) {
        HideWindow(this);
        event->ignore();
    } else {
        on_menu_exit_triggered();
    }
}

void MainWindow::changeEvent(QEvent *event) {
    const QEvent::Type type = event->type();

    if (type == QEvent::FontChange) {
        // QStyleSheetStyle caches font metrics and ignores FontChange; toggling the stylesheet repolishes.
        auto refreshStylesheetCache = [](QWidget *w) {
            const QString ss = w->styleSheet();
            if (ss.isEmpty()) return;
            w->setStyleSheet("");
            w->setStyleSheet(ss);
        };
        const auto allChildren = findChildren<QWidget*>();
        for (QWidget *w : allChildren) {
            refreshStylesheetCache(w);
        }
        // Never clear the app sheet first: that runs setStyle() and refills Qt's per-class font table over the new font (#1829).
        const QString appSheet = qApp->styleSheet();
        if (!appSheet.isEmpty()) {
            qApp->setStyleSheet(appSheet);
        }
        // After the repolish: with no font rule, QStyleSheetStyle resets a setFont() font to the parent's.
        applyLogBrowserFont();

        // Qt skips setFont when unchanged, so bump the point size to force a real FontChange.
        auto forceFontReapply = [](QWidget *w) {
            if (!w) return;
            const QFont currentFont = QApplication::font();
            QFont diffFont = currentFont;
            diffFont.setPointSize(currentFont.pointSize() + 1);
            w->setFont(diffFont);
            w->setFont(QFont());
            w->updateGeometry();
        };
        forceFontReapply(ui->profilesTableView);

        applyTopBarMetrics();
    }
    if (type == QEvent::FontChange ||
        type == QEvent::PaletteChange ||
        type == QEvent::StyleChange) {
        scheduleProxyListRefresh();
        refreshConnectionIcons();
    }
    if (type == QEvent::WindowStateChange) {
        syncConnectionViewState();
    }
    if (type == QEvent::ActivationChange) {
        // Not stamped from WindowDeactivate in eventFilter(): that only reaches visible filtered children.
        if (isActiveWindow()) sinceWindowDeactivated.invalidate();
        else sinceWindowDeactivated.start();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    syncConnectionViewState();
    scheduleProxyListRefresh();
}

void MainWindow::hideEvent(QHideEvent *event) {
    QMainWindow::hideEvent(event);
    syncConnectionViewState();
}

void MainWindow::syncConnectionViewState() {
    const bool inView = isVisible() && !isMinimized()
        && ui->stats_widget->currentWidget() == ui->connections_tab;
    Stats::connection_lister->SetInView(inView);
}

void MainWindow::scheduleProxyListRefresh() {
    constexpr int proxyListRefreshDebounceMs = 200;
    if (m_proxyListRefreshDebounce) m_proxyListRefreshDebounce->start(proxyListRefreshDebounceMs);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasText()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const auto mimeData = event->mimeData();

    if (mimeData->hasUrls()) {
        QStringList paths;
        for (const QUrl &url : mimeData->urls()) {
            if (url.isLocalFile()) paths << url.toLocalFile();
        }
        if (!paths.isEmpty()) {
            importFromFiles(paths);
            event->acceptProposedAction();
            return;
        }
    }

    if (mimeData->hasText()) {
        import_or_handle_deeplink(mimeData->text());
        event->acceptProposedAction();
        return;
    }

    event->ignore();
}

void MainWindow::openTraySelector(bool routing) {
    // Recreated on each open; WA_DeleteOnClose frees the old one and clears the QPointer.
    if (traySelector) traySelector->close();

    TrayProfileSelector::Callbacks cb;
    cb.startProfile = [this](int id) { profile_start(id); };
    cb.stopProfile = [this]() { profile_stop(false, false, true); };
    cb.chooseRoute = [this](int id) { connection()->chooseRoute(id); };
    cb.isRunning = [this]() { return runningSnapshot() != nullptr; };
    cb.runningId = [this]() {
        const auto running = runningSnapshot();
        return running ? running->id : -1;
    };
    cb.runningGid = [this]() {
        const auto running = runningSnapshot();
        return running ? running->gid : -1;
    };
    cb.runningName = [this]() {
        const auto running = runningSnapshot();
        return running ? running->name : QString();
    };

    traySelector = new TrayProfileSelector(
        routing ? TrayProfileSelector::Routing : TrayProfileSelector::Server, cb, this);
    traySelector->popupAt(QCursor::pos());
}

void MainWindow::openTrayOtpCodes() {
    if (trayOtpCodes) trayOtpCodes->close();
    trayOtpCodes = new TrayOtpCodes(this);
    trayOtpCodes->popupAt(QCursor::pos());
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (!qobject_cast<QLineEdit *>(QApplication::focusWidget())) {
            profile_start();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    const QEvent::Type type = event->type();

    // Accepting the override keeps any window-level Esc shortcut from taking the key first.
    if ((type == QEvent::ShortcutOverride || type == QEvent::KeyPress) && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape
        && obj->isWidgetType() && ui->logToolbar->isAncestorOf(static_cast<QWidget *>(obj))) {
        if (type == QEvent::ShortcutOverride) event->accept();
        else logFilterButton->setChecked(false);
        return true;
    }

    if (type == QEvent::Resize && obj == ui->toolButton_program) {
        const int h = ui->toolButton_program->height();
        if (h > 0 && ui->toolButton_startstop->height() != h) {
            ui->toolButton_startstop->setFixedSize(h, h);
        }
    }
    // The viewport, not the window: splitter moves and scrollbar toggles change its width too.
    if (type == QEvent::Resize && obj == ui->profilesTableView->viewport()) {
        const auto *resize = static_cast<QResizeEvent *>(event);
        if (resize->size().width() != resize->oldSize().width()) refresh_proxy_list_column_size();
    }
    if (type == QEvent::MouseButtonPress) {
        auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
        if (obj == ui->label_running && mouseEvent->button() == Qt::LeftButton && runningSnapshot() != nullptr) {
            url_test_current();
            return true;
        } else if (obj == ui->label_inbound && mouseEvent->button() == Qt::LeftButton) {
            on_menu_basic_settings_triggered();
            return true;
        }
    } else if (type == QEvent::MouseButtonDblClick) {
        if (obj == ui->splitter) {
            const auto size = ui->splitter->size();
            ui->splitter->setSizes({size.height() / 2, size.height() / 2});
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::start_select_mode(QObject *context, const std::function<void(int)> &callback) {
    select_mode = true;
    if (shortcut_select_cancel == nullptr) {
        shortcut_select_cancel = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        shortcut_select_cancel->setContext(Qt::WindowShortcut);
        connect(shortcut_select_cancel, &QShortcut::activated, this, [this] {
            if (select_mode) finish_select_mode(-1);
        });
    }
    shortcut_select_cancel->setEnabled(true);
    connectOnce(this, &MainWindow::profile_selected, context, callback);
    refresh_status();
}

void MainWindow::finish_select_mode(int profileId) {
    if (shortcut_select_cancel != nullptr) shortcut_select_cancel->setEnabled(false);
    emit profile_selected(profileId);
    select_mode = false;
    refresh_status();
}
