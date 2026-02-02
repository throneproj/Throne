#include "include/ui/setting/dialog_hotkey.h"

#include <include/global/GuiUtils.hpp>

#include "include/ui/mainwindow_interface.h"
#include <QAction>




DialogHotkey::DialogHotkey(QWidget *parent, const QList<QAction*>& actions) : QDialog(parent), ui(new Ui::DialogHotkey) {
    ui->setupUi(this);
    ui->show_mainwindow->setKeySequence(Configs::dataManager->settingsRepo->hotkey_mainwindow);
    ui->show_groups->setKeySequence(Configs::dataManager->settingsRepo->hotkey_group);
    ui->show_routes->setKeySequence(Configs::dataManager->settingsRepo->hotkey_route);
    ui->system_proxy->setKeySequence(Configs::dataManager->settingsRepo->hotkey_system_proxy_menu);
    ui->toggle_proxy->setKeySequence(Configs::dataManager->settingsRepo->hotkey_toggle_system_proxy);

    generateShortcutItems(actions);

    GetMainWindow()->RegisterHotkey(true);
}

void DialogHotkey::generateShortcutItems(const QList<QAction*>& actions)
{
    auto widget = new QWidget(ui->shortcut_area);
    auto layout = new QFormLayout(widget);
    widget->setLayout(layout);
    ui->shortcut_area->setWidget(widget);
    for (auto action : actions)
    {
        auto kseq = new QtExtKeySequenceEdit(this);
        if (!action->shortcut().isEmpty()) kseq->setKeySequence(action->shortcut());
        seqEdit2ID[kseq] = action->data().toString();
        layout->addRow(action->text(), kseq);
    }
}

void DialogHotkey::accept()
{
    Configs::dataManager->settingsRepo->hotkey_mainwindow = ui->show_mainwindow->keySequence().toString();
    Configs::dataManager->settingsRepo->hotkey_group = ui->show_groups->keySequence().toString();
    Configs::dataManager->settingsRepo->hotkey_route = ui->show_routes->keySequence().toString();
    Configs::dataManager->settingsRepo->hotkey_system_proxy_menu = ui->system_proxy->keySequence().toString();
    Configs::dataManager->settingsRepo->hotkey_toggle_system_proxy = ui->toggle_proxy->keySequence().toString();

    auto mp = seqEdit2ID.toStdMap();
    for (const auto& [kseq, actionID] : mp)
    {
        Configs::dataManager->settingsRepo->shortcuts[actionID] = kseq->keySequence();
    }
    Configs::dataManager->settingsRepo->Save();

    Configs::dataManager->settingsRepo->Save();
    MW_dialog_message(Dialog_DialogManageHotkeys, "UpdateShortcuts");
    GetMainWindow()->RegisterHotkey(false);
    QDialog::accept();
}

void DialogHotkey::reject()
{
    GetMainWindow()->RegisterHotkey(false);
    QDialog::reject();
}

DialogHotkey::~DialogHotkey() {
    delete ui;
}
