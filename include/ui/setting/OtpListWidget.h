#pragma once

#include <QListWidget>
#include <QPoint>

// Drag is driven by hand: Qt's own carries the selection, and these rows are unselectable.
class OtpListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit OtpListWidget(QWidget *parent = nullptr);

signals:
    void reorderRequested(int from, int to);

protected:
    void mousePressEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;

    void dragMoveEvent(QDragMoveEvent *event) override;

    void dragLeaveEvent(QDragLeaveEvent *event) override;

    void dropEvent(QDropEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

    // Where the row would be inserted: 0..count(), count() meaning past the last row.
    [[nodiscard]] int insertionRowAt(const QPoint &pos) const;

    void setDropRow(int row);
private:
    QPoint pressPos;

    int pressedRow = -1;

    int dropRow = -1;
};
