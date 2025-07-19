#include "include/ui/setting/dialog_hotkey.h"

#include "include/ui/mainwindow_interface.h"

DialogHotkey::DialogHotkey(QWidget *parent) : QDialog(parent), ui(new Ui::DialogHotkey) {
    ui->setupUi(this);
    ui->show_mainwindow->setKeySequence(Configs::dataStore->hotkey_mainwindow);
    ui->show_groups->setKeySequence(Configs::dataStore->hotkey_group);
    ui->show_routes->setKeySequence(Configs::dataStore->hotkey_route);
    ui->system_proxy->setKeySequence(Configs::dataStore->hotkey_system_proxy_menu);
    ui->toggle_proxy->setKeySequence(Configs::dataStore->hotkey_toggle_system_proxy);
    GetMainWindow()->RegisterHotkey(true);
}

DialogHotkey::~DialogHotkey() {
    if (result() == QDialog::Accepted) {
        Configs::dataStore->hotkey_mainwindow = ui->show_mainwindow->keySequence().toString();
        Configs::dataStore->hotkey_group = ui->show_groups->keySequence().toString();
        Configs::dataStore->hotkey_route = ui->show_routes->keySequence().toString();
        Configs::dataStore->hotkey_system_proxy_menu = ui->system_proxy->keySequence().toString();
        Configs::dataStore->hotkey_toggle_system_proxy = ui->toggle_proxy->keySequence().toString();
        Configs::dataStore->Save();
    }
    GetMainWindow()->RegisterHotkey(false);
    delete ui;
}
