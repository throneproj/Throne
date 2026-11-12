#include "include/ui/profile/edit_autoselector.h"

#include "include/configs/AutoSelectorPlan.h"
#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/entities/Group.h"
#include "include/database/entities/Profile.h"
#include "include/stats/autoselector/AutoSelectorMonitor.hpp"
#include "include/ui/utils/NoWheelComboBox.h"
#include "include/ui/utils/ValidationForm.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QScrollBar>
#include <QSet>
#include <QTableWidget>

#include <algorithm>

EditAutoSelector::EditAutoSelector(QWidget *parent) : QWidget(parent), ui(new Ui::EditAutoSelector) {
    ui->setupUi(this);

    ui->balance_mode->addItem(tr("Rotate on a timer (keeps sessions stable)"), "rotate");
    ui->balance_mode->addItem(tr("Per connection (widest spread)"), "connection");

    ui->strategy->addItem(tr("Fastest and steadiest"), "latency");
    ui->strategy->addItem(tr("In priority order"), "priority");
    ui->tiers->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tiers->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    updateStrategyControls();
    fitTierTable();
    updateTierButtons();

    ui->advanced_area->setVisible(false);
    connect(ui->advanced_toggle, &QPushButton::toggled, this, [this](bool on) {
        ui->advanced_area->setVisible(on);
        ui->advanced_toggle->setText(on ? tr("Advanced ▴") : tr("Advanced…"));
        resizeDialogToContent();
    });

    mirrorTooltipsToLabels();

    // Unlike the rest of this form, releasing the pin is written immediately, not in onEnd().
    connect(ui->pinned_clear, &QPushButton::clicked, this, [this] {
        m_pinnedID = -1;
        refreshPinnedRow();
        if (ent != nullptr) {
            if (auto outbound = ent->AutoSelector(); outbound != nullptr) {
                outbound->pinnedID = -1;
                Configs::dataManager->profilesRepo->Save(ent);
            }
        }
        // The running core holds its own copy of the pin.
        if (Stats::autoSelectorMonitor->Active()) {
            (void) Stats::autoSelectorMonitor->RequestSelect({});
        }
    });

    connect(ui->balance, &QCheckBox::toggled, this, [this] { updateBalanceEnabled(); });
    connect(ui->balance_mode, &QComboBox::currentIndexChanged, this, [this] { updateBalanceEnabled(); });

    connect(ui->strategy, &QComboBox::currentIndexChanged, this, [this] {
        updateStrategyControls();
        updateBalanceEnabled();
        if (!m_loading) resizeDialogToContent();
        refreshPlanSummary();
    });
    connect(ui->tier_add, &QPushButton::clicked, this, [this] {
        appendTierRow({});
        chainTierTabOrder();
        const int row = ui->tiers->rowCount() - 1;
        ui->tiers->setCurrentCell(row, 1);
        if (auto *names = ui->tiers->cellWidget(row, 1)) names->setFocus();
        // A blank row is checked once its editor loses focus, not the moment it appears.
        onTiersChanged(false);
    });
    connect(ui->tier_remove, &QPushButton::clicked, this, [this] {
        const int row = ui->tiers->currentRow();
        if (row < 0) return;
        ui->tiers->removeRow(row);
        onTiersChanged(true);
    });
    connect(ui->tier_up, &QPushButton::clicked, this, [this] { moveTier(-1); });
    connect(ui->tier_down, &QPushButton::clicked, this, [this] { moveTier(1); });
    connect(ui->tiers, &QTableWidget::currentCellChanged, this, [this] { updateTierButtons(); });
    connect(ui->passive, &QCheckBox::toggled, this, [this](bool on) {
        ui->site_memory_l->setEnabled(on);
        ui->site_memory->setEnabled(on);
    });

    connect(ui->groups, &QListWidget::itemChanged, this, [this] {
        if (form) form->revalidate(ui->groups);
        refreshPlanSummary();
    });
    connect(ui->name_filter, &QLineEdit::textChanged, this, [this] { refreshPlanSummary(); });
    connect(ui->country_filter, &QLineEdit::textChanged, this, [this] { refreshPlanSummary(); });
    connect(ui->exclude_unavailable, &QCheckBox::toggled, this, [this] { refreshPlanSummary(); });
    connect(ui->pool_cap, &QSpinBox::valueChanged, this, [this] { refreshPlanSummary(); });
    connect(ui->build_limit, &QSpinBox::valueChanged, this, [this] { refreshPlanSummary(); });
    connect(ui->result_validity, &QSpinBox::valueChanged, this, [this] { refreshPlanSummary(); });
}

