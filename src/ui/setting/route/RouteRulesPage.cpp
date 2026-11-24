#include "include/ui/setting/route/RouteRulesPage.h"

#include "include/ui/setting/route/RouteEditorCommon.h"
#include "include/ui/setting/route/RouteEndpointPanel.h"
#include "include/ui/setting/route/RouteQuickLists.h"
#include "include/ui/setting/route/RouteRuleEditor.h"
#include "include/ui/setting/route/RouteRuleList.h"
#include "include/ui/utils/ThemedIcon.hpp"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyleOption>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace {
    QLabel* rulesPageLabel(const QString& text, QWidget* parent, bool wrap, qreal scale) {
        auto* label = new QLabel(text, parent);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(wrap);
        label->setProperty("colorRole", QStringLiteral("muted"));
        if (scale != 1.0) {
            QFont font = label->font();
            if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * scale);
            else if (font.pixelSize() > 0) font.setPixelSize(qMax(8, qRound(font.pixelSize() * scale)));
            label->setFont(font);
        }
        return label;
    }

    // Plain text, as names come from profiles. An empty accept asks Yes or No.
    bool rulesPageConfirm(QWidget* parent, const QString& title, const QString& text, const QString& accept) {
        QMessageBox box(QMessageBox::Question, title, text, QMessageBox::NoButton, parent);
        box.setTextFormat(Qt::PlainText);
        QPushButton* yes = accept.isEmpty() ? box.addButton(QMessageBox::Yes) : box.addButton(accept, QMessageBox::DestructiveRole);
        box.addButton(accept.isEmpty() ? QMessageBox::No : QMessageBox::Cancel);
        box.setDefaultButton(yes);
        box.exec();
        return box.clickedButton() == yes;
    }

    bool rulesPageRowEmpty(const Configs::RouteProfile& profile, const RouteEditor::Row& row) {
        for (int i = qMax(0, row.first); i < row.first + row.count && i < profile.Rules.size(); ++i) {
            if (!profile.Rules.at(i)->isEmpty()) return false;
        }
        return true;
    }

    // QPushButton sizes an icon-only button by its icon alone; this one is square and as tall as a text button.
    class RulesPageIconButton : public QPushButton {
    public:
        RulesPageIconButton(const QString& icon, const QString& name, QWidget* parent) : QPushButton(parent) {
            setIcon(ThemedIcon(icon));
            setToolTip(name);
            setAccessibleName(name);
            setAutoDefault(false);
        }

        [[nodiscard]] QSize sizeHint() const override {
            QStyleOptionButton option;
            initStyleOption(&option);
            const QSize contents(iconSize().width(), fontMetrics().size(Qt::TextShowMnemonic, QStringLiteral("X")).height());
            const int side = style()->sizeFromContents(QStyle::CT_PushButton, &option, contents, this).height();
            return {side, side};
        }

        [[nodiscard]] QSize minimumSizeHint() const override {
            return sizeHint();
        }
    };

    // Sized by the page it shows: a QStackedWidget's own hints take the largest page, hidden ones included.
    class RulesPageStack : public QStackedWidget {
    public:
        using QStackedWidget::QStackedWidget;

        [[nodiscard]] QSize sizeHint() const override {
            const QWidget* page = currentWidget();
            return page != nullptr ? page->sizeHint() : QStackedWidget::sizeHint();
        }

        [[nodiscard]] QSize minimumSizeHint() const override {
            const QWidget* page = currentWidget();
            return page != nullptr ? page->minimumSizeHint() : QStackedWidget::minimumSizeHint();
        }

        [[nodiscard]] bool hasHeightForWidth() const override {
            const QWidget* page = currentWidget();
            return page != nullptr && page->hasHeightForWidth();
        }

        [[nodiscard]] int heightForWidth(int width) const override {
            const QWidget* page = currentWidget();
            return page != nullptr ? page->heightForWidth(width) : -1;
        }
    };
} // namespace

