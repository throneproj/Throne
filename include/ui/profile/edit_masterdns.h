#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_masterdns.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditMasterDns;
}
QT_END_NAMESPACE

class EditMasterDns : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditMasterDns(QWidget *parent = nullptr);
    ~EditMasterDns() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

private:
    [[nodiscard]] QString method() const;

    Ui::EditMasterDns *ui;
    std::shared_ptr<Configs::Profile> ent;
};
