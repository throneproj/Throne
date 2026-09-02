#pragma once

#include <QDialog>
#include "ui_edit_wireguard_amnezia.h"
#include "include/database/entities/Profile.h"

namespace Ui {
class EditWireguardAmnezia;
}

class EditWireguardAmnezia : public QDialog
{
    Q_OBJECT

public:
    EditWireguardAmnezia(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent);

    ~EditWireguardAmnezia() override;

public slots:
    void accept() override;

private:
    Ui::EditWireguardAmnezia *ui;
    std::shared_ptr<Configs::Profile> ent;
};
