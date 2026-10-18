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

    void bindValidation(Validation::Form &form) override;

    bool usesQuic() override;

private:
    // ECH and Disable SNI come from the profile (the outer Advanced dialog writes them); REALITY and uTLS from the outer form.
    QString tlsConflict(bool clientRandom, bool customSni) const;

    Ui::EditTrustTunnel *ui;
    std::shared_ptr<Configs::Profile> ent;
};
