#pragma once

#include <QDialog>
#include <QStringList>
#include <functional>
#include "ui_edit_openvpn_advanced.h"
#include "include/database/entities/Profile.h"

namespace Ui {
class EditOpenVPNAdvanced;
}

namespace Validation { class Form; }

class EditOpenVPNAdvanced : public QDialog
{
    Q_OBJECT

public:
    // pageInput writes the profile page's unsaved input into a copy, so the checks see the whole profile.
    EditOpenVPNAdvanced(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent,
                        std::function<void(Configs::openvpn &)> pageInput = {});

    ~EditOpenVPNAdvanced() override;

public slots:
    void accept() override;

private:
    void editPem(QPushButton *button, const QString &title, QStringList &target);

    void addServerRow(const QString &server, const QString &port, const QString &network);

    void addPullFilterRow(const QString &action, const QString &text);

    void bindValidation();

    void saveInto(Configs::openvpn &outbound) const;

    std::shared_ptr<Configs::openvpn> withInput() const;

    Ui::EditOpenVPNAdvanced *ui;
    std::shared_ptr<Configs::Profile> ent;
    std::function<void(Configs::openvpn &)> pageInput;
    Validation::Form *form = nullptr;

    struct {
        QStringList staticKey;
        QStringList peerFingerprint;
    } CACHE;
};
