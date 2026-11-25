#pragma once

#include <QPointer>
#include <QTimer>
#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_autoselector.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditAutoSelector;
}
QT_END_NAMESPACE

namespace Configs {
    struct AutoSelectorPlan;
}

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
    QTimer *m_planTimer = nullptr;
    quint64 m_planGeneration = 0;

    [[nodiscard]] QList<int> checkedGroupIDs() const;

    void fillGroups(const QList<int> &tracked);

    // Debounced planNow().
    void refreshPlanSummary();

    // Plans the form's values on a worker thread; only the newest result reaches showPlan().
    void planNow();

    void showPlan(const Configs::AutoSelectorPlan &plan, bool prioritized);

    [[nodiscard]] bool prioritySelected() const;

    void updateStrategyControls() const;

    void setTierRows(const QList<Configs::autoSelector::Tier> &tiers);

    void appendTierRow(const Configs::autoSelector::Tier &tier);

    [[nodiscard]] QList<Configs::autoSelector::Tier> tierRows() const;

    void moveTier(int offset);

    void onTiersChanged(bool revalidate);

    void updateTierButtons() const;

    // True when the table's height changed.
    bool fitTierTable() const;

    void chainTierTabOrder() const;

    [[nodiscard]] QString tierIssue() const;

    void updateBalanceEnabled() const;

    void refreshPinnedRow() const;

    int m_pinnedID = -1;

    void mirrorTooltipsToLabels() const;

    void resizeDialogToContent();
};
