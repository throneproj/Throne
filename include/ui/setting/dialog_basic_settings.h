#ifndef DIALOG_BASIC_SETTINGS_H
#define DIALOG_BASIC_SETTINGS_H

#include <QDialog>
#include <QJsonObject>
#include "ui_dialog_basic_settings.h"

namespace Ui {
    class DialogBasicSettings;
}

class DialogBasicSettings : public QDialog {
    Q_OBJECT

public:
    explicit DialogBasicSettings(QWidget *parent = nullptr);

    ~DialogBasicSettings();

public slots:

    void accept();

private:
    Ui::DialogBasicSettings *ui;

    struct {
        QString custom_inbound;
        bool needRestart = false;
        bool updateDisableTray = false;
    } CACHE;

private slots:
    void on_core_settings_clicked();
};

#endif // DIALOG_BASIC_SETTINGS_H
