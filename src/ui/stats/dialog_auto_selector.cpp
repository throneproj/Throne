#include "include/ui/stats/dialog_auto_selector.h"

#include "include/global/GuiUtils.hpp"

#include <QCheckBox>
#include <QDateTime>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
    enum Column {
        ColRank = 0,
        ColName,
        ColState,
        ColLatency,
        ColJitter,
        ColChecks,
        ColDials,
        ColLastOK,
        ColNote,
        ColCount
    };

    QString agoText(qint64 ms)
    {
        if (ms <= 0) return QObject::tr("never");
        const auto secs = (QDateTime::currentMSecsSinceEpoch() - ms) / 1000;
        if (secs < 5) return QObject::tr("just now");
        if (secs < 60) return QObject::tr("%1s ago").arg(secs);
        if (secs < 3600) return QObject::tr("%1m ago").arg(secs / 60);
        return QObject::tr("%1h ago").arg(secs / 3600);
    }

    QString stateText(const Stats::AutoSelectorMemberView &member)
    {
        if (member.state == "ok") return QObject::tr("Working");
        if (member.state == "degraded") return QObject::tr("Unstable");
        if (member.state == "untested") return QObject::tr("Not checked");
        if (member.state == "dead") return QObject::tr("Failing");
        if (member.state == "cooldown") return QObject::tr("Paused");
        return member.state;
    }

    // Sorting the status column by its label would be alphabetical noise; order
    // it best-to-worst instead.
    int stateOrder(const Stats::AutoSelectorMemberView &member)
    {
        if (member.state == "ok") return 0;
        if (member.state == "degraded") return 1;
        if (member.state == "untested") return 2;
        if (member.state == "cooldown") return 3;
        return 4; // dead
    }

    // A sort key that may be absent. Missing values are parked at the end in
    // both directions, so an unmeasured profile never floats to the top of a
    // "fastest first" sort.
    struct Key {
        bool known = false;
        double value = 0;
    };

    Key latencyKey(const Stats::AutoSelectorMemberView &m) { return {m.samples > 0 && m.averageMs > 0, static_cast<double>(m.averageMs)}; }
    Key jitterKey(const Stats::AutoSelectorMemberView &m) { return {m.samples > 0, static_cast<double>(m.deviationMs)}; }

    Key checksKey(const Stats::AutoSelectorMemberView &m)
    {
        if (m.samples <= 0) return {};
        return {true, static_cast<double>(m.samples - m.failures) / m.samples};
    }

    Key dialsKey(const Stats::AutoSelectorMemberView &m)
    {
        if (m.dialTotal <= 0) return {};
        return {true, static_cast<double>(m.dialTotal - m.dialFail) / m.dialTotal};
    }

    Key lastOKKey(const Stats::AutoSelectorMemberView &m) { return {m.lastOKms > 0, static_cast<double>(m.lastOKms)}; }

    // Explains the row in plain words, so a demoted server never looks arbitrary.
    QString noteText(const Stats::AutoSelectorMemberView &member)
    {
        if (member.pinned) {
            if (member.selected) return QObject::tr("your choice, carrying traffic now");
            // The pin is a preference, not an override: the selector had to move
            // off it, and will come back once it is working again.
            return QObject::tr("your choice, but not usable right now");
        }
        if (member.selected) return QObject::tr("carrying traffic now");
        if (member.state == "cooldown") {
            const auto secs = (member.cooldownUntilMs - QDateTime::currentMSecsSinceEpoch()) / 1000;
            if (secs > 0) return QObject::tr("failed to connect, retrying in %1s").arg(secs);
            return QObject::tr("failed to connect");
        }
        if (member.state == "dead") {
            return member.lastError.isEmpty() ? QObject::tr("every check failed") : member.lastError;
        }
        if (member.qualified) return QObject::tr("ready to take over");
        if (member.state == "untested") {
            return member.active ? QObject::tr("check in progress") : QObject::tr("queued for checking");
        }
        if (member.failures > 0) return QObject::tr("%1 of %2 checks failed").arg(member.failures).arg(member.samples);
        return {};
    }
} // namespace

