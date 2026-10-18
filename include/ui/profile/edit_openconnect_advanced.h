#pragma once

#include <QDialog>
#include <QStringList>
#include <functional>
#include "ui_edit_openconnect_advanced.h"
#include "include/database/entities/Profile.h"

namespace Ui {
class EditOpenConnectAdvanced;
}

namespace Validation { class Form; }

class EditOpenConnectAdvanced : public QDialog
{
    Q_OBJECT

public:
    // pageInput writes the profile page's unsaved input into a copy, so the checks see the whole profile.
    EditOpenConnectAdvanced(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent,
                            std::function<void(Configs::openconnect &)> pageInput = {});

    ~EditOpenConnectAdvanced() override;

public slots:
    void accept() override;

private:
    void editPem(QPushButton *button, const QString &title, QStringList &target);

    void addTnccCertificateRow(const QStringList &certificate, const QString &path);

    void editTnccCertificate();

    void addFormEntryRow(const QString &formId, const QString &submissionKey, const QString &name,
                         const QString &value, bool promote);

    void bindValidation();

    void saveInto(Configs::openconnect &outbound) const;

    std::shared_ptr<Configs::openconnect> withInput() const;

    Ui::EditOpenConnectAdvanced *ui;
    std::shared_ptr<Configs::Profile> ent;
    std::function<void(Configs::openconnect &)> pageInput;
    Validation::Form *form = nullptr;

    struct {
        QStringList peerFingerprint;
        QStringList mcaCertificate;
        QStringList mcaKey;
    } CACHE;
};