EditAutoSelector::~EditAutoSelector() {
    delete ui;
}

// Hiding a child never shrinks the dialog: parent layouts cache their last size, so the resize is only correct one event-loop pass after the invalidation.
void EditAutoSelector::resizeDialogToContent() {
    auto *dialog = get_edit_dialog ? get_edit_dialog() : nullptr;
    if (dialog == nullptr) return;
    if (layout() != nullptr) layout()->invalidate();
    updateGeometry();

    runOnThread([this, dialog] {
        // QScrollArea::sizeHint() caps at 36 font heights, and the nested form layouts stay inert until activated.
        for (auto *nested : ui->advanced_content->findChildren<QLayout *>()) nested->activate();
        ui->advanced_area->setMinimumWidth(ui->advanced_content->sizeHint().width() + 2 * ui->advanced_area->frameWidth() +
                                           ui->advanced_area->verticalScrollBar()->sizeHint().width());
        if (auto *dialogLayout = dialog->layout()) dialogLayout->activate();

        const auto *screen = dialog->screen() != nullptr ? dialog->screen() : QGuiApplication::primaryScreen();
        if (screen == nullptr) return;
        const auto available = screen->availableGeometry();
        // Title bar and borders; zero until shown.
        const auto frame = dialog->frameGeometry().size() - dialog->size();
        // adjustSize() would clamp to 2/3 of the screen, so bound it here instead.
        dialog->resize(dialog->sizeHint().boundedTo(available.size() - frame));
        if (!dialog->isVisible()) return;

        // Growing keeps the top-left corner, which can still push the buttons below the screen edge.
        const auto outer = dialog->size() + frame;
        const int x = std::clamp(dialog->x(), available.left(),
                                 std::max(available.left(), available.x() + available.width() - outer.width()));
        const int y = std::clamp(dialog->y(), available.top(),
                                 std::max(available.top(), available.y() + available.height() - outer.height()));
        if (QPoint(x, y) != dialog->pos()) dialog->move(x, y);
    }, this);
}

void EditAutoSelector::mirrorTooltipsToLabels() const {
    for (auto *form : findChildren<QFormLayout *>()) {
        for (int row = 0; row < form->rowCount(); row++) {
            auto *labelItem = form->itemAt(row, QFormLayout::LabelRole);
            auto *fieldItem = form->itemAt(row, QFormLayout::FieldRole);
            if (labelItem == nullptr || fieldItem == nullptr) continue;
            auto *label = labelItem->widget();
            auto *field = fieldItem->widget();
            if (label == nullptr || field == nullptr) continue;
            if (const auto tip = field->toolTip(); !tip.isEmpty()) label->setToolTip(tip);
        }
    }
}

void EditAutoSelector::fillGroups(const QList<int> &tracked) {
    ui->groups->clear();
    m_missingGids.clear();
    const QSet<int> trackedSet(tracked.begin(), tracked.end());
    auto order = Configs::dataManager->groupsRepo->GetGroupsTabOrder();
    for (int gid : tracked) {
        if (!order.contains(gid)) order << gid;
    }
    for (int gid : order) {
        auto group = Configs::dataManager->groupsRepo->GetGroup(gid);
        if (group == nullptr) {
            if (trackedSet.contains(gid) && !m_missingGids.contains(gid)) m_missingGids << gid;
            continue;
        }
        // An archived group is listed only while tracked, so saving never drops it unseen.
        if (group->archive && !trackedSet.contains(gid)) continue;
        auto *item = new QListWidgetItem(group->name);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setData(Qt::UserRole, gid);
        item->setCheckState(trackedSet.contains(gid) ? Qt::Checked : Qt::Unchecked);
        ui->groups->addItem(item);
    }

    // Up to five rows, then it scrolls; measured from a real row so fonts and scaling never clip it.
    const int rowHeight = std::max(ui->groups->count() > 0 ? ui->groups->sizeHintForRow(0) : 0,
                                   ui->groups->fontMetrics().height());
    const int rows = qBound(1, ui->groups->count(), 5);
    ui->groups->setFixedHeight(rows * rowHeight + 2 * ui->groups->frameWidth());
}