DialogAutoSelector::DialogAutoSelector(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Auto Selector Stats"));

    auto *layout = new QVBoxLayout(this);

    m_headline = new QLabel(this);
    auto headlineFont = m_headline->font();
    headlineFont.setBold(true);
    m_headline->setFont(headlineFont);
    m_headline->setWordWrap(true);
    layout->addWidget(m_headline);

    m_detail = new QLabel(this);
    m_detail->setWordWrap(true);
    layout->addWidget(m_detail);

    auto *controls = new QHBoxLayout();
    m_onlyProblems = new QCheckBox(tr("Only show profiles with problems"), this);
    connect(m_onlyProblems, &QCheckBox::toggled, this, [this] { refresh(); });
    controls->addWidget(m_onlyProblems);
    controls->addStretch();

    m_pin = new QPushButton(tr("Use this profile"), this);
    m_pin->setToolTip(tr("Keep the selected profile in use instead of letting the ranking choose. "
                         "Useful when several profiles measure much the same and you prefer one of "
                         "them.\n\nIt stays a preference, not a lock: if that profile stops working "
                         "the selector still moves on, and comes back to your choice once it "
                         "recovers."));
    connect(m_pin, &QPushButton::clicked, this, [this] { applySelection(highlightedTag()); });
    controls->addWidget(m_pin);

    m_release = new QPushButton(tr("Back to automatic"), this);
    m_release->setToolTip(tr("Stop preferring a particular profile and let the ranking decide again."));
    connect(m_release, &QPushButton::clicked, this, [this] { applySelection({}); });
    controls->addWidget(m_release);

    m_recheck = new QPushButton(tr("Check all now"), this);
    m_recheck->setToolTip(tr("Re-measure every running profile immediately instead of waiting for the "
                             "next scheduled check."));
    connect(m_recheck, &QPushButton::clicked, this, [this] {
        Stats::autoSelectorMonitor->RequestRecheck();
        m_footer->setText(tr("Re-checking every profile..."));
    });
    controls->addWidget(m_recheck);
    layout->addLayout(controls);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({"#", tr("Profile"), tr("Status"), tr("Latency"), tr("Jitter"),
                                        tr("Checks"), tr("Connects"), tr("Last OK"), tr("Notes")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);

    auto *header = m_table->horizontalHeader();
    header->setSectionsClickable(true);
    header->setSortIndicatorShown(true);
    header->setSortIndicator(m_sortColumn, m_sortOrder);
    connect(header, &QHeaderView::sectionClicked, this, &DialogAutoSelector::onHeaderClicked);
    // Interactive rather than Stretch: the columns are auto-sized once and are
    // then the user's to adjust, which Stretch would fight on every poll.
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        m_pin->setEnabled(!highlightedTag().isEmpty() && highlightedTag() != m_pinnedTag);
    });
    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *) {
        applySelection(highlightedTag());
    });
    layout->addWidget(m_table, 1);

    m_footer = new QLabel(this);
    m_footer->setWordWrap(true);
    layout->addWidget(m_footer);

    refresh();
    fitToColumns();
}

// A QTableWidget's size hint is a fixed default that has nothing to do with how
// many columns it holds, so sizing to the hint alone leaves all nine clamped to
// a few characters each. Take what the columns actually need as the floor, then
// bound the result to the desktop.
void DialogAutoSelector::fitToColumns()
{
    int needed = 2 * m_table->frameWidth() + m_table->verticalScrollBar()->sizeHint().width() + 24;
    for (int column = 0; column < ColCount; column++) needed += m_table->columnWidth(column);

    const auto available = QGuiApplication::primaryScreen()->availableGeometry().size();
    const QSize target(std::max({needed, sizeHint().width(), 900}), std::max(sizeHint().height(), 620));
    resize(target.boundedTo(available));
}

QString DialogAutoSelector::highlightedTag() const
{
    const auto rows = m_table->selectionModel() != nullptr ? m_table->selectionModel()->selectedRows()
                                                           : QModelIndexList{};
    if (rows.isEmpty()) return {};
    // The tag rides on the name cell rather than being looked up by row: sorting
    // and filtering both reorder the table under the selection.
    const auto *item = m_table->item(rows.first().row(), ColName);
    return item == nullptr ? QString() : item->data(Qt::UserRole).toString();
}

void DialogAutoSelector::applySelection(const QString &tag)
{
    if (tag.isEmpty() && m_pinnedTag.isEmpty()) return;
    const auto error = Stats::autoSelectorMonitor->RequestSelect(tag);
    if (!error.isEmpty()) {
        m_footer->setText(tr("Could not change the profile: %1").arg(error));
        return;
    }
    // The core applies this immediately; the table only catches up on the next
    // poll, so say so rather than looking like nothing happened.
    m_pinnedTag = tag;
    m_footer->setText(tag.isEmpty() ? tr("Back to automatic — the selector will choose again.")
                                    : tr("Now using your chosen profile."));
    m_pin->setEnabled(false);
    m_release->setEnabled(!tag.isEmpty());
}

