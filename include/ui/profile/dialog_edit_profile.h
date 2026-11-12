#pragma once
#include <QDialog>
#include <optional>
#include "profile_editor.h"

#include "ui_dialog_edit_profile.h"
#include "include/configs/common/multiplex.h"
#include "include/configs/common/xrayMultiplex.h"
#include "include/database/entities/Profile.h"
#include "include/engine/EditLocks.h"

namespace Ui {
    class DialogEditProfile;
}

class ResolverSourceWidget;

class DialogEditProfile : public QDialog {
    Q_OBJECT

public:
    explicit DialogEditProfile(const QString &_type, int profileOrGroupId, QWidget *parent = nullptr);

    ~DialogEditProfile() override;

public slots:

    void accept() override;

private slots:
    void on_certificate_edit_clicked();
    void on_xray_downloadsettings_edit_clicked();
    void on_xray_finalmask_edit_clicked();
    void on_xray_xdrive_template_edit_clicked();

private:
    Ui::DialogEditProfile *ui;
    Validation::Form *form{};

    QWidget *innerWidget{};
    ProfileEditor *innerEditor{};
    // Stands in for the address field while an endpoint source sets the host; the field keeps the profile's own.
    QLineEdit *addressEffective{};
    QList<QWidget *> outerTabOrder;
    qsizetype innerTabOrderIndex{-1};
    QLabel *xdnsResolversLabel{};
    ResolverSourceWidget *xdnsResolvers{};

    QString type;
    int groupId = -1;
    bool newEnt = false;
    std::shared_ptr<Configs::Profile> ent;
    std::optional<Engine::EditLock> editLock;

    QString network_title_base;

    struct {
        QStringList certificate;
        QString XrayDownloadSettings;
        QJsonObject XrayFinalmask;
        QJsonObject XrayXDriveTemplate;
        // Saved into the profile only while Multiplex is Custom.
        Configs::MuxSettings mux;
        Configs::XrayMuxSettings xrayMux;
    } CACHE;

    // dialog_edit_profile.cpp: profile type, protocol editor, common fields, layout
    void setupTypeList();
    bool typeSelected(const QString &newType);
    void mountEditor(QWidget *widget, ProfileEditor *editor);
    void updateControls();
    void updateCommonRows();
    void relayout();
    void fitToContent();
    bool onEnd();
    void editor_cache_updated_impl();
    void setCacheButtonText(QPushButton *button, bool isSet);
    static void setRowVisible(QWidget *label, QWidget *field, bool visible);
    static void selectComboText(QComboBox *combo, const QString &text);
    static void offerCustomMux(QComboBox *combo, int customIndex);

    // dialog_edit_profile_singbox.cpp: sing-box transport, TLS and multiplex
    void setupSingboxStream();
    void bindSingboxStream();
    void loadSingboxStream();
    void saveSingboxStream();
    void updateSingboxRows();
    void updateTlsControlsEnabled();
    // The profile's own spoof SNI (set in Advanced) while its spoof state is on, else empty.
    QString ownTlsSpoof();
    bool singboxTlsFragmentOn();
    bool singboxUtlsActive();

    // dialog_edit_profile_xray.cpp: Xray stream settings
    void setupXrayStream();
    void bindXrayStream();
    void loadXrayStream();
    void loadXrayNetwork(const QString &network);
    void saveXrayStream();
    void updateXrayRows();
    static void keepXrayComboValue(QComboBox *combo, const QString &value);

    // dialog_edit_profile_xhttp.cpp: the XHTTP panel of the Xray stream
    void setupXrayXHTTPControls();
    void setupXrayXHTTPDescriptions();
    void bindXrayXHTTP();
    void setXrayXHTTPHelp(QWidget *caption, QWidget *field, const QString &text, const QString &jsonKey, const QString &description);
    void loadXrayXHTTP(const Configs::xrayXHTTP &xhttp);
    void saveXrayXHTTP(Configs::xrayXHTTP &xhttp);
    void updateXrayXHTTPControls();

    // dialog_edit_profile_xdrive.cpp: the XDRIVE panel of the Xray stream
    void setupXrayXDriveControls();
    void setupXrayXDriveDescriptions();
    void bindXrayXDrive();
    void loadXrayXDrive(const Configs::xrayXDrive &xdrive);
    void saveXrayXDrive(Configs::xrayXDrive &xdrive);
    void updateXrayXDriveControls();

    // dialog_edit_profile_kcp.cpp: the mKCP panel of the Xray stream and the finalmask checks
    void setupXrayKCPControls();
    void bindXrayKCP();
    void loadXrayKCP(const Configs::xrayKCP &kcp);
    void saveXrayKCP(Configs::xrayKCP &kcp);
};
