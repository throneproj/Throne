#include "include/ui/setting/dialog_vpn_settings.h"

#include "include/configs/proxy/Preset.hpp"
#include "include/global/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/ui/mainwindow_interface.h"

#include <QMessageBox>
#define ADJUST_SIZE runOnUiThread([=] { adjustSize(); adjustPosition(mainwindow); }, this);
DialogVPNSettings::DialogVPNSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogVPNSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);

    ui->vpn_implementation->addItems(Preset::SingBox::VpnImplementation);
    ui->vpn_implementation->setCurrentText(Configs::dataStore->vpn_implementation);
    ui->vpn_mtu->setCurrentText(Int2String(Configs::dataStore->vpn_mtu));
    ui->vpn_ipv6->setChecked(Configs::dataStore->vpn_ipv6);
    ui->strict_route->setChecked(Configs::dataStore->vpn_strict_route);
    ui->tun_routing->setChecked(Configs::dataStore->enable_tun_routing);
    ADJUST_SIZE
}

DialogVPNSettings::~DialogVPNSettings() {
    delete ui;
}

void DialogVPNSettings::accept() {
    //
    auto mtu = ui->vpn_mtu->currentText().toInt();
    if (mtu > 10000 || mtu < 1000) mtu = 9000;
    Configs::dataStore->vpn_implementation = ui->vpn_implementation->currentText();
    Configs::dataStore->vpn_mtu = mtu;
    Configs::dataStore->vpn_ipv6 = ui->vpn_ipv6->isChecked();
    Configs::dataStore->vpn_strict_route = ui->strict_route->isChecked();
    Configs::dataStore->enable_tun_routing = ui->tun_routing->isChecked();
    //
    QStringList msg{"UpdateDataStore"};
    msg << "VPNChanged";
    MW_dialog_message("", msg.join(","));
    QDialog::accept();
}

void DialogVPNSettings::on_troubleshooting_clicked() {


    QMessageBox msg(
        QMessageBox::Information,
        tr("Troubleshooting"),
        tr("If you have trouble starting VPN, you can force reset Core process here.\n\n"
            "If still not working, see documentation for more information.\n"
            "https://matsuridayo.github.io/n-configuration/#vpn-tun"),
        QMessageBox::NoButton,
        this
    );
    msg.addButton(tr("Reset"), QMessageBox::ActionRole);
    auto cancel = msg.addButton(tr("Cancel"), QMessageBox::ActionRole);

    msg.setDefaultButton(cancel);
    msg.setEscapeButton(cancel);

    auto r = msg.exec() - 2;
    if (r == 0) {
        GetMainWindow()->StopVPNProcess();
    }
}