QList<int> EditAutoSelector::checkedGroupIDs() const {
    QList<int> ids;
    for (int i = 0; i < ui->groups->count(); i++) {
        const auto *item = ui->groups->item(i);
        if (item->checkState() == Qt::Checked) ids << item->data(Qt::UserRole).toInt();
    }
    return ids;
}

void EditAutoSelector::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->AutoSelector();
    if (outbound == nullptr) return;

    // Every setValue below fires valueChanged, which would replan against a half-filled form.
    m_loading = true;

    fillGroups(outbound->gids.isEmpty() ? QList<int>{this->ent->gid} : outbound->gids);

    ui->name_filter->setText(outbound->nameFilter);
    ui->country_filter->setText(outbound->countryFilter);
    ui->exclude_unavailable->setChecked(outbound->excludeUnavailable);
    ui->pool_cap->setMaximum(Configs::autoSelector::kMaxPoolCap);
    ui->pool_cap->setValue(outbound->poolCap);
    ui->build_limit->setMaximum(Configs::autoSelector::kMaxBuildLimit);
    ui->build_limit->setValue(outbound->buildLimit);
    ui->result_validity->setValue(outbound->resultValidityMins);

    ui->test_url->setText(outbound->testURL);
    ui->connectivity_url->setText(outbound->connectivityURL);
    ui->interval->setValue(outbound->intervalSec);
    m_pinnedID = outbound->pinnedID;
    refreshPinnedRow();
    ui->bench_interval->setValue(outbound->benchIntervalSec);
    ui->watch_interval->setValue(outbound->watchIntervalSec);
    ui->active_size->setValue(outbound->activeSize);
    ui->sampling->setValue(outbound->sampling);
    ui->tolerance->setValue(outbound->toleranceMs);
    ui->max_rtt->setValue(outbound->maxRTTms);
    ui->dial_retries->setValue(outbound->dialRetries);
    ui->interrupt_on_switch->setChecked(outbound->interruptOnSwitch);

    ui->balance->setChecked(outbound->balance);
    ui->expected->setValue(outbound->expected);
    if (const int idx = ui->balance_mode->findData(outbound->balanceMode); idx >= 0)
        ui->balance_mode->setCurrentIndex(idx);
    ui->balance_interval->setValue(outbound->balanceIntervalSec);

    if (const int idx = ui->strategy->findData(outbound->strategy); idx >= 0) ui->strategy->setCurrentIndex(idx);
    setTierRows(outbound->tiers);
    ui->failback_checks->setValue(outbound->failbackChecks);
    ui->failback_delay->setValue(outbound->failbackDelaySec);
    ui->failback_interrupt->setChecked(outbound->failbackInterrupt);
    ui->passive->setChecked(outbound->passive);
    ui->site_memory->setValue(outbound->siteMemoryMins);

    m_loading = false;
    updateStrategyControls();
    updateBalanceEnabled();
    refreshPlanSummary();

    // Fields of a collapsed section are not checked, so a stored bad value has to be shown.
    using namespace Configs::Checks;
    const bool advancedInvalid = !AutoSelectorCountries(outbound->countryFilter).isEmpty() ||
                                 !AutoSelectorUrl(outbound->testURL).isEmpty() ||
                                 !AutoSelectorUrl(outbound->connectivityURL).isEmpty();
    if (advancedInvalid) ui->advanced_toggle->setChecked(true);
}

