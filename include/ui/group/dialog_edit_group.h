#pragma once

#include <QDialog>
#include <QHash>
#include <optional>
#include "ui_dialog_edit_group.h"
#include "include/database/entities/Group.h"
#include "include/engine/EditLocks.h"

class QComboBox;

namespace Validation {
    class Form;
}

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogEditGroup;
}
QT_END_NAMESPACE

class DialogEditGroup : public QDialog {
    Q_OBJECT

public:
    explicit DialogEditGroup(const std::shared_ptr<Configs::Group> &ent, QWidget *parent = nullptr);

    ~DialogEditGroup() override;

    // The accepted edit changed the endpoint of a group whose inheriting profile the running config uses.
    [[nodiscard]] bool RestartNeeded() const { return restartNeeded; }

private:
    Ui::DialogEditGroup *ui;

    std::shared_ptr<Configs::Group> ent;

    std::optional<Engine::EditLock> editLock;

    struct {
        int front_proxy;
    } CACHE;

    struct {
        int landing_proxy;
    } LANDING;

    Configs::SubscriptionOptions subOptions;

    Configs::EndpointSource endpoint;

    bool restartNeeded = false;

    QHash<QString, int> proxyNameToId;

    Validation::Form *form = nullptr;

    int resolve_proxy_selection(QComboBox *combo, int fallback) const;

private slots:

    void accept() override;

    QString get_proxy_name(int id);
};