RouteRulesPage::RouteRulesPage(std::shared_ptr<Configs::RouteProfile> routeProfile, QWidget* parent) : QWidget(parent), profile(std::move(routeProfile)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    layout->addWidget(splitter);

    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);
    auto* newButton = new QToolButton(left);
    newButton->setText(tr("New rule"));
    newButton->setPopupMode(QToolButton::MenuButtonPopup);
    auto* quickMenu = new QMenu(newButton);
    quickMenu->addSection(RouteRuleList::tr("Quick list"));
    for (const auto kind : RouteEditor::QuickListOrder())
        quickMenu->addAction(RouteEditor::ListName(kind), this, [this, kind] { openQuickList(kind); });
    newButton->setMenu(quickMenu);
    duplicateButton = new RulesPageIconButton(QStringLiteral("copy"), tr("Duplicate"), left);
    deleteButton = new RulesPageIconButton(QStringLiteral("trash"), tr("Delete"), left);
    filterEdit = new QLineEdit(left);
    filterEdit->setPlaceholderText(tr("Filter rules"));
    filterEdit->setToolTip(tr("Filter by name, domain or app"));
    filterEdit->setClearButtonEnabled(true);
    toolbar->addWidget(newButton);
    toolbar->addWidget(duplicateButton);
    toolbar->addWidget(deleteButton);
    toolbar->addWidget(filterEdit, 1);
    leftLayout->addLayout(toolbar);
    list = new RouteRuleList(profile, left);
    leftLayout->addWidget(list, 1);

    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    detailScroll = new QScrollArea(right);
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* detail = new QWidget(detailScroll);
    auto* detailLayout = new QVBoxLayout(detail);
    detailLayout->setContentsMargins(12, 4, 12, 12);
    stack = new RulesPageStack(detail);
    detailLayout->addWidget(stack);
    placeholder = new QWidget(stack);
    auto* placeholderLayout = new QVBoxLayout(placeholder);
    auto* placeholderText = rulesPageLabel(tr("Select a rule to edit it."), placeholder, true, 1.0);
    placeholderText->setAlignment(Qt::AlignCenter);
    placeholderLayout->addWidget(placeholderText);
    ruleEditor = new RouteRuleEditor(profile, stack);
    quickListEditor = new QuickListEditor(profile, stack);
    endpointPanel = new EndpointRulePanel(profile, stack);
    stack->addWidget(placeholder);
    stack->addWidget(ruleEditor);
    stack->addWidget(quickListEditor);
    stack->addWidget(endpointPanel);
    stack->setCurrentWidget(placeholder);
    detailScroll->setWidget(detail);
    detail->installEventFilter(this);
    rightLayout->addWidget(detailScroll, 1);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    // Weights until the handle is dragged; without them the list's size hint takes most of the width.
    splitter->setSizes({700, 1300});

    connect(newButton, &QToolButton::clicked, this, &RouteRulesPage::addRule);
    connect(duplicateButton, &QPushButton::clicked, this, [this] { duplicateRow(list->selectedRow()); });
    connect(deleteButton, &QPushButton::clicked, this, [this] { deleteRow(list->selectedRow()); });
    connect(filterEdit, &QLineEdit::textChanged, list, &RouteRuleList::setFilter);
    connect(list, &RouteRuleList::selectedRowChanged, this, &RouteRulesPage::onRowSelected);
    connect(list, &RouteRuleList::reorderRequested, this, &RouteRulesPage::moveRow);
    connect(list, &RouteRuleList::toggleRequested, this, [this](int row) { setRowDisabled(row, !rowDisabled(row)); });
    connect(list, &RouteRuleList::deleteRequested, this, [this] { deleteRow(list->selectedRow()); });
    connect(list, &RouteRuleList::menuRequested, this, &RouteRulesPage::showRowMenu);
    connect(ruleEditor, &RouteRuleEditor::changed, this, [this] { rowEdited(rowOf(ruleEditor->rule())); });
    connect(quickListEditor, &QuickListEditor::changed, this, &RouteRulesPage::scheduleQuickListRebuild);
    connect(endpointPanel, &EndpointRulePanel::openEndpointsRequested, this, &RouteRulesPage::openEndpointsRequested);

    onRowSelected(-1);
    fitDetailWidth();
}

RouteRulesPage::~RouteRulesPage() = default;

void RouteRulesPage::reload() {
    rebuild(selected, true);
}

void RouteRulesPage::setDefaultOutbound(int outboundID) {
    list->setDefaultOutbound(outboundID);
}

void RouteRulesPage::selectRule(const std::shared_ptr<Configs::RouteRule>& rule) {
    const int row = rowOf(rule);
    if (row < 0) return;
    if (!list->isRowShown(row)) filterEdit->clear();
    list->setSelectedRow(row);
}

