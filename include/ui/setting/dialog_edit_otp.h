#pragma once

#include <QDialog>
#include <QTimer>
#include <memory>

#include "include/database/entities/OtpProfile.h"
#include "ui_dialog_edit_otp.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogEditOtp;
}
QT_END_NAMESPACE

namespace Validation {
    class Form;
}

// The caller's profile is only written when exec() returns Accepted.
class DialogEditOtp : public QDialog {
    Q_OBJECT

public:
    explicit DialogEditOtp(QWidget *parent, std::shared_ptr<Configs::OtpProfile> profile);

    ~DialogEditOtp() override;

public slots:
    void accept() override;

protected:
    void changeEvent(QEvent *event) override;

private:
    Ui::DialogEditOtp *ui;

    std::shared_ptr<Configs::OtpProfile> profile;

    QTimer *previewTimer;

    Validation::Form *form = nullptr;

    // Empty return means the form is usable.
    QString collect(Configs::OtpProfile &out) const;

    void applyIconColors() const;

    void updateTypeFields() const;

    void updatePreview() const;
};
