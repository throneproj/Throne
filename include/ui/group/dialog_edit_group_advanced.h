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
    // profileCount: the group's profiles now, which a country filter may IP-test on the next update.
    DialogEditGroupAdvanced(const Configs::SubscriptionOptions &options, int serverIntervalHours,
                            const Configs::EndpointSource &endpoint, bool subscription, int profileCount, QWidget *parent = nullptr);

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
    int profileCount = 0;
    // type_exclude ids this version does not know (saved by a newer one), written back unchanged.
    QStringList unknownTypes;
    Validation::Form *form = nullptr;

    void loadEndpoint();

    void loadTypeExclude();

    [[nodiscard]] Configs::EndpointSource::Mode endpointMode() const;

    void syncEndpoint();

    void syncHwidFields();

    void syncUrlTestFollowUps();

    void syncTypeExclude();

    void syncCountryHint();
};