QList<Validation::FormIssue> RouteRulesPage::issues() {
    QList<Validation::FormIssue> out;
    const QPointer<RouteRulesPage> self(this);
    const auto& rows = list->rows();
    for (int row = 0; row < rows.size(); ++row) {
        const RouteEditor::Row& current = rows.at(row);
        if (current.first < 0 || current.first >= profile->Rules.size()) continue;
        if (current.kind == RouteEditor::RowKind::Rule) {
            const auto rule = profile->Rules.at(current.first);
            const QString base = tr("Rule %1 \"%2\"").arg(QString::number(row + 1), rule->name);
            QList<Configs::RuleIssue> problems = RouteEditor::RuleProblems(*rule, profile->ignoreXrayRouting);
            // Values typed but rejected are not in the rule yet, and saving would drop them.
            if (rule == ruleEditor->rule()) {
                if (Configs::RuleIssue pending = ruleEditor->pendingIssue(); !pending.message.isEmpty()) problems.prepend(pending);
            }
            for (const auto& issue : std::as_const(problems)) {
                const bool then = issue.attr.isEmpty() || issue.attr == QLatin1String("action") || issue.attr == QLatin1String("outbound");
                Validation::FormIssue item;
                item.severity = issue.severity;
                item.label = base + QStringLiteral(" › ") + (then ? tr("Then") : RouteEditor::ConditionLabel(issue.attr));
                item.message = issue.value.isEmpty() ? issue.message : QStringLiteral("\"%1\": %2").arg(RouteEditor::ElideValue(issue.value), issue.message);
                item.reveal = [self, rule, attr = issue.attr, value = issue.value]() -> QWidget* {
                    if (self.isNull()) return nullptr;
                    self->selectRule(rule);
                    if (self->ruleEditor->rule() != rule) return self->list;
                    return self->ruleEditor->reveal(attr, value);
                };
                out << item;
            }
        } else if (current.kind == RouteEditor::RowKind::QuickList) {
            const auto entries = QuickLists::Entries(*profile, current.first, current.count);
            for (int i = 0; i < entries.size(); ++i) {
                const QuickLists::Entry& entry = entries.at(i);
                if (entry.issue.message.isEmpty()) continue;
                Validation::FormIssue item;
                // A turned-off list never blocks saving.
                item.severity = entry.rule != nullptr && entry.rule->disabled ? Validation::Severity::Warning : entry.issue.severity;
                item.label = tr("%1 list").arg(RouteEditor::ListName(current.list));
                item.message = tr("Entry %1 (\"%2\"): %3").arg(QString::number(i + 1), RouteEditor::ElideValue(entry.value), entry.issue.message);
                item.reveal = [self, entry]() -> QWidget* {
                    if (self.isNull()) return nullptr;
                    self->selectRule(entry.rule);
                    return self->quickListEditor->revealEntry(entry);
                };
                out << item;
            }
        }
    }
    return out;
}

std::shared_ptr<Configs::RouteRule> RouteRulesPage::firstRuleOf(int row) const {
    const auto& rows = list->rows();
    if (row < 0 || row >= rows.size()) return nullptr;
    const int first = rows.at(row).first;
    return first >= 0 && first < profile->Rules.size() ? profile->Rules.at(first) : nullptr;
}

int RouteRulesPage::rowOf(const std::shared_ptr<Configs::RouteRule>& rule) const {
    if (rule == nullptr) return -1;
    return RouteEditor::RowOfRule(list->rows(), static_cast<int>(profile->Rules.indexOf(rule)));
}

void RouteRulesPage::rebuild(const std::shared_ptr<Configs::RouteRule>& keep, bool recompute, bool keepDraft) {
    list->rebuild(recompute);
    // The rebuilt list selects nothing and says nothing, so the page shows the row itself.
    const int row = rowOf(keep);
    if (row >= 0) list->setSelectedRow(row);
    // Editing moves an entry into the input. Keep the editor open when that was its last entry.
    else if (!keepDraft) onRowSelected(-1);
}

