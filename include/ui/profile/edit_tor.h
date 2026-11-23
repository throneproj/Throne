#pragma once

#include <QWidget>
#include "profile_editor.h"
#include "ui_edit_tor.h"

class QButtonGroup;
class QComboBox;

QT_BEGIN_NAMESPACE
namespace Ui {
    class EditTor;
}
QT_END_NAMESPACE

class EditTor : public QWidget, public ProfileEditor {
    Q_OBJECT

public:
    explicit EditTor(QWidget *parent = nullptr);
    ~EditTor() override;

    void onStart(std::shared_ptr<Configs::Profile> _ent) override;

    bool onEnd() override;

    void bindValidation(Validation::Form &form) override;

    // "Any" plus Configs::torExitCountries; a stored code outside that list is kept as an extra entry.
    static void FillExitCountries(QComboBox *combo, const QString &current);

private:
    [[nodiscard]] QString bridgeMode() const;

    [[nodiscard]] QStringList customLines() const;

    void updateBridgeRows();

    void showTorStatus(const QString &text = {});

    void getBridges();

    void copyBridges();

    void downloadTor();

    Ui::EditTor *ui;
    QButtonGroup *modeButtons;
    std::shared_ptr<Configs::Profile> ent;
};
