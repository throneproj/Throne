#pragma once

#include <QListView>

#include <memory>

#include "include/ui/setting/route/RouteEditorCommon.h"

class RouteRuleListModel;

// The Rules tab's rows, one line each, painted by a delegate, with "Everything else" fixed last.
// Any row but that one drags to a new place; the list only asks for changes, the page makes them.
class RouteRuleList : public QListView {
    Q_OBJECT

public:
    explicit RouteRuleList(std::shared_ptr<Configs::RouteProfile> profile, QWidget* parent = nullptr);
    ~RouteRuleList() override;

    // Rows from profile->Rules again, nothing selected; a row whose rules are unchanged keeps its look unless recompute.
    void rebuild(bool recompute = false);

    [[nodiscard]] const QList<RouteEditor::Row>& rows() const;

    // Recomputes the row's cached title, summary, pill and issue after its rules changed.
    void refreshRow(int row);

    // The route the "Everything else" row shows.
    void setDefaultOutbound(int outboundID);

    // Hides the rows whose title, summary and values all miss text; dragging is off while it is set.
    void setFilter(const QString& text);
    [[nodiscard]] bool isRowShown(int row) const;

    // -1 for none.
    [[nodiscard]] int selectedRow() const;
    void setSelectedRow(int row);

signals:
    // -1 when nothing is selected.
    void selectedRowChanged(int row);
    // to is the row's index in the new order, never past the last row; sent after the event that caused it.
    void reorderRequested(int from, int to);
    // The row's check box, or Space; sent after the event.
    void toggleRequested(int row);
    // The Delete key while the list has focus; sent after the event.
    void deleteRequested();
    // Sent after the event, with the row already selected.
    void menuRequested(int row, const QPoint& globalPos);

protected:
    void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
    QModelIndex moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void startRowDrag(int row);
    [[nodiscard]] int dropGapAt(const QPoint& pos) const;
    [[nodiscard]] int dropIndicatorY(int gap) const;
    [[nodiscard]] QPixmap rowPixmap(int row) const;
    [[nodiscard]] bool onCheckBox(int row, const QPoint& pos) const;
    void applyFilter(int row);
    void requestReorder(int from, int to);

    RouteRuleListModel* rowModel;
    QString filter;
    QPoint pressPos;
    int pressRow = -1;
    int dragRow = -1;
    int dropGap = -1;
    int droppedTo = -1;
};
