#pragma once
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_trojan.h"

namespace Ui {
    class EditTrojan;
}

class EditTrojan : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditTrojan(QWidget *parent = nullptr);

    ~EditTrojan() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    Ui::EditTrojan *ui;
    std::shared_ptr<Configs::Profile> ent;
};