#pragma once

#include <QDialog>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QString>

#include "include/engine/vpn/VpnAuth.h"
#include "ui_dialog_vpn_auth.h"

namespace Ui {
class DialogVpnAuth;
}

namespace Engine {
    class VpnAuthController;
}

class QAbstractButton;
class QLineEdit;
class QPushButton;
class QTimer;
class QWidget;

class DialogVpnAuth : public QDialog {
    Q_OBJECT

public:
    // `localOnly`: no challenge exists core-side, so nothing is submitted or cancelled.
    DialogVpnAuth(QWidget *parent, Engine::VpnAuthController *auth, const VpnAuthChallenge &challenge, bool localOnly = false);

    ~DialogVpnAuth() override;

    [[nodiscard]] QString enteredUsername() const;

    [[nodiscard]] QString enteredPassword() const;

public slots:
    void reject() override;

private:
    enum class CloseAction {
        Cancel,
        Acknowledge,
        None,
    };

    void buildCredentialFields();

    void buildSecretField();

    void buildFormFields();

    void addRow(const QString &label, QWidget *field);

    void setFieldsEnabled(bool enabled);

    void submit();

    void refreshDeadline();

    void fitToContent();

    Ui::DialogVpnAuth *ui;
    QPointer<Engine::VpnAuthController> auth;
    VpnAuthChallenge challenge;
    bool localOnly = false;
    CloseAction closeAction = CloseAction::Cancel;
    bool submitting = false;
    bool finishing = false;

    QPushButton *submitButton = nullptr;
    QPushButton *cancelButton = nullptr;
    QPushButton *dismissButton = nullptr;
    QAbstractButton *openUrlButton = nullptr;

    QLineEdit *usernameEdit = nullptr;
    QLineEdit *passwordEdit = nullptr;
    QLineEdit *secretEdit = nullptr;
    QList<QPair<VpnAuthField, QWidget *>> formWidgets;

    QTimer *deadlineTimer = nullptr;
};