void RouteRulesPage::onRowSelected(int row) {
    const auto& rows = list->rows();
    const auto rule = firstRuleOf(row);
    const bool moved = rule != selected;
    selected = rule;
    if (rule == nullptr) {
        ruleEditor->setRule(nullptr);
        showPage(placeholder);
    } else {
        const RouteEditor::Row current = rows.at(row);
        switch (current.kind) {
            case RouteEditor::RowKind::Rule:
                ruleEditor->setRule(rule);
                showPage(ruleEditor);
                break;
            case RouteEditor::RowKind::QuickList:
                ruleEditor->setRule(nullptr);
                quickListEditor->setRow(current, rows);
                showPage(quickListEditor);
                break;
            case RouteEditor::RowKind::Endpoint:
                ruleEditor->setRule(nullptr);
                endpointPanel->setRule(rule);
                showPage(endpointPanel);
                break;
        }
    }
    if (moved) detailScroll->verticalScrollBar()->setValue(0);
    updateActions();
}

void RouteRulesPage::showPage(QWidget* page) {
    if (stack->currentWidget() == page) return;
    stack->setCurrentWidget(page);
    stack->updateGeometry();
    fitDetailWidth();
}

// The editor pane never scrolls sideways, so the splitter must leave it the shown page's minimum width.
void RouteRulesPage::fitDetailWidth() {
    const QWidget* detail = detailScroll->widget();
    const int need = detail->minimumSizeHint().width() + detailScroll->verticalScrollBar()->sizeHint().width() + 2 * detailScroll->frameWidth();
    if (detailScroll->minimumWidth() != need) detailScroll->setMinimumWidth(need);
}

bool RouteRulesPage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == detailScroll->widget() && event->type() == QEvent::LayoutRequest) fitDetailWidth();
    return QWidget::eventFilter(watched, event);
}

void RouteRulesPage::updateActions() {
    const auto& rows = list->rows();
    const int row = list->selectedRow();
    const bool valid = row >= 0 && row < rows.size();
    const RouteEditor::RowKind kind = valid ? rows.at(row).kind : RouteEditor::RowKind::Rule;
    duplicateButton->setEnabled(valid && kind == RouteEditor::RowKind::Rule);
    deleteButton->setEnabled(valid && !(kind == RouteEditor::RowKind::QuickList && rulesPageRowEmpty(*profile, rows.at(row))));
    QString tip = tr("Delete");
    if (valid && kind == RouteEditor::RowKind::QuickList) tip = tr("Remove all entries");
    else if (valid && kind == RouteEditor::RowKind::Endpoint) tip = tr("Remove endpoint");
    deleteButton->setToolTip(tip);
    deleteButton->setAccessibleName(tip);
}

void RouteRulesPage::rowEdited(int row) {
    if (row >= 0) list->refreshRow(row);
    emit changed();
}

void RouteRulesPage::scheduleQuickListRebuild() {
    if (quickListRebuildQueued) return;
    quickListRebuildQueued = true;
    // Later: the editor emits from its own handlers, and a rule it added after its row reshapes the rows.
    QTimer::singleShot(0, this, [this, edited = selected] {
        quickListRebuildQueued = false;
        const bool draft = edited && selected == edited && stack->currentWidget() == quickListEditor && quickListEditor->hasPendingEntry();
        rebuild(selected, false, draft);
        if (const int row = rowOf(edited); row >= 0) list->refreshRow(row);
        updateActions();
        emit changed();
    });
}

void RouteRulesPage::addRule() {
    QSet<QString> taken;
    for (const auto& rule : std::as_const(profile->Rules)) taken.insert(rule->name);
    int number = 1;
    while (taken.contains(tr("Rule %1").arg(number))) ++number;
    auto rule = std::make_shared<Configs::RouteRule>();
    rule->name = tr("Rule %1").arg(number);

    const auto& rows = list->rows();
    const int row = list->selectedRow();
    int at = static_cast<int>(profile->Rules.size());
    if (row >= 0 && row < rows.size()) at = qBound(0, rows.at(row).first + rows.at(row).count, at);
    profile->Rules.insert(at, rule);
    // The new row must show, and a filter would hide it.
    if (!filterEdit->text().isEmpty()) filterEdit->clear();
    rebuild(rule, false);
    ruleEditor->focusName();
    emit changed();
}