bool EditAutoSelector::onEnd() {
    if (get_edit_text_name().isEmpty()) {
        MessageBoxWarning(software_name, tr("Name cannot be empty."));
        return false;
    }
    auto outbound = this->ent->AutoSelector();
    if (outbound == nullptr) return false;

    const auto gids = checkedGroupIDs();
    if (gids.isEmpty()) {
        MessageBoxWarning(software_name, tr("Check at least one group for this auto selector to track."));
        return false;
    }
    outbound->gids = gids;
    outbound->nameFilter = ui->name_filter->text().trimmed();
    outbound->countryFilter = ui->country_filter->text().trimmed();
    outbound->excludeUnavailable = ui->exclude_unavailable->isChecked();
    outbound->poolCap = ui->pool_cap->value();
    outbound->buildLimit = ui->build_limit->value();
    outbound->resultValidityMins = ui->result_validity->value();

    outbound->testURL = ui->test_url->text().trimmed();
    outbound->connectivityURL = ui->connectivity_url->text().trimmed();
    outbound->intervalSec = ui->interval->value();
    outbound->pinnedID = m_pinnedID;
    outbound->benchIntervalSec = ui->bench_interval->value();
    outbound->watchIntervalSec = ui->watch_interval->value();
    outbound->activeSize = ui->active_size->value();
    outbound->sampling = ui->sampling->value();
    outbound->toleranceMs = ui->tolerance->value();
    outbound->maxRTTms = ui->max_rtt->value();
    outbound->dialRetries = ui->dial_retries->value();
    outbound->interruptOnSwitch = ui->interrupt_on_switch->isChecked();

    outbound->balance = ui->balance->isChecked();
    outbound->expected = ui->expected->value();
    outbound->balanceMode = ui->balance_mode->currentData().toString();
    outbound->balanceIntervalSec = ui->balance_interval->value();
    outbound->strategy = ui->strategy->currentData().toString();
    outbound->tiers = tierRows();
    outbound->failbackChecks = ui->failback_checks->value();
    outbound->failbackDelaySec = ui->failback_delay->value();
    outbound->failbackInterrupt = ui->failback_interrupt->isChecked();
    outbound->passive = ui->passive->isChecked();
    outbound->siteMemoryMins = ui->site_memory->value();
    // The cross-field rules have passed, so this only clamps what the spin boxes cannot produce.
    outbound->Normalize();

    // The pool is a ranking, so a filter change can leave entries that no longer qualify.
    const auto candidates = Configs::AutoSelectorRankingCandidates(this->ent);
    const QSet<int> eligible(candidates.begin(), candidates.end());
    QList<int> pool;
    for (int id : outbound->pool) {
        if (eligible.contains(id)) pool << id;
    }
    outbound->pool = pool;
    return true;
}

void EditAutoSelector::bindValidation(Validation::Form &form) {
    using namespace Validation;
    using namespace Configs::Checks;
    this->form = &form;
    form.addRule({ui->groups}, [this] {
        return checkedGroupIDs().isEmpty() ? tr("Check at least one group for this auto selector to track.") : QString();
    }, this);
    form.bind(ui->name_filter, Rules::Regex(), this);
    form.bind(ui->country_filter, AutoSelectorCountries, this);
    form.bind(ui->test_url, AutoSelectorUrl, this);
    form.bind(ui->connectivity_url, AutoSelectorUrl, this);
    form.addRule({ui->build_limit, ui->pool_cap}, [this] {
        return AutoSelectorBuildLimit(ui->build_limit->value(), ui->pool_cap->value());
    }, this);
    form.addRule({ui->bench_interval, ui->interval}, [this] {
        return AutoSelectorBenchInterval(ui->bench_interval->value(), ui->interval->value());
    }, this);
    form.addRule({ui->watch_interval, ui->interval}, [this] {
        return AutoSelectorWatchInterval(ui->watch_interval->value(), ui->interval->value());
    }, this);
    form.addRule({ui->active_size, ui->expected}, [this] {
        return AutoSelectorReadyCount(ui->active_size->value(), ui->expected->value());
    }, this);
    form.addRule({ui->active_size, ui->build_limit}, [this] {
        return AutoSelectorCloseCount(ui->active_size->value(), ui->build_limit->value());
    }, this);
    form.addRule({ui->tiers}, [this] { return tierIssue(); }, this);
    // Sampling stays checkable in latency mode, where failback is not used.
    form.addRule({ui->failback_checks, ui->sampling}, [this] {
        if (!prioritySelected()) return QString();
        return AutoSelectorFailbackChecks(ui->failback_checks->value(), ui->sampling->value());
    }, this);

    // The rules above skip a collapsed section, so its problems are reported here and expand it when revealed.
    form.addIssueSource([this] {
        QList<FormIssue> issues;
        if (!ui->advanced_area->isHidden()) return issues;
        const auto add = [this, &issues](QWidget *widget, const QString &message) {
            if (message.isEmpty()) return;
            issues.append({Severity::Error, {}, message, widget, [this, widget]() -> QWidget * {
                ui->advanced_toggle->setChecked(true);
                return widget;
            }});
        };
        add(ui->country_filter, AutoSelectorCountries(ui->country_filter->text().trimmed()));
        add(ui->test_url, AutoSelectorUrl(ui->test_url->text().trimmed()));
        add(ui->connectivity_url, AutoSelectorUrl(ui->connectivity_url->text().trimmed()));
        add(ui->build_limit, AutoSelectorBuildLimit(ui->build_limit->value(), ui->pool_cap->value()));
        add(ui->bench_interval, AutoSelectorBenchInterval(ui->bench_interval->value(), ui->interval->value()));
        add(ui->watch_interval, AutoSelectorWatchInterval(ui->watch_interval->value(), ui->interval->value()));
        add(ui->active_size, AutoSelectorReadyCount(ui->active_size->value(), ui->expected->value()));
        add(ui->active_size, AutoSelectorCloseCount(ui->active_size->value(), ui->build_limit->value()));
        if (prioritySelected()) {
            add(ui->failback_checks, AutoSelectorFailbackChecks(ui->failback_checks->value(), ui->sampling->value()));
        }
        return issues;
    }, this);
}

