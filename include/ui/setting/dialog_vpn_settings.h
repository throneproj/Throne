#pragma once

#include <QDialog>
#include "ui_dialog_vpn_settings.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogVPNSettings;
}
QT_END_NAMESPACE

class DialogVPNSettings : public QDialog {
    Q_OBJECT

public:
    explicit DialogVPNSettings(QWidget *parent = nullptr);

    ~DialogVPNSettings() override;

private:
    Ui::DialogVPNSettings *ui;

public slots:

    void accept() override;

    void on_restore_default_addresses_clicked();

    void on_troubleshooting_clicked();
};
