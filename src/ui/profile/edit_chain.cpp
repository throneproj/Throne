#include "include/ui/profile/edit_chain.h"


#include "include/database/ProfilesRepo.h"
#include "include/engine/EngineHost.h"
#include "include/engine/tests/TestRunner.h"
#include "include/ui/mainwindow.h"
#include "include/ui/profile/ProxyItem.h"
#include "include/ui/utils/ValidationForm.h"

namespace {
    // generate.cpp chainScanError over the build order, which reads the list bottom-up.
    QString editChainSegmentError(const QList<std::shared_ptr<Configs::Profile>> &topFirst) {
        const auto count = topFirst.size();
        int extraCores = 0, extraCoreIndex = -1, xrayFullConfigs = 0, xrayFullConfigIndex = -1, xrayHops = 0, transitions = 0;
        bool customFullConfig = false, inXray = false;
        for (qsizetype k = 0; k < count; ++k) {
            const auto &hop = topFirst[count - 1 - k];
            const bool xray = hop->outbound->IsXray();
            if (xray != inXray) transitions++;
            inXray = xray;
            if (hop->outbound->IsExtraCore()) {
                extraCores++;
                extraCoreIndex = int(k);
            }
            if (hop->outbound->IsXrayFullConfig()) {
                xrayFullConfigs++;
                xrayFullConfigIndex = int(k);
            }
            if (xray) xrayHops++;
            if (hop->type == "custom" && hop->Custom() != nullptr && hop->Custom()->type == Configs::Custom::CustomFullConfig) customFullConfig = true;
        }
        if (customFullConfig) return EditChain::tr("A custom %1 config cannot be a hop; only custom outbounds can.").arg(software_core_name);
        if (extraCores > 1) return EditChain::tr("Only one Extra Core profile can be in a chain.");
        if (xrayFullConfigs > 1) return EditChain::tr("Only one custom Xray config can be in a chain.");
        if (extraCores > 0 && xrayFullConfigs > 0) return EditChain::tr("An Extra Core profile and a custom Xray config cannot be in the same chain.");
        if (xrayFullConfigs > 0 && xrayHops > 0) return EditChain::tr("A custom Xray config cannot share a chain with other Xray profiles.");
        // An extra core or a custom Xray config is reached through its local socks port, so nothing can come after it.
        if (extraCores == 1 && count > 1 && extraCoreIndex != count - 1) {
            return EditChain::tr("An Extra Core profile can only be the final hop; move it to the top of the list.");
        }
        if (xrayFullConfigs == 1 && count > 1 && xrayFullConfigIndex != count - 1) {
            return EditChain::tr("A custom Xray config can only be the final hop; move it to the top of the list.");
        }
        if (transitions > 2) {
            return EditChain::tr("The chain switches between %1 and Xray too often; keep the Xray hops together, between %1 hops.").arg(software_core_name);
        }
        return {};
    }

    // The hops above a selector and the ones below it are built as two separate chains.
    QString editChainScanError(const QList<std::shared_ptr<Configs::Profile>> &topFirst) {
        qsizetype selectorAt = -1;
        for (qsizetype i = 0; i < topFirst.size(); ++i) {
            if (topFirst[i]->type != "autoselector") continue;
            if (selectorAt >= 0) return EditChain::tr("A chain can hold only one auto selector.");
            selectorAt = i;
        }
        if (selectorAt < 0) return editChainSegmentError(topFirst);
        for (const auto &hop : topFirst) {
            const bool customFullConfig = hop->type == "custom" && hop->Custom() != nullptr && hop->Custom()->type == Configs::Custom::CustomFullConfig;
            if (hop->outbound->IsExtraCore() || hop->outbound->IsXrayFullConfig() || customFullConfig)
                return EditChain::tr("An auto selector cannot share a chain with an Extra Core profile or a custom config.");
        }
        // The selector stays in the lower part: an Xray hop below it switches back to sing-box to reach it.
        if (auto error = editChainSegmentError(topFirst.mid(selectorAt)); !error.isEmpty()) return error;
        return editChainSegmentError(topFirst.mid(0, selectorAt));
    }

    using StepResult = Engine::TestRunner::ChainStepResult;
    using StepReport = Engine::TestRunner::ChainStepReport;

    struct StepRow {
        QString text;
        QString toolTip;
        QString role;
    };

    struct StepVerdict {
        QList<StepRow> rows;
        QString status;
        QString statusRole;
    };

    bool stepWorks(const StepResult &result) {
        return result.latency > 0 || result.latency == Configs::kLatencyConnectOnly;
    }

    QString stepLatencyText(const StepResult &result) {
        if (result.latency == Configs::kLatencyConnectOnly) return EditChain::tr("Connect OK");
        return EditChain::tr("%1 ms").arg(result.latency);
    }

    QString aloneNote(const QHash<int, StepResult> &alone, int hop) {
        if (!alone.contains(hop)) return {};
        const auto &result = alone[hop];
        return stepWorks(result) ? EditChain::tr("On its own: %1").arg(stepLatencyText(result))
                                 : EditChain::tr("On its own: unavailable");
    }

