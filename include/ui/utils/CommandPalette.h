#pragma once

#include <functional>

#include <QFrame>
#include <QLineEdit>
#include <QStandardItemModel>

class QAction;
class QLabel;
class QListView;

// Ctrl+K search over menu commands and settings; the caller supplies the entries and what each one runs.
class CommandPalette : public QFrame {
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
        QString shortcut;
        bool enabled = true;
        bool checkable = false;
        bool checked = false;
        std::function<void()> run;
    };

    explicit CommandPalette(QWidget *parent);

    void setEntries(QList<Entry> list);

    void popup(const QPoint &topCenter, int width, const QString &text = {});

    // Drops "&" mnemonics, keeping "&&" as a literal ampersand, and a trailing colon or ellipsis.
    static QString plainText(QString text);

    // Search words from an object name ("actionUrl_Test_Group" -> "Url Test Group"); empty for Designer's defaults.
    static QString keywordsFor(const QString &objectName);

    // Text fields bind Ctrl+K to "delete to end of line" on Linux and macOS; stop them swallowing the action's shortcut in window.
    static void keepShortcutFromTextFields(QAction *action, QWidget *window);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void refilter();
    void addHeader(const QString &title);
    void addEntry(int entryIndex, int matchStart, int matchLength);
    void selectFirst();
    void moveSelection(int delta);
    void activate(const QModelIndex &index);
    void fitHeight();

    QLineEdit *edit;
    QListView *list;
    QLabel *hint;
    QStandardItemModel model;
    QList<Entry> entries;
};

// Toolbar field that only opens the palette: the typing happens in the palette itself.
class CommandPaletteLauncher : public QLineEdit {
    Q_OBJECT

public:
    explicit CommandPaletteLauncher(QWidget *parent);

signals:
    void launchRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
};
