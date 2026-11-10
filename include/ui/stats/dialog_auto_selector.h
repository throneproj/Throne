#pragma once

#include <QDialog>

#include "include/stats/autoselector/AutoSelectorMonitor.hpp"

class QLabel;
class QTableWidget;
class QPushButton;
class QCheckBox;

class DialogAutoSelector : public QDialog {
    Q_OBJECT

public:
    explicit DialogAutoSelector(QWidget *parent = nullptr);

    void refresh();

private:
    void buildRows(const Stats::AutoSelectorView &view);

    // QTableWidget's own sort compares displayed strings, ordering "100 ms" before "20 ms".
    void sortRows(QList<Stats::AutoSelectorMemberView> &rows, const Stats::AutoSelectorView &view) const;

    void onHeaderClicked(int column);

    void fitToColumns();

    // An empty `tag` releases the pin.
    void applySelection(const QString &tag);

    [[nodiscard]] QString highlightedTag() const;

    QLabel *m_headline = nullptr;
    QLabel *m_detail = nullptr;
    QLabel *m_footer = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_recheck = nullptr;
    QPushButton *m_pin = nullptr;
    QPushButton *m_release = nullptr;
    QCheckBox *m_onlyProblems = nullptr;

    QString m_pinnedTag;

    int m_sortColumn = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    bool m_columnsSized = false;
};