void RouteRulesPage::openQuickList(Configs::simpleAction kind) {
    filterEdit->clear();
    const auto& shown = list->rows();
    for (int i = 0; i < shown.size(); ++i) {
        if (shown[i].kind == RouteEditor::RowKind::QuickList && shown[i].list == kind) {
            list->setSelectedRow(i);
            return;
        }
    }
    for (const auto& row : RouteEditor::BuildRows(*profile, true)) {
        if (row.kind != RouteEditor::RowKind::QuickList || row.list != kind) continue;
        list->setSelectedRow(-1);
        selected = profile->Rules.at(row.first);
        quickListEditor->setRow(row, shown);
        showPage(quickListEditor);
        updateActions();
        return;
    }
}

void RouteRulesPage::duplicateRow(int row) {
    const auto& rows = list->rows();
    if (row < 0 || row >= rows.size() || rows.at(row).kind != RouteEditor::RowKind::Rule) return;
    const int at = rows.at(row).first;
    const auto source = firstRuleOf(row);
    if (source == nullptr) return;
    QSet<QString> taken;
    for (const auto& rule : std::as_const(profile->Rules)) taken.insert(rule->name);
    const QString base = tr("%1 copy").arg(source->name);
    QString name = base;
    for (int n = 2; taken.contains(name); ++n) name = QStringLiteral("%1 (%2)").arg(base, QString::number(n));
    auto copy = std::make_shared<Configs::RouteRule>(*source);
    copy->name = name;
    copy->remote = false;
    profile->Rules.insert(at + 1, copy);
    rebuild(copy, false);
    emit changed();
}

void RouteRulesPage::deleteRow(int row) {
    const auto& rows = list->rows();
    if (row < 0 || row >= rows.size()) return;
    switch (rows.at(row).kind) {
        case RouteEditor::RowKind::Rule: deleteRule(row); break;
        case RouteEditor::RowKind::QuickList: clearQuickList(row); break;
        case RouteEditor::RowKind::Endpoint: removeEndpoint(row); break;
    }
}

void RouteRulesPage::deleteRule(int row) {
    const auto rule = firstRuleOf(row);
    if (rule == nullptr) return;
    if (!rulesPageConfirm(this, tr("Delete rule"), tr("Delete the rule \"%1\"?").arg(rule->name), tr("Delete"))) return;
    // The question ran its own event loop: look the rule up again.
    const int at = rowOf(rule);
    if (at < 0) return;
    const auto next = neighbourOf(at);
    profile->Rules.removeAll(rule);
    rebuild(next, false);
    emit changed();
}

void RouteRulesPage::clearQuickList(int row) {
    const auto& rows = list->rows();
    if (row < 0 || row >= rows.size()) return;
    const RouteEditor::Row current = rows.at(row);
    const auto first = firstRuleOf(row);
    const int count = static_cast<int>(QuickLists::Entries(*profile, current.first, current.count).size());
    if (first == nullptr || count == 0) return;
    if (!rulesPageConfirm(this, tr("Remove all entries"), tr("Remove all %n entries from the %1 list?", nullptr, count).arg(RouteEditor::ListName(current.list)),
                          tr("Remove all entries"))) {
        return;
    }
    const int at = rowOf(first);
    if (at < 0) return;
    const RouteEditor::Row now = list->rows().at(at);
    // The rules stay, empty: saving drops them.
    for (int i = qMax(0, now.first); i < now.first + now.count && i < profile->Rules.size(); ++i) {
        const auto& rule = profile->Rules.at(i);
        for (const QString& attr : RouteEditor::SetConditions(*rule)) rule->set_field_value(attr, {});
    }
    rebuild(selected == first ? neighbourOf(at) : selected, false);
    emit changed();
}

void RouteRulesPage::removeEndpoint(int row) {
    const auto rule = firstRuleOf(row);
    if (rule == nullptr) return;
    const int endpointId = rule->outboundID;
    // The old dialog's questions, kept under its context.
    const QString title = QCoreApplication::translate("RouteItem", "Endpoint rule");
    if (const int owner = RouteEditor::InnerHopOwner(*profile, endpointId); owner >= 0) {
        const QString text = QCoreApplication::translate("RouteItem",
                                                         "This rule belongs to \"%1\", an inner hop of the endpoint \"%2\".\n\n"
                                                         "Stop routing to that endpoint's inner hops?")
                                 .arg(RouteEditor::ProfileName(endpointId), RouteEditor::ProfileName(owner));
        if (rulesPageConfirm(this, title, text, {})) emit innerHopsOffRequested(owner);
        return;
    }
    const QString text = QCoreApplication::translate("RouteItem",
                                                     "This rule belongs to the endpoint \"%1\" and cannot be deleted on its own.\n\n"
                                                     "Remove that endpoint from this routing profile as well?")
                             .arg(RouteEditor::ProfileName(endpointId));
    if (rulesPageConfirm(this, title, text, {})) emit endpointRemovalRequested(endpointId);
}

