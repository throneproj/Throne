#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_psiphon.h"

class QComboBox;

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditPsiphon;
}
QT_END_NAMESPACE

class EditPsiphon : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditPsiphon(QWidget *parent = nullptr);
    ~EditPsiphon() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    // "Best performance" plus the regions by name: Configs::psiphonRegions at once, the core's live list once it answers with one. A stored
    // code outside the list is kept as an extra entry.
    static void FillRegions(QComboBox *combo, const QString &current);

private:
    Ui::EditPsiphon *ui;
    std::shared_ptr<Configs::Profile> ent;
};
