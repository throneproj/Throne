#pragma once

#include <functional>

#include <QDialog>
#include <QPointer>
#include <QSet>
#include <QStandardItemModel>

class QAction;
class QLabel;
class QLineEdit;
class QListView;
class QMenu;

// Ctrl+K search over menu commands and settings: commands are read from the menus, the caller supplies the settings.
class CommandPalette : public QDialog {
    Q_OBJECT

public:
    struct Entry {
        enum Kind { Command, Setting };
        Kind kind = Command;
        QString label;
        // Where the entry lives, shown after the label: "Groups › Clean up", "Basic Settings › Logging".
        QString path;
        // Heading used while browsing without a query.
        QString section;
        // Matched but never shown; object names give an English fallback in a translated UI.
        QString keywords;
        // A command's own action, or the action that opens a setting's dialog; its state is read each time the list is filled.
        QPointer<QAction> action;
        // Menu actions of the submenus a command sits in: hiding or disabling one hides or disables the command.
        QList<QPointer<QAction>> submenus;
        std::function<void()> run;
        // The Blocker's answer for action, refreshed whenever the palette comes to the front.
        QString blocked;
    };

    // Why an entry's action cannot run right now, or empty when it can.
    using Blocker = std::function<QString(const QAction *)>;
    // Builds the Blocker for the current state; asked again whenever the palette refreshes and before an entry runs.
    using BlockerSource = std::function<Blocker()>;

    // Commands come from menus, each listed under its heading; skip is left out.
    CommandPalette(QWidget *parent, QList<QPair<QString, QMenu *>> menus, const QAction *skip);

    void setSettings(QList<Entry> settings);

    // Refreshes the menus' state, collects the commands again if a menu changed, then shows an empty query.
    void present(BlockerSource source);

    // Drops "&" mnemonics, keeping "&&" as a literal ampersand, and a trailing colon or ellipsis.
    static QString plainText(QString text);

    // Search words from an object name ("actionUrl_Test_Group" -> "Url Test Group"); empty for Designer's defaults.
    static QString keywordsFor(const QString &objectName);

    // Text fields bind Ctrl+K to "delete to end of line" on Linux and macOS; stop them swallowing the action's shortcut in window.
    static void keepShortcutFromTextFields(QAction *action, QWidget *window);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    // The selected entry, found again after the list is rebuilt.
    struct Selection {
        Entry::Kind kind = Entry::Command;
        QPointer<QAction> action;
        QString label;
        QString path;
    };

    Blocker currentBlocker() const;
    void refreshCommands();
    void refreshBlocked();
    void refreshState();
    void scheduleRefresh();
    void collect(QMenu *menu, const QString &section, const QString &path, const QList<QPointer<QAction>> &submenus,
                 const QSet<QMenu *> &refreshed, QSet<const QAction *> &seen, QList<Entry> &out);
    void markCommandsDirty();
    // With keep set, the list stays where it was scrolled and keep stays selected if it is still listed.
    void refilter(const Selection &keep = {});
    void addHeader(const QString &title);
    void addEntry(int entryIndex, int matchStart, int matchLength);
    Selection currentSelection() const;
    void select(const Selection &keep);
    void moveSelection(int delta);
    void activate(const QModelIndex &index);

    QLineEdit *edit;
    QListView *list;
    QLabel *hint;
    QStandardItemModel model;

    QList<QPair<QString, QMenu *>> roots;
    const QAction *skipAction;
    BlockerSource blockerSource;
    QList<QPointer<QMenu>> collectedMenus;
    bool commandsDirty = true;
    bool collecting = false;
    // Set when the palette loses the focus: whatever happened elsewhere is read again once it is back.
    bool stale = false;
    bool refreshQueued = false;

    // The commands first, then the settings.
    QList<Entry> entries;
    qsizetype commandCount = 0;
};
