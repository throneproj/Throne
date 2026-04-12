#pragma once
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_trusttunnel.h"

namespace Ui {
    class EditTrustTunnel;
}

class EditTrustTunnel : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditTrustTunnel(QWidget *parent = nullptr);

    ~EditTrustTunnel() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    Ui::EditTrustTunnel *ui;
    std::shared_ptr<Configs::Profile> ent;
};
