#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QDropEvent>
#include <QDebug>
#include <functional>
#include <utility>

class MyTableWidget : public QTableWidget {
public:
    explicit MyTableWidget(QWidget *parent = nullptr) : QTableWidget(parent) {
        // 拖拽设置
        this->setDragDropMode(QAbstractItemView::InternalMove); // 内部移动
        this->setDropIndicatorShown(true);                      // drop位置 提示
        this->setSelectionBehavior(QAbstractItemView::SelectRows);
    };

    QList<int> order;          // id sorted (save)
    std::map<int, int> id2Row; // id2Row
    QList<int> row2Id;         // row2Id: use this to refresh data

    std::function<void()> callback_save_order;
    std::function<void(int id)> refresh_data;

    void _save_order(bool saveToFile) {
        order.clear();
        id2Row.clear();
        for (int i = 0; i < this->rowCount(); i++) {
            auto id = row2Id[i];
            order += id;
            id2Row[id] = i;
        }
        if (callback_save_order != nullptr && saveToFile)
        {
            callback_save_order();
        }
    }

    void update_order(bool saveToFile) {
        if (order.isEmpty()) {
            _save_order(false);
            return;
        }

        // Then save the order
        _save_order(saveToFile);
    };

protected:
    void dropEvent(QDropEvent *event) override {
        if (order.isEmpty()) return;

        int row_src, row_dst;
        row_src = this->currentRow();
        auto id_src = row2Id[row_src];
        QTableWidgetItem *item = this->itemAt(event->position().toPoint());
        if (item != nullptr) {
            row_dst = item->row();
            // Modify order
            row2Id.swapItemsAt(row_src, row_dst);
            order.removeAt(row_src);
            order.insert(row_dst, id_src);
        } else {
            return;
        }

        // Do update order & refresh
        clearSelection();
        update_order(true);
        refresh_data(-1);
    };
};
