#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_mieru.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditMieru;
}
QT_END_NAMESPACE

class EditMieru : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditMieru(QWidget *parent = nullptr);

    ~EditMieru() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

    bool serverPortOptional() override;

private:
    Ui::EditMieru *ui;
    std::shared_ptr<Configs::Profile> ent;
};
