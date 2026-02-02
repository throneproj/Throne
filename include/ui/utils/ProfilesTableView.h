#pragma once

#include <QTableView>
#include <functional>

// Table view for the profiles list: drag-drop reorder and custom vertical header.
class ProfilesTableView : public QTableView {
    Q_OBJECT
public:
    explicit ProfilesTableView(QWidget *parent = nullptr);

    // Called when user drops to reorder: (sourceRow, destinationRow).
    std::function<void(int row1, int row2)> rowsSwapped;

    void setModel(QAbstractItemModel *model) override;

    int firstVisibleRow();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    class ProfilesTableVerticalHeader *m_verticalHeader = nullptr;
};
