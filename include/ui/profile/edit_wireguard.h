#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_wireguard.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditWireguard;
}
QT_END_NAMESPACE

class EditWireguard : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditWireguard(QWidget *parent = nullptr);

    ~EditWireguard() override;

    void onStart(std::shared_ptr<Configs::ProxyEntity> _ent) override;

    bool onEnd() override;

private:
    Ui::EditWireguard *ui;
    std::shared_ptr<Configs::ProxyEntity> ent;
};
