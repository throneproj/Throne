#pragma once

#include <QPointer>
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_wireguard.h"

namespace Configs {
    struct IpListEntry;
}

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

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

    bool retarget(const std::shared_ptr<Configs::Profile> &profile) override;

private:
    Ui::EditWireguard *ui;
    std::shared_ptr<Configs::Profile> ent;
    QPointer<Validation::Form> form;

    void updateWarpFind();

    void findWarpEndpoint();

    void applyWarpEndpoint(const Configs::IpListEntry &entry, bool junk, bool ownAddress);
};
