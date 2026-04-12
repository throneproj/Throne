#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_socks.h"

namespace Ui {
    class EditSocks;
}

class EditSocks : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditSocks(QWidget *parent = nullptr);

    ~EditSocks() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    Ui::EditSocks *ui;
    std::shared_ptr<Configs::Profile> ent;
};
