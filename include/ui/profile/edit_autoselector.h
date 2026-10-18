#pragma once

#include <QPointer>
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_autoselector.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditAutoSelector;
}
QT_END_NAMESPACE

class EditAutoSelector : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditAutoSelector(QWidget *parent = nullptr);

    ~EditAutoSelector() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

private:
    Ui::EditAutoSelector *ui;
    std::shared_ptr<Configs::Profile> ent;
    QPointer<Validation::Form> form;
    bool m_loading = false;
    // Stored groups that no longer exist: only previewed, saving drops them.
    QList<int> m_missingGids;

    [[nodiscard]] QList<int> checkedGroupIDs() const;

    void fillGroups(const QList<int> &tracked);

    void refreshPlanSummary();

    void updateBalanceEnabled() const;

    void refreshPinnedRow() const;

    int m_pinnedID = -1;

    void mirrorTooltipsToLabels() const;

    void resizeDialogToContent();
};
