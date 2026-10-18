#pragma once

#include <QDialog>
#include "ui_dialog_edit_group_advanced.h"
#include "include/database/entities/Group.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogEditGroupAdvanced;
}
QT_END_NAMESPACE

namespace Validation {
    class Form;
}

class DialogEditGroupAdvanced : public QDialog {
    Q_OBJECT

public:
    // serverIntervalHours: the interval the group's server last sent, 0 if none. A plain group gets no subscription sections.
    DialogEditGroupAdvanced(const Configs::SubscriptionOptions &options, int serverIntervalHours,
                            const Configs::EndpointSource &endpoint, bool subscription, QWidget *parent = nullptr);

    ~DialogEditGroupAdvanced() override;

    [[nodiscard]] Configs::SubscriptionOptions Options() const { return options; }

    [[nodiscard]] Configs::EndpointSource Endpoint() const { return endpoint; }

public slots:
    void accept() override;

private:
    Ui::DialogEditGroupAdvanced *ui;
    Configs::SubscriptionOptions options;
    Configs::EndpointSource endpoint;
    bool globalSendHwid = false;
    Validation::Form *form = nullptr;

    void loadEndpoint();

    [[nodiscard]] Configs::EndpointSource::Mode endpointMode() const;

    void syncEndpoint();

    void syncHwidFields();

    void syncUrlTestFollowUps();
};
