#pragma once

#include <QPointer>
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

    void bindValidation(Validation::Form &form) override;

private:
    Ui::EditChain *ui;
    std::shared_ptr<Configs::Profile> ent;
    QPointer<Validation::Form> form;
    // Stored hops left out on load: deleted profiles, chains and auto selectors.
    int droppedHops = 0;
    bool stepTestRunning = false;

    // explain: tell the user why a picked profile cannot be a hop.
    bool AddProfileToListIfExist(int profileId, bool explain = true);

    static void ReplaceProfile(ProxyItem *w, int profileId);

    QString hopsError() const;

    QList<int> hopIds() const;

    ProxyItem *hopItem(int row) const;

    void clearStepResults();

    void setStepStatus(const QString &text, const QString &colorRole);

private slots:

    void on_select_profile_clicked();

    void on_test_hops_clicked();
};
