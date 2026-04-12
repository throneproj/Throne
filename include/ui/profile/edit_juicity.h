#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_juicity.h"

namespace Ui {
    class EditJuicity;
}

class EditJuicity : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditJuicity(QWidget *parent = nullptr);

    ~EditJuicity() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    Ui::EditJuicity *ui;
    std::shared_ptr<Configs::Profile> ent;
};