void RouteRulesPage::moveRow(int from, int to) {
    const auto& rows = list->rows();
    const int count = static_cast<int>(rows.size());
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) return;
    RouteEditor::MoveRow(*profile, rows, from, to);
    rebuild(selected, true);
    emit changed();
}

bool RouteRulesPage::rowDisabled(int row) const {
    const auto& rows = list->rows();
    if (row < 0 || row >= rows.size()) return false;
    const RouteEditor::Row& current = rows.at(row);
    for (int i = qMax(0, current.first); i < current.first + current.count && i < profile->Rules.size(); ++i) {
        if (!profile->Rules.at(i)->disabled) return false;
    }
    return true;
}

void RouteRulesPage::setRowDisabled(int row, bool disabled) {
    const auto& rows = list->rows();
    if (row < 0 || row >= rows.size() || rows.at(row).kind == RouteEditor::RowKind::Endpoint) return;
    const RouteEditor::Row current = rows.at(row);
    for (int i = qMax(0, current.first); i < current.first + current.count && i < profile->Rules.size(); ++i) profile->Rules.at(i)->disabled = disabled;
    list->refreshRow(row);
    if (firstRuleOf(row) == selected) {
        if (current.kind == RouteEditor::RowKind::Rule) ruleEditor->refresh();
        else quickListEditor->setRow(current, rows);
    }
    emit changed();
}

void RouteRulesPage::showRowMenu(int row, const QPoint& globalPos) {
    const auto& rows = list->rows();
    if (row < 0 || row >= rows.size()) return;
    const RouteEditor::Row current = rows.at(row);
    const auto first = firstRuleOf(row);
    if (first == nullptr) return;

    QMenu menu(this);
    QAction* duplicate = nullptr;
    if (current.kind == RouteEditor::RowKind::Rule) {
        duplicate = menu.addAction(tr("Duplicate"));
        menu.addSeparator();
    }
    QAction* top = menu.addAction(tr("Move to top"));
    top->setEnabled(row > 0);
    QAction* bottom = menu.addAction(tr("Move to bottom"));
    bottom->setEnabled(row < rows.size() - 1);
    QAction* toggle = nullptr;
    if (current.kind != RouteEditor::RowKind::Endpoint) {
        menu.addSeparator();
        toggle = menu.addAction(rowDisabled(row) ? tr("Enable") : tr("Disable"));
    }
    menu.addSeparator();
    QAction* remove = nullptr;
    switch (current.kind) {
        case RouteEditor::RowKind::Rule: remove = menu.addAction(tr("Delete")); break;
        case RouteEditor::RowKind::QuickList:
            remove = menu.addAction(tr("Remove all entries"));
            remove->setEnabled(!rulesPageRowEmpty(*profile, current));
            break;
        case RouteEditor::RowKind::Endpoint: remove = menu.addAction(tr("Remove endpoint")); break;
    }

    const QAction* picked = menu.exec(globalPos);
    if (picked == nullptr) return;
    // The menu ran its own event loop: look the row up again.
    const int at = rowOf(first);
    if (at < 0) return;
    if (picked == duplicate) duplicateRow(at);
    else if (picked == top) moveRow(at, 0);
    else if (picked == bottom) moveRow(at, static_cast<int>(list->rows().size()) - 1);
    else if (picked == toggle) setRowDisabled(at, !rowDisabled(at));
    else if (picked == remove) deleteRow(at);
}

std::shared_ptr<Configs::RouteRule> RouteRulesPage::neighbourOf(int row) const {
    const int count = static_cast<int>(list->rows().size());
    for (int i = row + 1; i < count; ++i) {
        if (list->isRowShown(i)) return firstRuleOf(i);
    }
    for (int i = row - 1; i >= 0; --i) {
        if (list->isRowShown(i)) return firstRuleOf(i);
    }
    return nullptr;
}
