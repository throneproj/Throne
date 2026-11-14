#pragma once

#include <QTableView>
#include <functional>

class ProfilesTableView : public QTableView {
    Q_OBJECT
public:
    explicit ProfilesTableView(QWidget *parent = nullptr);

    // Drops in source-model rows; target == row count means the unpinned end.
    std::function<void(int from, int target, bool below)> rowDropped;

    void updatePinnedIndicator();

    void setModel(QAbstractItemModel *model) override;

    int firstVisibleRow();

protected:
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    class ProfilesTableVerticalHeader *m_verticalHeader = nullptr;
    class ProfilesTableFilterHeader *m_filterHeader = nullptr;
    class ProfilesFilterProxyModel *m_filterProxy = nullptr;
};