void DialogAutoSelector::refresh()
{
    const auto view = Stats::autoSelectorMonitor->Snapshot();
    if (!view.valid) {
        m_headline->setText(tr("No auto selector is running."));
        m_detail->clear();
        m_footer->clear();
        m_table->setRowCount(0);
        m_recheck->setEnabled(false);
        m_pin->setEnabled(false);
        m_release->setEnabled(false);
        return;
    }
    m_recheck->setEnabled(true);
    m_pinnedTag = view.pinnedTag;
    m_pin->setEnabled(!highlightedTag().isEmpty() && highlightedTag() != m_pinnedTag);
    m_release->setEnabled(!m_pinnedTag.isEmpty());
    m_headline->setText(view.summary());
    m_detail->setText(view.detail());

    QStringList footer;
    if (view.suspended) {
        footer << tr("Checks are paused because this machine has no network connection. "
                     "No profile is being blamed for it, and the ranking is frozen until the "
                     "connection returns.");
    } else {
        if (view.roundsCompleted > 0) footer << tr("Last check round %1.").arg(agoText(view.lastRoundMs));
        if (view.nextRoundMs > 0) {
            const auto secs = (view.nextRoundMs - QDateTime::currentMSecsSinceEpoch()) / 1000;
            if (secs > 0) footer << tr("Next in %1s.").arg(secs);
        }
    }
    if (!view.pinnedTag.isEmpty()) {
        const auto pinnedName = view.pinnedName.isEmpty() ? view.pinnedTag : view.pinnedName;
        footer << (view.pinnedTag == view.selectedTag
                       ? tr("Using %1 because you chose it.").arg(pinnedName)
                       : tr("You chose %1, but it is not working right now, so the selector picked "
                            "another. It will go back to yours once it recovers.")
                             .arg(pinnedName));
    }
    if (!view.lastSwitchReason.isEmpty() && view.lastSwitchMs > 0) {
        footer << tr("Last switch %1 (%2).").arg(agoText(view.lastSwitchMs), view.lastSwitchReason);
    }
    if (view.exhaustedSinceMs > 0) {
        footer << tr("Nothing is working right now — if this holds, the selector will rebuild from the "
                     "next best profiles.");
    }
    m_footer->setText(footer.join(" "));

    buildRows(view);
}

void DialogAutoSelector::sortRows(QList<Stats::AutoSelectorMemberView> &rows) const
{
    const bool ascending = m_sortOrder == Qt::AscendingOrder;
    const int column = m_sortColumn;

    const auto compareKeys = [ascending](const Key &left, const Key &right, bool &decided) {
        decided = true;
        if (left.known != right.known) return left.known; // unknowns always last
        if (!left.known) { decided = false; return false; }
        if (left.value == right.value) { decided = false; return false; }
        return ascending ? left.value < right.value : left.value > right.value;
    };

    std::stable_sort(rows.begin(), rows.end(),
                     [&](const Stats::AutoSelectorMemberView &a, const Stats::AutoSelectorMemberView &b) {
        bool decided = false;
        bool result = false;
        switch (column) {
            case ColName: {
                const int cmp = QString::compare(a.name.isEmpty() ? a.tag : a.name,
                                                 b.name.isEmpty() ? b.tag : b.name, Qt::CaseInsensitive);
                if (cmp != 0) return ascending ? cmp < 0 : cmp > 0;
                break;
            }
            case ColState:
                if (stateOrder(a) != stateOrder(b))
                    return ascending ? stateOrder(a) < stateOrder(b) : stateOrder(a) > stateOrder(b);
                break;
            case ColLatency: result = compareKeys(latencyKey(a), latencyKey(b), decided); break;
            case ColJitter: result = compareKeys(jitterKey(a), jitterKey(b), decided); break;
            case ColChecks: result = compareKeys(checksKey(a), checksKey(b), decided); break;
            case ColDials: result = compareKeys(dialsKey(a), dialsKey(b), decided); break;
            case ColLastOK: result = compareKeys(lastOKKey(a), lastOKKey(b), decided); break;
            case ColNote: {
                const int cmp = QString::compare(noteText(a), noteText(b), Qt::CaseInsensitive);
                if (cmp != 0) return ascending ? cmp < 0 : cmp > 0;
                break;
            }
            default: // ColRank
                if (a.rank != b.rank) return ascending ? a.rank < b.rank : a.rank > b.rank;
                break;
        }
        if (decided) return result;
        // Rank is the tiebreak everywhere, so equal rows keep the order the core
        // ranked them in rather than shuffling between polls.
        return a.rank < b.rank;
    });
}

