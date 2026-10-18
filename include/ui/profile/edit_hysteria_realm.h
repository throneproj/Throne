#pragma once

#include <QDialog>
#include "ui_edit_hysteria_realm.h"
#include "include/database/entities/Profile.h"

namespace Ui {
class EditHysteriaRealm;
}

namespace Validation { class Form; }

class EditHysteriaRealm : public QDialog
{
    Q_OBJECT

public:
    EditHysteriaRealm(QWidget *parent, const std::shared_ptr<Configs::Profile> &_ent);

    ~EditHysteriaRealm() override;

public slots:
    void accept() override;

private:
    void syncPortMappingFields();

    Ui::EditHysteriaRealm *ui;
    std::shared_ptr<Configs::Profile> ent;
    Validation::Form *form = nullptr;
};