bool EditAutoSelector::prioritySelected() const {
    return ui->strategy->currentData().toString() == "priority";
}

void EditAutoSelector::updateStrategyControls() const {
    const bool prioritized = prioritySelected();
    ui->tiers_l->setVisible(prioritized);
    ui->tiers_area->setVisible(prioritized);
    for (QWidget *widget : std::initializer_list<QWidget *>{ui->failback_checks_l, ui->failback_checks, ui->failback_delay_l,
                                                            ui->failback_delay, ui->failback_interrupt}) {
        widget->setEnabled(prioritized);
    }
}

void EditAutoSelector::setTierRows(const QList<Configs::autoSelector::Tier> &tiers) {
    ui->tiers->setRowCount(0);
    for (const auto &tier : tiers) appendTierRow(tier);
    chainTierTabOrder();
    fitTierTable();
    updateTierButtons();
}

void EditAutoSelector::appendTierRow(const Configs::autoSelector::Tier &tier) {
    const int row = ui->tiers->rowCount();
    ui->tiers->insertRow(row);

    auto *group = new NoWheelComboBox(ui->tiers);
    group->addItem(tr("Any group"), -1);
    for (int i = 0; i < ui->groups->count(); i++) {
        const auto *item = ui->groups->item(i);
        group->addItem(item->text(), item->data(Qt::UserRole));
    }
    // Kept selectable so saving never turns a tier for an unlisted group into "any group".
    if (tier.gid >= 0 && group->findData(tier.gid) < 0) {
        const auto stored = Configs::dataManager->groupsRepo->GetGroup(tier.gid);
        group->addItem(stored != nullptr ? stored->name : tr("missing group"), tier.gid);
    }
    group->setCurrentIndex(std::max(0, group->findData(tier.gid)));
    ui->tiers->setCellWidget(row, 0, group);

    auto *names = new QLineEdit(tier.nameFilter, ui->tiers);
    names->setPlaceholderText(tr("any name"));
    names->setToolTip(tr("Optional regular expression, matched anywhere in the profile name and ignoring case, for example ^DE or germany|nl."));
    ui->tiers->setCellWidget(row, 1, names);

    connect(group, &QComboBox::currentIndexChanged, this, [this] { onTiersChanged(true); });
    connect(names, &QLineEdit::textChanged, this, [this] { refreshPlanSummary(); });
    // The tier rule is anchored on the table, which never sees its cell editors' focus-out.
    connect(names, &QLineEdit::editingFinished, this, [this] {
        if (form) form->revalidate(ui->tiers);
    });
}

QList<Configs::autoSelector::Tier> EditAutoSelector::tierRows() const {
    QList<Configs::autoSelector::Tier> tiers;
    for (int row = 0; row < ui->tiers->rowCount(); row++) {
        Configs::autoSelector::Tier tier;
        if (const auto *group = qobject_cast<QComboBox *>(ui->tiers->cellWidget(row, 0))) tier.gid = group->currentData().toInt();
        if (const auto *names = qobject_cast<QLineEdit *>(ui->tiers->cellWidget(row, 1))) tier.nameFilter = names->text().trimmed();
        tiers << tier;
    }
    return tiers;
}

void EditAutoSelector::moveTier(int offset) {
    const int row = ui->tiers->currentRow();
    const int target = row + offset;
    if (row < 0 || target < 0 || target >= ui->tiers->rowCount()) return;
    auto tiers = tierRows();
    tiers.swapItemsAt(row, target);
    setTierRows(tiers);
    ui->tiers->setCurrentCell(target, 1);
    onTiersChanged(true);
}