void DialogAutoSelector::onHeaderClicked(int column)
{
    if (column == m_sortColumn) {
        m_sortOrder = m_sortOrder == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        m_sortColumn = column;
        // Latency-like columns read best smallest-first; "last OK" and the
        // success ratios are most useful worst-first.
        m_sortOrder = (column == ColLastOK || column == ColChecks || column == ColDials)
                          ? Qt::DescendingOrder
                          : Qt::AscendingOrder;
    }
    m_table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
    refresh();
}

void DialogAutoSelector::buildRows(const Stats::AutoSelectorView &view)
{
    const bool filtered = m_onlyProblems->isChecked();
    QList<Stats::AutoSelectorMemberView> rows;
    for (const auto &member : view.members) {
        if (filtered && !member.hasProblem()) continue;
        rows << member;
    }
    sortRows(rows);

    // Rebuilding cells on every 2s poll would fight the user's scroll position,
    // so only the row count changes trigger a full reset.
    if (m_table->rowCount() != rows.size()) m_table->setRowCount(static_cast<int>(rows.size()));

    const auto setCell = [this](int row, int column, const QString &text, const QString &tip = {}) {
        auto *item = m_table->item(row, column);
        if (item == nullptr) {
            item = new QTableWidgetItem();
            m_table->setItem(row, column, item);
        }
        if (item->text() != text) item->setText(text);
        if (!tip.isEmpty()) item->setToolTip(tip);
    };

    for (int row = 0; row < rows.size(); row++) {
        const auto &member = rows[row];
        setCell(row, ColRank, QString::number(member.rank));
        setCell(row, ColName, member.name.isEmpty() ? member.tag : member.name, member.tag);
        setCell(row, ColState, stateText(member));
        setCell(row, ColLatency, member.averageMs > 0 ? tr("%1 ms").arg(member.averageMs) : QStringLiteral("-"),
                member.samples > 0 ? tr("Best %1 ms, worst %2 ms").arg(member.minMs).arg(member.maxMs) : QString());
        setCell(row, ColJitter, member.samples > 0 ? tr("%1 ms").arg(member.deviationMs) : QStringLiteral("-"),
                tr("How much the latency moves around. The selector prefers steady profiles over "
                   "fast but erratic ones."));
        setCell(row, ColChecks,
                member.samples > 0 ? tr("%1 / %2 ok").arg(member.samples - member.failures).arg(member.samples)
                                   : QStringLiteral("-"));
        setCell(row, ColDials,
                member.dialTotal > 0 ? tr("%1 / %2 ok").arg(member.dialTotal - member.dialFail).arg(member.dialTotal)
                                     : QStringLiteral("-"),
                tr("Real connection attempts made by apps through this profile."));
        setCell(row, ColLastOK, agoText(member.lastOKms));
        setCell(row, ColNote, noteText(member));

        auto *nameItem = m_table->item(row, ColName);
        nameItem->setData(Qt::UserRole, member.tag);
        auto font = nameItem->font();
        if (font.bold() != member.selected || font.italic() != member.pinned) {
            font.setBold(member.selected);
            font.setItalic(member.pinned);
            nameItem->setFont(font);
        }
    }
    // Only once: re-fitting on every 2s poll would yank column widths around
    // under the user's cursor.
    if (!m_columnsSized && !rows.isEmpty()) {
        m_columnsSized = true;
        auto *header = m_table->horizontalHeader();
        // Measure with the stretch off, or the last column reports the table's
        // current width instead of what its content needs — and that width is
        // exactly what is being computed here.
        header->setStretchLastSection(false);
        m_table->resizeColumnsToContents();
        // Give the two free-text columns room to breathe beyond their content.
        m_table->setColumnWidth(ColName, std::max(m_table->columnWidth(ColName), 220));
        m_table->setColumnWidth(ColNote, std::max(m_table->columnWidth(ColNote), 260));
        // The first poll can land after the dialog is already up, so re-fit now
        // that the real column widths are known.
        fitToColumns();
        header->setStretchLastSection(true);
    }
}
