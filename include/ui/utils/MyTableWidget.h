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
        this->setDragDropMode(InternalMove);
        this->setDropIndicatorShown(true);
        this->setSelectionBehavior(SelectRows);
    }

    std::function<void(int row1, int row2)> rowsSwapped;
protected:
    void dropEvent(QDropEvent *event) override {
        if (const QTableWidgetItem *item = this->itemAt(event->position().toPoint()); item != nullptr) {
            const int row_dst = item->row();
            if (rowsSwapped)
            {
                rowsSwapped(row_dst, currentRow());
                clearSelection();
            }
        }
    }
};
