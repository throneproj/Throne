#pragma once

#include <QPointer>
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_hysteria.h"

namespace Ui {
    class EditHysteria;
}

class EditHysteria : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditHysteria(QWidget *parent = nullptr);

    ~EditHysteria() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

    bool locksServerAddress() override;

    bool serverPortOptional() override;

    bool serverAddressOptional() override;

private:
    Ui::EditHysteria *ui;
    std::shared_ptr<Configs::Profile> ent;
    QPointer<Validation::Form> form;

    void editHysteriaLayout(const QString& version, const QString& obfs_type);

    bool isV2() const;

    bool realmOn() const;

    bool hopping() const;
};