    QString joinLines(const QStringList &lines) {
        QStringList kept;
        for (const auto &line : lines) {
            if (!line.isEmpty()) kept << line;
        }
        return kept.join(QLatin1Char('\n'));
    }

    StepVerdict judgeSteps(const StepReport &report, const QStringList &names) {
        QHash<int, StepResult> through;
        QHash<int, StepResult> alone;
        for (const auto &result : report.results) (result.alone ? alone : through).insert(result.hop, result);

        StepVerdict out;
        int breakAt = -1;
        for (int i = 0; i < names.size(); ++i) {
            StepRow row;
            if (report.invalidHops.contains(i)) {
                row = {EditChain::tr("Invalid"), EditChain::tr("This profile is not valid; the log has the details."), "danger"};
                if (breakAt < 0) breakAt = i;
            } else if (through.contains(i)) {
                const auto &result = through[i];
                if (stepWorks(result)) {
                    row = {stepLatencyText(result), i == 0 ? EditChain::tr("Works.") : EditChain::tr("Works through every hop above it."), "success"};
                } else if (breakAt >= 0) {
                    row = {EditChain::tr("Not reached"),
                           joinLines({EditChain::tr("A hop above already fails, so this says nothing about this hop."), aloneNote(alone, i)}),
                           "muted"};
                } else {
                    breakAt = i;
                    if (alone.contains(i) && stepWorks(alone[i])) {
                        row = {EditChain::tr("Fails through the hops above"),
                               joinLines({EditChain::tr("Works on its own (%1), but not through the hops above it: the hop above may not reach "
                                                        "this server, or may not carry its protocol (UDP, for example).")
                                              .arg(stepLatencyText(alone[i])),
                                          result.error}),
                               "danger"};
                    } else {
                        row = {EditChain::tr("Unavailable"), joinLines({aloneNote(alone, i), result.error}), "danger"};
                    }
                }
            } else if (alone.contains(i)) {
                const auto &result = alone[i];
                row = {stepWorks(result) ? stepLatencyText(result) : EditChain::tr("Unavailable"),
                       joinLines({EditChain::tr("Tested on its own only: a hop above is not valid."), result.error}), "muted"};
            } else {
                row = {EditChain::tr("Not tested"), {}, "muted"};
            }
            out.rows << row;
        }

        if (!report.error.isEmpty()) {
            out.status = report.error;
            out.statusRole = "danger";
        } else if (report.aborted) {
            out.status = EditChain::tr("The test was stopped.");
            out.statusRole = "muted";
        } else if (breakAt >= 0) {
            out.status = EditChain::tr("The chain breaks at hop %1, %2.").arg(QString::number(breakAt + 1), names[breakAt]);
            out.statusRole = "danger";
        } else if (report.hasLanding) {
            const int landing = static_cast<int>(names.size());
            if (through.contains(landing) && stepWorks(through[landing])) {
                out.status = EditChain::tr("Every hop works, and so does the group's landing proxy after them (%1).").arg(stepLatencyText(through[landing]));
                out.statusRole = "success";
            } else if (alone.contains(landing) && stepWorks(alone[landing])) {
                out.status = EditChain::tr("Every hop works, but the group's landing proxy fails through them.");
                out.statusRole = "danger";
            } else {
                out.status = EditChain::tr("Every hop works, but the group's landing proxy is unavailable.");
                out.statusRole = "danger";
            }
        } else {
            out.status = EditChain::tr("Every hop works through the ones above it.");
            out.statusRole = "success";
        }
        return out;
    }
}

EditChain::EditChain(QWidget *parent) : QWidget(parent), ui(new Ui::EditChain) {
    ui->setupUi(this);
    // Adding, removing, replacing and dragging hops are the list's focus-out.
    const auto onHopsChanged = [this] {
        if (form) form->revalidate(ui->listWidget);
        if (!stepTestRunning && !ui->step_status->isHidden()) clearStepResults();
    };
    auto *model = ui->listWidget->model();
    connect(model, &QAbstractItemModel::rowsInserted, this, onHopsChanged);
    connect(model, &QAbstractItemModel::rowsRemoved, this, onHopsChanged);
    connect(model, &QAbstractItemModel::rowsMoved, this, onHopsChanged);
    connect(model, &QAbstractItemModel::dataChanged, this, onHopsChanged);
}

EditChain::~EditChain() {
    if (stepTestRunning && Engine::host() != nullptr) Engine::host()->tests()->stop();
    delete ui;
}

void EditChain::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Chain();

    droppedHops = 0;
    for (auto id: outbound->list) {
        if (!AddProfileToListIfExist(id)) droppedHops++;
    }
}

bool EditChain::onEnd() {
    if (get_edit_text_name().isEmpty()) {
        MessageBoxWarning(software_name, tr("Name cannot be empty."));
        return false;
    }

    this->ent->Chain()->list = hopIds();
    return true;
}