void EditAutoSelector::onTiersChanged(bool revalidate) {
    updateTierButtons();
    if (fitTierTable() && !m_loading) resizeDialogToContent();
    if (revalidate && form) form->revalidate(ui->tiers);
    refreshPlanSummary();
}

void EditAutoSelector::updateTierButtons() const {
    const int row = ui->tiers->currentRow();
    ui->tier_remove->setEnabled(row >= 0);
    ui->tier_up->setEnabled(row > 0);
    ui->tier_down->setEnabled(row >= 0 && row < ui->tiers->rowCount() - 1);
}

// Up to five tiers, then it scrolls; rows take their editors' height so nothing clips.
bool EditAutoSelector::fitTierTable() const {
    auto *table = ui->tiers;
    // Theme fonts and borders apply at polish, which Qt otherwise defers to the event loop.
    table->ensurePolished();
    table->setVerticalScrollBarPolicy(table->rowCount() > 5 ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    auto *rows = table->verticalHeader();
    int rowHeight = rows->defaultSectionSize();
    if (table->rowCount() > 0) {
        for (int column = 0; column < table->columnCount(); column++) {
            if (const auto *cell = table->cellWidget(0, column)) rowHeight = std::max(rowHeight, cell->sizeHint().height());
        }
    }
    if (rowHeight != rows->defaultSectionSize()) rows->setDefaultSectionSize(rowHeight);
    const int height = table->horizontalHeader()->sizeHint().height() + qBound(1, table->rowCount(), 5) * rowHeight +
                       2 * table->frameWidth();
    if (table->minimumHeight() == height && table->maximumHeight() == height) return false;
    table->setFixedHeight(height);
    return true;
}

// Cell widgets join the focus chain at its end, so slot them in after the strategy box.
void EditAutoSelector::chainTierTabOrder() const {
    QWidget *previous = ui->strategy;
    for (int row = 0; row < ui->tiers->rowCount(); row++) {
        for (int column = 0; column < ui->tiers->columnCount(); column++) {
            if (auto *cell = ui->tiers->cellWidget(row, column)) {
                QWidget::setTabOrder(previous, cell);
                previous = cell;
            }
        }
    }
}

QString EditAutoSelector::tierIssue() const {
    const auto tiers = tierRows();
    if (auto message = Configs::Checks::AutoSelectorTierCount(static_cast<int>(tiers.size())); !message.isEmpty()) return message;
    for (int i = 0; i < tiers.size(); i++) {
        auto message = Configs::Checks::AutoSelectorTierMatch(tiers[i].gid, tiers[i].nameFilter);
        if (message.isEmpty()) message = Validation::Regex(tiers[i].nameFilter);
        if (!message.isEmpty()) return tr("Tier %1: %2").arg(QString::number(i + 1), message);
    }
    return {};
}

void EditAutoSelector::refreshPinnedRow() const
{
    const bool pinned = m_pinnedID >= 0;
    ui->pinned_l->setVisible(pinned);
    ui->pinned_name->setVisible(pinned);
    ui->pinned_clear->setVisible(pinned);
    if (!pinned) return;

    auto member = Configs::dataManager->profilesRepo->GetProfile(m_pinnedID);
    const auto name = member != nullptr && member->outbound != nullptr
                          ? member->outbound->DisplayName()
                          : QObject::tr("a profile that no longer exists");
    ui->pinned_name->setText(tr("%1 — chosen by you, so the selector stays on it while it works").arg(name));
}

void EditAutoSelector::updateBalanceEnabled() const {
    const bool on = ui->balance->isChecked();
    ui->balance_mode->setEnabled(on);
    ui->balance_mode_l->setEnabled(on);
    const bool rotating = on && ui->balance_mode->currentData().toString() == "rotate";
    ui->balance_interval->setEnabled(rotating);
    ui->balance_interval_l->setEnabled(rotating);

    if (!on) {
        ui->balance_hint->setText(tr("The best profile carries all traffic; the other ready profiles are "
                                     "kept verified so one can take over instantly."));
        return;
    }
    QString hint = rotating ? tr("New connections move to another good profile on each rotation; open ones are "
                                 "left alone and finish where they started. Traffic is counted exactly.")
                            : tr("Every new connection may take a different profile. Widest spread, but your "
                                 "exit IP changes mid-session and per-profile traffic becomes approximate.");
    if (prioritySelected()) {
        hint += QLatin1Char(' ') + tr("In priority order, traffic is only spread over the ready profiles of the tier in use.");
    }
    ui->balance_hint->setText(hint);
}

void EditAutoSelector::refreshPlanSummary() {
    if (m_loading) return;
    auto outbound = this->ent == nullptr ? nullptr : this->ent->AutoSelector();
    if (outbound == nullptr) return;

    // Throwaway copy: PlanAutoSelector normalises, which would clamp fields the form hasn't written yet.
    auto probe = std::make_shared<Configs::Profile>(new Configs::autoSelector(*outbound),
                                                    QStringLiteral("autoselector"));
    probe->id = this->ent->id;
    probe->gid = this->ent->gid;
    auto *preview = probe->AutoSelector();
    const auto checked = checkedGroupIDs();
    preview->gids = checked.isEmpty() ? checked : checked + m_missingGids;
    preview->nameFilter = ui->name_filter->text().trimmed();
    preview->countryFilter = ui->country_filter->text().trimmed();
    preview->excludeUnavailable = ui->exclude_unavailable->isChecked();
    preview->poolCap = ui->pool_cap->value();
    preview->buildLimit = ui->build_limit->value();
    preview->resultValidityMins = ui->result_validity->value();
    preview->strategy = ui->strategy->currentData().toString();
    preview->tiers = tierRows();

    const auto plan = Configs::PlanAutoSelector(probe);

    if (!plan.error.isEmpty()) {
        ui->plan_summary->setText(plan.error);
        return;
    }

    QStringList lines;
    lines << tr("%1 of %2 profiles in the checked groups can be used; %3 would run.")
                 .arg(plan.eligible)
                 .arg(plan.membersInGroups)
                 .arg(plan.build.size());
    if (preview->Prioritized() && !plan.tierBuilt.isEmpty()) {
        const int rest = static_cast<int>(plan.tierBuilt.size()) - 1;
        QStringList counts;
        QStringList empty;
        QStringList crowded;
        for (int i = 0; i <= rest; i++) {
            counts << tr("%1 → %2").arg(i < rest ? QString::number(i + 1) : tr("everything else"), QString::number(plan.tierBuilt[i]));
            if (i < rest && plan.tierBuilt[i] == 0) (plan.tierEligible.value(i) > 0 ? crowded : empty) << QString::number(i + 1);
        }
        lines << tr("By tier: %1.").arg(counts.join(tr(", ")));
        if (empty.size() == 1) {
            lines << tr("No usable profile belongs to tier %1.").arg(empty.first());
        } else if (!empty.isEmpty()) {
            lines << tr("No usable profile belongs to tiers %1.").arg(empty.join(tr(", ")));
        }
        if (crowded.size() == 1) {
            lines << tr("Too few profiles may run to cover every tier, so tier %1 runs none.").arg(crowded.first());
        } else if (!crowded.isEmpty()) {
            lines << tr("Too few profiles may run to cover every tier, so tiers %1 run none.").arg(crowded.join(tr(", ")));
        }
    }
    if (plan.missingGroups > 0) {
        lines << tr("%n tracked group(s) no longer exist and are ignored; saving removes them.", nullptr,
                    plan.missingGroups);
    }
    if (!plan.skipped.isEmpty()) {
        QStringList reasons;
        for (const auto &[skip, count] : plan.skipped) {
            reasons << tr("%1 %2").arg(count).arg(Configs::AutoSelectorSkipReason(skip));
        }
        lines << tr("Skipped: %1.").arg(reasons.join(tr(", ")));
    }
    if (plan.keptUnavailable > 0) {
        lines << tr("Every profile's last test failed, which usually means the network was down "
                    "rather than the servers, so all %1 are kept and will be re-checked.")
                     .arg(plan.keptUnavailable);
    }
    if (plan.truncated) {
        lines << (preview->Prioritized()
                      ? tr("More than %1 profiles match, so only the best-ranked ones of each tier are kept.")
                      : tr("More than %1 profiles match, so only the best-ranked ones are kept."))
                     .arg(plan.poolCapUsed);
    }
    if (plan.rankedByTest > 0) {
        lines << tr("%1 have a recent test result that will be reused.").arg(plan.rankedByTest);
    }
    if (plan.needsRanking) {
        lines << tr("The rest will be measured as the selector starts: it runs as soon as enough profiles work, and "
                    "switches to the full ranking once all are measured.");
    }
    ui->plan_summary->setText(lines.join(" "));
}
