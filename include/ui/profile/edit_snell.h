#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_snell.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditSnell;
}
QT_END_NAMESPACE

class EditSnell : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditSnell(QWidget *parent = nullptr);

    ~EditSnell() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    void applyVersion();

    Ui::EditSnell *ui;
    std::shared_ptr<Configs::Profile> ent;
};
