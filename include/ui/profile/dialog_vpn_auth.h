#pragma once

#include <QDialog>
#include <QList>
#include <QPair>
#include <QString>
#include "ui_dialog_vpn_auth.h"

namespace Ui {
class DialogVpnAuth;
}

class QAbstractButton;
class QLineEdit;
class QPushButton;
class QTimer;
class QWidget;

// Plain mirror of libcore::VPNChallengeField, so the proto headers stay out of moc's way.
struct VpnAuthField {
    QString submissionKey;
    QString name;
    QString label;
    QString kind;
    QString value;
    QList<QPair<QString, QString>> options;
};

struct VpnAuthChallenge {
    QString endpointTag;
    QString id;
    QString kind;
    QString username;
    QString message;
    QString banner;
    QString error;
    QString url;
    bool echo = false;
    qint64 deadline = 0;
    QList<VpnAuthField> fields;
};

class DialogVpnAuth : public QDialog {
    Q_OBJECT

public:
    // `localOnly`: no challenge exists core-side, so nothing is submitted or cancelled.
    DialogVpnAuth(QWidget *parent, const VpnAuthChallenge &challenge, bool localOnly = false);

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
