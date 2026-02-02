#ifndef EDIT_SHADOWSOCKS_H
#define EDIT_SHADOWSOCKS_H

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_shadowsocks.h"

namespace Ui {
    class EditShadowSocks;
}

class EditShadowSocks : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditShadowSocks(QWidget *parent = nullptr);

    ~EditShadowSocks() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    Ui::EditShadowSocks *ui;
    std::shared_ptr<Configs::Profile> ent;
};

#endif // EDIT_SHADOWSOCKS_H
