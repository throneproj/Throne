#pragma once

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

    bool locksServerAddress() override;

private:
    Ui::EditHysteria *ui;
    std::shared_ptr<Configs::Profile> ent;

    void editHysteriaLayout(const QString& version, const QString& obfs_type);
};

