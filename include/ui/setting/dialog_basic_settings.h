#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QTimer>
#include "ui_dialog_basic_settings.h"
#include "include/engine/EditLocks.h"

namespace Ui {
    class DialogBasicSettings;
}

namespace Validation {
    class Form;
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

    Validation::Form *form = nullptr;

    Engine::EditLock settingsLock{Engine::EditTarget::Settings};

    void bindValidation();

    void applyRegexHighlighting();

    void refreshUrlSchemeStatus();

    void downloadXrayGeoAsset(const QString &url, const QString &fileName);

    void rememberGeoAssetUrl(QComboBox *combo, const QString &url, QStringList &history);

    struct {
        QString custom_inbound;
        bool needRestart = false;
        bool updateDisableTray = false;
        bool updateTrayIcon = false;
        bool updateMaxLogLines = false;
        // What the log font boxes showed on open; left untouched, an unset family/size keeps following its default.
        QString shownLogFontFamily;
        int shownLogFontSize = 0;
        bool updateDisableAdmin = false;
    } CACHE;

private slots:
    void on_backup_create_clicked();
    void on_backup_restore_clicked();
    void on_xray_geoip_download_clicked();
    void on_xray_geosite_download_clicked();
};