#pragma once

#include <QAbstractListModel>
#include <QDialog>
#include <QList>
#include <QStringList>

class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeView;
class ProcessPickerModel;
class ProcessPickerFilter;

// Programs the core has seen connecting or finds running, turned into process rule values spelled as the core matches them.
class ProcessPicker : public QDialog {
    Q_OBJECT

public:
    // Name: process_name. Path: process_path. Folder: a process_path_regex covering the app's install folder.
    enum class Match { Name, Path, Folder };

    // allowed: the kinds the caller takes, the first one preselected.
    ProcessPicker(QWidget* parent, const QList<Match>& allowed);

    [[nodiscard]] Match match() const;

    // The selection in the chosen kind, in list order, without duplicates.
    [[nodiscard]] QStringList values() const;

    // Empty when cancelled.
    static QStringList Pick(QWidget* parent, const QList<Match>& allowed, Match* match = nullptr);

    // One program as its connections report it: the path (a service's DLL for a svchost service), else the name. Empty when cancelled.
    static QString ChooseProgram(QWidget* parent);

private:
    ProcessPicker(QWidget* parent, const QList<Match>& allowed, bool oneProgram);

    bool chooseOne = false;
    ProcessPickerModel* model;
    ProcessPickerFilter* filter;
    QTreeView* view;
    QLineEdit* search;
    QComboBox* scope;
    QPushButton* refreshButton;
    QButtonGroup* matchGroup;
    QLabel* status;
    QLabel* summary;
    QPushButton* addButton;
    int generation = 0;

    void reload();
    void browse();
    void applyFilter();
    void updateSummary();
    void fitColumns();
    [[nodiscard]] QList<int> selectedRows() const;
    [[nodiscard]] QString chosenProgram() const;
};

// Completion rows for process rule editors: head, then programs spelled as the core matches them, then tail.
// The programs load in the background and are shared by every instance.
class ProcessCompletionModel : public QAbstractListModel {
public:
    struct Layout {
        QStringList head;
        bool names = false;
        QString namePrefix;
        bool paths = false;
        QString pathPrefix;
        QStringList tail;
    };

    ProcessCompletionModel(Layout layout, QObject* parent);

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

    void rebuild();

private:
    struct Suggestion {
        QString text;
        QString path;
        QString tip;
        bool operator==(const Suggestion&) const = default;
    };

    Layout config;
    QList<Suggestion> programs;
};
