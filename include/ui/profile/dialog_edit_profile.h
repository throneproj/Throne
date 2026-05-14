#pragma once
#include <QDialog>
#include <QMargins>
#include "profile_editor.h"

#include "include/ui/utils/FloatCheckBox.h"
#include "ui_dialog_edit_profile.h"
#include "include/database/entities/Profile.h"

namespace Ui {
    class DialogEditProfile;
}

class QResizeEvent;
class QSize;
class QShowEvent;
class QSpacerItem;

class DialogEditProfile : public QDialog {
    Q_OBJECT

public:
    explicit DialogEditProfile(const QString &_type, int profileOrGroupId, QWidget *parent = nullptr);

    ~DialogEditProfile() override;

    void toggleSingboxWidgets(bool show);

    void toggleXrayWidgets(bool show);

public slots:

    void accept() override;

private slots:
    void on_certificate_edit_clicked();
    void on_xray_downloadsettings_edit_clicked();
private:
    Ui::DialogEditProfile *ui;

    std::map<QWidget *, FloatCheckBox *> apply_to_group_ui;

    QWidget *innerWidget{};
    ProfileEditor *innerEditor{};

    QString type;
    int groupId;
    bool newEnt = false;
    std::shared_ptr<Configs::Profile> ent;

    QString network_title_base;
    bool postShowFitDone = false;
    bool xhttpLayoutInitialized = false;
    bool xhttpWideLayout = false;
    QMargins xrayLayoutBaseMargins;
    QSpacerItem *xrayTopSpacer{};

    struct {
        QStringList certificate;
        QString XrayDownloadSettings;
    } CACHE;

    void typeSelected(const QString &newType);

    void updateXrayCommons(QString network);

    void setupDialogLayoutBehavior();

    void setupXrayXHTTPControls();

    void updateXrayXHTTPControls();

    void setupXrayXHTTPDescriptions();

    void setXrayXHTTPHelp(QWidget *caption, QWidget *field, const QString &text,
                          const QString &jsonKey, const QString &description);

    void queueRefreshDialogLayout();

    bool updateXrayXHTTPResponsiveLayout();

    void updateDialogColumnSizing();

    QSize visibleDialogContentSize() const;

    void updateXrayVerticalOffset();

    void refreshDialogLayout();

    void fitDialogToContent();

    bool validateHeaders();

    bool validateXrayXHTTPSettings();

    bool onEnd();

    void editor_cache_updated_impl();

    void resizeEvent(QResizeEvent *event) override;

    void showEvent(QShowEvent *event) override;
};
