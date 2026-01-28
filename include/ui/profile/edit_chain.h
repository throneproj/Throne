#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_chain.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditChain;
}
QT_END_NAMESPACE

class ProxyItem;

class EditChain : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditChain(QWidget *parent = nullptr);

    ~EditChain() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

private:
    Ui::EditChain *ui;
    std::shared_ptr<Configs::Profile> ent;

    void AddProfileToListIfExist(int profileId);

    static void ReplaceProfile(ProxyItem *w, int profileId);

private slots:

    void on_select_profile_clicked();
};
