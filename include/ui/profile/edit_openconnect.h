#pragma once

#include <QPointer>
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_openconnect.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditOpenConnect;
}
QT_END_NAMESPACE

class EditOpenConnect : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditOpenConnect(QWidget *parent = nullptr);

    ~EditOpenConnect() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

    QList<QPair<QPushButton *, QString>> get_editor_cached() override;

    bool serverPortOptional() override;

private:
    void editPem(QPushButton *button, const QString &title, QStringList &target);

    // Writes this page's input: into the live profile on save, into a copy when checking.
    void saveInto(Configs::openconnect &outbound) const;

    std::shared_ptr<Configs::openconnect> withInput() const;

    // The Advanced dialog and the PEM boxes change what the rules see without a focus-out.
    void revalidateInput();

    Ui::EditOpenConnect *ui;
    std::shared_ptr<Configs::Profile> ent;
    QPointer<Validation::Form> form;
    QList<QWidget *> checkedWidgets;

    struct {
        QStringList certificateAuthority;
        QStringList clientCertificate;
        QStringList clientKey;
    } CACHE;
};
