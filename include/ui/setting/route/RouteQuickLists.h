#pragma once

#include <QWidget>

#include <memory>

#include "include/ui/setting/route/RouteEditorCommon.h"
#include "include/ui/utils/ValidationForm.h"

// The four quick lists (Direct, Proxy, Block, Proxy without exit): simple rules edited as typed entries.
namespace QuickLists {
    struct Entry {
        // domain, suffix, keyword, regex, ruleset, ip, processName or processPath.
        QString prefix;
        QString value;
        std::shared_ptr<Configs::RouteRule> rule;
        // The rule attribute holding the value, e.g. domain_suffix.
        QString attr;
        // RouteProfile::CheckSimpleRule's verdict; an empty message when the value is fine.
        Configs::RuleIssue issue;
    };

    // The entries of profile.Rules[first, first + count), in display order.
    QList<Entry> Entries(const Configs::RouteProfile& profile, int first, int count);

    // "prefix:value" for a line typed by its prefix or by its shape; empty, with *error set, when it is neither.
    QString Canonical(const QString& line, QString* error);

    // Into the row's rule of that kind, else into a new rule placed right after the row.
    bool Add(Configs::RouteProfile& profile, const RouteEditor::Row& row, const QString& line, QString* error);

    // Leaves the rule in place even when its row disappears.
    void Remove(const Entry& entry);

    // Gives each missing list a backing rule to fill.
    void EnsureLists(Configs::RouteProfile& profile);

    QString TypeLabel(const QString& prefix);

    int ErrorCount(const QList<Entry>& entries);

    // "14 entries · 1 error · suffix, ip, process", or "Empty, left out of the config".
    QString Summary(const QList<Entry>& entries);
} // namespace QuickLists

// A quick list row's editor: its entries with their errors, and a field that adds lines.
class QuickListEditor : public QWidget {
    Q_OBJECT

public:
    explicit QuickListEditor(std::shared_ptr<Configs::RouteProfile> profile, QWidget* parent = nullptr);
    ~QuickListEditor() override;

    // rows is the whole row list, for the note naming the list's other rows.
    void setRow(const RouteEditor::Row& row, const QList<RouteEditor::Row>& rows);
    bool hasPendingEntry() const;

    // Selects that entry; returns the widget to focus.
    QWidget* revealEntry(const QuickLists::Entry& entry);

signals:
    // Entries were added or removed; a new rule may now follow the row, so rows must be rebuilt.
    void changed();

private:
    struct Private;
    std::unique_ptr<Private> d;

    void reload(int current, int scroll);
    void submit();
    void removeEntry(int row);
    void editEntry(int row);
};