void EditChain::bindValidation(Validation::Form &form) {
    this->form = &form;
    const auto label = tr("Hops");
    form.addRule({ui->listWidget}, [this] { return hopsError(); }, this, Validation::Severity::Error, label);
    form.addRule({ui->listWidget}, [this] {
        if (droppedHops == 0) return QString();
        return tr("%n hop(s) no longer exist or cannot be hops, so they were left out; saving removes them.", nullptr, droppedHops);
    }, this, Validation::Severity::Warning, label);
}

QList<int> EditChain::hopIds() const {
    QList<int> ids;
    for (int i = 0; i < ui->listWidget->count(); i++) ids << ui->listWidget->item(i)->data(114514).toInt();
    return ids;
}

ProxyItem *EditChain::hopItem(int row) const {
    return qobject_cast<ProxyItem *>(ui->listWidget->itemWidget(ui->listWidget->item(row)));
}

QString EditChain::hopsError() const {
    QList<std::shared_ptr<Configs::Profile>> hops;
    const auto ids = hopIds();
    for (int i = 0; i < ids.size(); i++) {
        auto hop = Configs::dataManager->profilesRepo->GetProfile(ids[i]);
        if (hop == nullptr || hop->outbound == nullptr) return tr("Hop %1 no longer exists; replace or remove it.").arg(i + 1);
        hops << hop;
    }
    if (hops.isEmpty()) return tr("Add at least one profile to the chain.");
    return editChainScanError(hops);
}

void EditChain::clearStepResults() {
    for (int i = 0; i < ui->listWidget->count(); i++) {
        if (auto *item = hopItem(i)) item->refresh_data();
    }
    ui->step_status->hide();
}

void EditChain::setStepStatus(const QString &text, const QString &colorRole) {
    ProxyItem::setColorRole(ui->step_status, colorRole);
    ui->step_status->setText(text);
    ui->step_status->show();
}

void EditChain::on_test_hops_clicked() {
    if (stepTestRunning) return;
    if (const auto error = hopsError(); !error.isEmpty()) {
        clearStepResults();
        setStepStatus(error, "danger");
        return;
    }
    const auto hops = hopIds();
    QStringList names;
    for (int i = 0; i < hops.size(); i++) {
        auto *item = hopItem(i);
        names << (item != nullptr ? item->ent->outbound->DisplayName() : QString());
        if (item != nullptr) item->showStepResult(tr("Testing…"), {}, "muted");
    }
    stepTestRunning = true;
    ui->test_hops->setEnabled(false);
    setStepStatus(tr("Testing each hop on its own and through the hops above it…"), "muted");

    const QPointer<EditChain> self(this);
    const bool started = Engine::host()->tests()->runChainStepTest(hops, ent->gid, [self, hops, names](const StepReport &report) {
        runOnUiThread([self, hops, names, report] {
            if (self == nullptr) return;
            self->stepTestRunning = false;
            self->ui->test_hops->setEnabled(true);
            if (self->hopIds() != hops) {
                self->clearStepResults();
                self->setStepStatus(tr("The hops changed during the test; test again."), "muted");
                return;
            }
            const auto verdict = judgeSteps(report, names);
            for (int i = 0; i < verdict.rows.size(); i++) {
                if (auto *item = self->hopItem(i)) item->showStepResult(verdict.rows[i].text, verdict.rows[i].toolTip, verdict.rows[i].role);
            }
            self->setStepStatus(verdict.status, verdict.statusRole);
        });
    });
    if (!started) {
        stepTestRunning = false;
        ui->test_hops->setEnabled(true);
        clearStepResults();
        setStepStatus(tr("Another test is running; try again when it finishes."), "danger");
    }
}

void EditChain::on_select_profile_clicked() {
    get_edit_dialog()->hide();
    GetMainWindow()->start_select_mode(this, [=,this](int id) {
        get_edit_dialog()->show();
        AddProfileToListIfExist(id);
    });
}

static bool acceptChainHop(const std::shared_ptr<Configs::Profile> &ent) {
    return ent != nullptr && ent->type != "chain";
}

bool EditChain::AddProfileToListIfExist(int profileId) {
    auto _ent = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (!acceptChainHop(_ent)) return false;
    auto wI = new QListWidgetItem();
    wI->setData(114514, profileId);
    auto w = new ProxyItem(this, _ent, wI);
    ui->listWidget->addItem(wI);
    ui->listWidget->setItemWidget(wI, w);
    connect(w->get_change_button(), &QPushButton::clicked, w, [=,this] {
        get_edit_dialog()->hide();
        GetMainWindow()->start_select_mode(w, [=,this](int newId) {
            get_edit_dialog()->show();
            ReplaceProfile(w, newId);
        });
    });
    return true;
}

void EditChain::ReplaceProfile(ProxyItem *w, int profileId) {
    auto _ent = Configs::dataManager->profilesRepo->GetProfile(profileId);
    if (acceptChainHop(_ent)) {
        w->item->setData(114514, profileId);
        w->ent = _ent;
        w->refresh_data();
    }
}
