#pragma once

#include <QPointer>
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_dnstt.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditDnstt;
}
QT_END_NAMESPACE

class EditDnstt : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditDnstt(QWidget *parent = nullptr);
    ~EditDnstt() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

private:
    [[nodiscard]] QString mode() const;
    [[nodiscard]] bool compat() const;
    [[nodiscard]] bool vayNative() const;
    [[nodiscard]] QString backend() const;

    void updateControls();
    void updatePayload();

    Ui::EditDnstt *ui;
    std::shared_ptr<Configs::Profile> ent;
    QPointer<Validation::Form> form;
};
