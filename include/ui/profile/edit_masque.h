#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_masque.h"

namespace Configs_network {
    struct WarpIdentity;
}

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditMasque;
}
QT_END_NAMESPACE

class EditMasque : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditMasque(QWidget *parent = nullptr);

    ~EditMasque() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

    // The core dials 443 when the port is 0.
    bool serverPortOptional() override { return true; }

private:
    Ui::EditMasque *ui;
    std::shared_ptr<Configs::Profile> ent;

    void generateWarpIdentity();

    void applyWarpIdentity(const Configs_network::WarpIdentity &identity);
};
