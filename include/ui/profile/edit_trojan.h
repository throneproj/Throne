#ifndef EDIT_TROJAN_H
#define EDIT_TROJAN_H

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

#endif // EDIT_TROJAN_H
