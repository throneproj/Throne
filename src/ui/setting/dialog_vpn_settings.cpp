#include "include/ui/setting/dialog_vpn_settings.h"

#include "include/global/GuiUtils.hpp"
#include "include/global/Configs.hpp"
#include "include/ui/mainwindow_interface.h"
#ifdef Q_OS_WIN
#include "include/sys/windows/WinVersion.h"
#endif

#include <QMessageBox>


#define ADJUST_SIZE runOnThread([=,this] { adjustSize(); adjustPosition(mainwindow); }, this);
DialogVPNSettings::DialogVPNSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogVPNSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);

#ifdef Q_OS_WIN
    if (WinVersion::IsBuildNumGreaterOrEqual(BuildNumber::Windows_10_1507)) {
        ui->vpn_implementation->addItems(Configs::VPNImplementation::VPNImplementation);
        ui->vpn_implementation->setCurrentText(Configs::dataManager->settingsRepo->vpn_implementation);
    }
    else {
        ui->vpn_implementation->addItems(Configs::VPNImplementation::VPNImplementation);
        ui->vpn_implementation->setCurrentText("gvisor");
        ui->vpn_implementation->setEnabled(false);
    }
#else
    ui->vpn_implementation->addItems(Configs::VPNImplementation::VPNImplementation);
    ui->vpn_implementation->setCurrentText(Configs::dataManager->settingsRepo->vpn_implementation);
#endif
    ui->vpn_mtu->setCurrentText(Int2String(Configs::dataManager->settingsRepo->vpn_mtu));
    ui->vpn_ipv6->setChecked(Configs::dataManager->settingsRepo->vpn_ipv6);
    ui->strict_route->setChecked(Configs::dataManager->settingsRepo->vpn_strict_route);
    ui->tun_routing->setChecked(Configs::dataManager->settingsRepo->enable_tun_routing);
    ADJUST_SIZE
}

DialogVPNSettings::~DialogVPNSettings() {
    delete ui;
}

void DialogVPNSettings::accept() {
    //
    auto mtu = ui->vpn_mtu->currentText().toInt();
    if (mtu > 10000 || mtu < 1000) mtu = 9000;
    Configs::dataManager->settingsRepo->vpn_implementation = ui->vpn_implementation->currentText();
    Configs::dataManager->settingsRepo->vpn_mtu = mtu;
    Configs::dataManager->settingsRepo->vpn_ipv6 = ui->vpn_ipv6->isChecked();
    Configs::dataManager->settingsRepo->vpn_strict_route = ui->strict_route->isChecked();
    Configs::dataManager->settingsRepo->enable_tun_routing = ui->tun_routing->isChecked();
    //
    QStringList msg{"UpdateConfigs::dataManager->settingsRepo"};
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
