#pragma once

#include <QDialog>
#include "ui_dialog_vpn_settings.h"
#include "include/engine/EditLocks.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogVPNSettings;
}
QT_END_NAMESPACE

namespace Validation {
    class Form;
}

class DialogVPNSettings : public QDialog {
    Q_OBJECT

public:
    explicit DialogVPNSettings(QWidget *parent = nullptr);

    ~DialogVPNSettings() override;

    // Widgets this platform does not offer: the dialog hides them and the settings search skips them.
    static QList<QWidget *> UnavailableWidgets(const Ui::DialogVPNSettings *ui);

private:
    Ui::DialogVPNSettings *ui;

    Validation::Form *form = nullptr;

    Engine::EditLock settingsLock{Engine::EditTarget::Settings};

public slots:

    void accept() override;

    void on_restore_default_addresses_clicked();

    void on_restore_default_ranges_clicked();

    void on_troubleshooting_clicked();
};
