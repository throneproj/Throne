#pragma once

#include <QPointer>
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_openvpn.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditOpenVPN;
}
QT_END_NAMESPACE

class EditOpenVPN : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditOpenVPN(QWidget *parent = nullptr);

    ~EditOpenVPN() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

    QList<QPair<QPushButton *, QString>> get_editor_cached() override;

    bool serverPortOptional() override;

    bool serverAddressOptional() override;

private:
    void editPem(QPushButton *button, const QString &title, QStringList &target);

    // Writes this page's input: into the live profile on save, into a copy when checking.
    void saveInto(Configs::openvpn &outbound) const;

    std::shared_ptr<Configs::openvpn> withInput() const;

    // The Advanced dialog and the PEM boxes change what the rules see without a focus-out.
    void revalidateInput();

    Ui::EditOpenVPN *ui;
    std::shared_ptr<Configs::Profile> ent;
    QPointer<Validation::Form> form;
    QList<QWidget *> checkedWidgets;

    struct {
        QStringList certificate;
        QStringList clientCertificate;
        QStringList clientKey;
        QStringList controlWrapKey;
    } CACHE;
};
