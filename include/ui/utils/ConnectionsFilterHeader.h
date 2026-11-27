#pragma once

#include <array>
#include <initializer_list>
#include <utility>

#include <QHash>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyle>
#include <QToolButton>

#include "include/ui/utils/ConnectionsTreeModel.h"
#include "include/ui/utils/HeaderSortArrow.h"

class ConnectionsFilterHeader : public QHeaderView {
    Q_OBJECT
public:
    struct Filters {
        QString source;
        QString target;
        QString protocol;
        QString outbound;
    };

    explicit ConnectionsFilterHeader(QWidget *parent = nullptr)
        : QHeaderView(Qt::Horizontal, parent) {
        setSectionsClickable(true);
        setDefaultAlignment(Qt::AlignHCenter | Qt::AlignTop);

        target_filter = makeEdit();
        source_filter = makeEdit();
        protocol_filter = makeEdit();
        outbound_filter = makeEdit();

        connect(this, &QHeaderView::sectionResized, this, &ConnectionsFilterHeader::adjustPositions);

        setFiltersVisible(false);
    }

    static ResizeMode autoResizeMode(int section) {
        switch (section) {
        case ConnectionsTreeModel::ColTarget:   return Stretch;
        // Sized by fitOutboundSection, since it has to give way to a long Target.
        case ConnectionsTreeModel::ColOutbound: return Fixed;
        default:                                return ResizeToContents;
        }
    }

    void applyAutoResizeModes() {
        for (int section = 0; section < count(); ++section) {
            if (!m_userWidths.contains(section)) setSectionResizeMode(section, autoResizeMode(section));
        }
    }

    const QHash<int, int> &userWidths() const { return m_userWidths; }

    void setUserWidths(const QHash<int, int> &widths) {
        m_userWidths.clear();
        for (auto it = widths.cbegin(); it != widths.cend(); ++it) {
            if (!isUserResizable(it.key()) || it.value() <= 0) continue;
            m_userWidths.insert(it.key(), it.value());
            setSectionResizeMode(it.key(), Fixed);
            resizeSection(it.key(), it.value());
        }
        applyAutoResizeModes();
    }

    void resetUserWidths() {
        if (m_userWidths.isEmpty()) return;
        m_userWidths.clear();
        applyAutoResizeModes();
        resizeSections();
        fitOutboundSection();
        emit userWidthsChanged();
    }

    bool filtersVisible() const { return m_filtersVisible; }

    void clearFilterFor(int column) {
        if (QLineEdit *edit = editForColumn(column)) edit->clear();
    }

    Filters filters() const {
        return {textFor(ConnectionsTreeModel::ColSource), textFor(ConnectionsTreeModel::ColTarget),
                textFor(ConnectionsTreeModel::ColProtocol), textFor(ConnectionsTreeModel::ColOutbound)};
    }

    // Marks the sorted section with an arrow past its label; the room reserved for it keeps it off a narrow column's text.
    void setSortSection(int section, bool descending) {
        if (section == m_sortSection && descending == m_sortDescending) return;
        const int previous = m_sortSection;
        m_sortSection = section;
        m_sortDescending = descending;
        if (previous != section) {
            resizeSections();
            fitOutboundSection();
        }
        if (previous >= 0) updateSection(previous);
        if (section >= 0) updateSection(section);
    }

    // Only ever widens: an exact fit would resize the section every poll as live digits change.
    void growSection(int section) {
        const auto *view = qobject_cast<QAbstractItemView *>(parent());
        const int contentWidth = view != nullptr ? view->sizeHintForColumn(section) : -1;
        if (contentWidth <= m_grownWidths.value(section)) return;
        m_grownWidths.insert(section, contentWidth * 115 / 100);
        resizeSections();
    }

    QSize sizeHint() const override {
        QSize s = QHeaderView::sizeHint();
        if (m_filtersVisible) {
            s.setHeight(s.height() + 32);
        }
        return s;
    }

protected:
    // Protocol and Outbound size from their contents, so without a floor their fields shrink to the header label's width.
    QSize sectionSizeFromContents(int logicalIndex) const override {
        QSize s = QHeaderView::sectionSizeFromContents(logicalIndex);
        if (logicalIndex == m_sortSection) {
            s.rwidth() += 2 * HeaderSortArrow::kRoom;
        }
        if (m_filtersVisible && editForColumn(logicalIndex) != nullptr) {
            s.setWidth(qMax(s.width(), 120));
        }
        s.setWidth(qMax(s.width(), m_grownWidths.value(logicalIndex)));
        return s;
    }

    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override {
        QHeaderView::paintSection(painter, rect, logicalIndex);
        if (logicalIndex == m_sortSection) HeaderSortArrow::paint(this, painter, rect, logicalIndex, m_sortDescending);
    }

    // Qt's grip resizes only the section before it and the stretched Target pays, so the edge never moves; here a grip trades width between its neighbours.
    void mousePressEvent(QMouseEvent *event) override {
        if (m_drag.right >= 0) endGripDrag();
        if (event->button() == Qt::LeftButton) {
            const auto [left, right] = gripAt(eventPos(event));
            if (right >= 0) {
                beginGripDrag(left, right, eventPos(event));
                return;
            }
        }
        QHeaderView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (m_drag.right >= 0) {
            if (event->buttons() & Qt::LeftButton) {
                dragGripTo(eventPos(event));
                return;
            }
            endGripDrag();
        }
        if (event->buttons() == Qt::NoButton && gripAt(eventPos(event)).second >= 0) {
            setCursor(Qt::SplitHCursor);
            return;
        }
        QHeaderView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (m_drag.right < 0) {
            QHeaderView::mouseReleaseEvent(event);
            return;
        }
        if (event->button() != Qt::LeftButton) return;
        endGripDrag();
        if (gripAt(eventPos(event)).second < 0) unsetCursor();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override {
        const auto [left, right] = gripAt(eventPos(event));
        if (event->button() != Qt::LeftButton || right < 0) {
            QHeaderView::mouseDoubleClickEvent(event);
            return;
        }
        resetUserWidthsOf({left, right});
    }

    // Runs after every relayout, expand, viewport resize and (via the view's column timer) section resize.
    void updateGeometries() override {
        QHeaderView::updateGeometries();
        fitOutboundSection();
        adjustPositions();
    }

    bool eventFilter(QObject *obj, QEvent *event) override {
        if (!qobject_cast<QLineEdit*>(obj)) return QHeaderView::eventFilter(obj, event);

        // Window shortcuts resolve before the key reaches the field, so bare Return/Del would fire menu actions.
        if (event->type() == QEvent::ShortcutOverride) {
            if (!isTextEditingKey(static_cast<QKeyEvent*>(event))) {
                return QHeaderView::eventFilter(obj, event);
            }
            event->accept();
            return true;
        }

        if (event->type() == QEvent::KeyPress
            && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
            emit closeRequested();
            return true;
        }
        return QHeaderView::eventFilter(obj, event);
    }

public slots:
    void setFiltersVisible(bool visible) {
        m_filtersVisible = visible;

        if (!visible) {
            for (QLineEdit *edit : filterEdits()) edit->clear();
        }

        if (auto btn = qobject_cast<QToolButton*>(sender())) {
            btn->setToolTip(visible ? tr("Disable Filter") : tr("Enable Filter"));
        }

        for (QLineEdit *edit : filterEdits()) edit->setVisible(visible);

        resizeSections();
        emit geometriesChanged();
        adjustPositions();

        // Tab/Backtab/Shortcut focus reasons make QLineEdit select all; OtherFocusReason does not.
        if (visible) {
            target_filter->setFocus(Qt::OtherFocusReason);
        }
    }

    void adjustPositions() {
        if (!m_filtersVisible || count() < ConnectionsTreeModel::ColumnCount) return;

        const int editHeight = 24;
        const int topPos = height() - editHeight - 4;

        auto place = [&](QLineEdit *edit, int section) {
            if (isSectionHidden(section)) {
                edit->hide();
                return;
            }
            edit->show();
            edit->setGeometry(sectionViewportPosition(section) + 2, topPos, sectionSize(section) - 4, editHeight);
        };
        place(target_filter, ConnectionsTreeModel::ColTarget);
        place(source_filter, ConnectionsTreeModel::ColSource);
        place(protocol_filter, ConnectionsTreeModel::ColProtocol);
        place(outbound_filter, ConnectionsTreeModel::ColOutbound);
    }

signals:
    void filtersChanged();
    void userWidthsChanged();
    // The checkable toolbutton owns the visible state, so it has to be the one to untoggle us.
    void closeRequested();

private:
    struct GripDrag {
        int left = -1;  // Target here means the stretch absorbs the change
        int right = -1;
        int pressPos = 0;
        int leftSize = 0;
        int rightSize = 0;
        int minDelta = 0;
        int maxDelta = 0;
        int delta = 0;
    };

    static int eventPos(const QMouseEvent *event) { return event->position().toPoint().x(); }

    void beginGripDrag(int left, int right, int pos) {
        using Model = ConnectionsTreeModel;
        // A poll re-measuring an auto section mid-drag would slide the grip out from under the mouse.
        for (int section = 0; section < count(); ++section) {
            if (section != Model::ColTarget && !m_userWidths.contains(section)) setSectionResizeMode(section, Fixed);
        }
        const int leftFloor = left == Model::ColTarget ? qMax(minimumSectionSize(), sectionSizeHint(left)) : minimumSectionSize();
        m_drag = {left, right, pos, sectionSize(left), sectionSize(right)};
        m_drag.minDelta = qMin(0, leftFloor - m_drag.leftSize);
        m_drag.maxDelta = qMax(0, m_drag.rightSize - minimumSectionSize());
        setCursor(Qt::SplitHCursor);
    }

    void dragGripTo(int pos) {
        const int moved = isRightToLeft() ? m_drag.pressPos - pos : pos - m_drag.pressPos;
        const int delta = qBound(m_drag.minDelta, moved, m_drag.maxDelta);
        if (delta == m_drag.delta) return;
        m_drag.delta = delta;
        resizeSection(m_drag.right, m_drag.rightSize - delta);
        if (m_drag.left != ConnectionsTreeModel::ColTarget) resizeSection(m_drag.left, m_drag.leftSize + delta);
        // Qt only schedules the stretch pass; running it now keeps Target from lagging a frame behind the edge.
        resizeSections();
    }

    void endGripDrag() {
        using Model = ConnectionsTreeModel;
        const GripDrag drag = std::exchange(m_drag, {});
        if (drag.delta != 0) {
            if (drag.left != Model::ColTarget) m_userWidths.insert(drag.left, sectionSize(drag.left));
            m_userWidths.insert(drag.right, sectionSize(drag.right));
        }
        applyAutoResizeModes();
        resizeSections();
        // Moving Target's edge changes the room Outbound fits into, so refitting Outbound would pull the edge back.
        if (drag.delta != 0 && drag.left == Model::ColTarget && !m_userWidths.contains(Model::ColOutbound)
            && outboundFitWidth() != sectionSize(Model::ColOutbound)) {
            m_userWidths.insert(Model::ColOutbound, sectionSize(Model::ColOutbound));
        }
        fitOutboundSection();
        if (drag.delta != 0) emit userWidthsChanged();
    }

    void resetUserWidthsOf(std::initializer_list<int> sections) {
        bool changed = false;
        for (const int section : sections) changed |= m_userWidths.remove(section);
        if (!changed) return;
        applyAutoResizeModes();
        resizeSections();
        fitOutboundSection();
        emit userWidthsChanged();
    }

    // Outbound fits its content into what the stretched Target leaves past the Target's own content, so a long address wins.
    int outboundFitWidth() const {
        using Model = ConnectionsTreeModel;
        const auto *view = qobject_cast<QAbstractItemView *>(parent());
        if (view == nullptr || count() < Model::ColumnCount) return -1;

        const int floor = sectionSizeHint(Model::ColOutbound);
        const int wanted = qMax(floor, view->sizeHintForColumn(Model::ColOutbound));
        int room = viewport()->width() - qMax(sectionSizeHint(Model::ColTarget), view->sizeHintForColumn(Model::ColTarget));
        for (int section = 0; section < count(); ++section) {
            if (section != Model::ColTarget && section != Model::ColOutbound && !isSectionHidden(section))
                room -= sectionSize(section);
        }
        return qBound(floor, room, wanted);
    }

    void fitOutboundSection() {
        using Model = ConnectionsTreeModel;
        if (m_drag.right >= 0 || m_userWidths.contains(Model::ColOutbound)) return;
        const int width = outboundFitWidth();
        if (width > 0 && width != sectionSize(Model::ColOutbound)) resizeSection(Model::ColOutbound, width);
    }

    // Target stretches into whatever the other columns leave, so it has no width of its own to keep.
    bool isUserResizable(int section) const {
        return section > ConnectionsTreeModel::ColTarget && section < count();
    }

    int adjacentVisibleVisual(int visual, int step) const {
        for (visual += step; visual >= 0 && visual < count(); visual += step) {
            if (!isSectionHidden(logicalIndex(visual))) return visual;
        }
        return -1;
    }

    // Located as QHeaderViewPrivate::sectionHandleAt does; the last section's far edge is held at the viewport edge by the stretch, so it has no grip.
    std::pair<int, int> gripAt(int position) const {
        int visual = visualIndexAt(position);
        if (visual < 0) return {-1, -1};
        const int section = logicalIndex(visual);
        const int grip = style()->pixelMetric(QStyle::PM_HeaderGripMargin, nullptr, this);
        const int start = sectionViewportPosition(section);
        bool atStart = position < start + grip;
        bool atEnd = position > start + sectionSize(section) - grip;
        if (isRightToLeft()) std::swap(atStart, atEnd);
        if (atStart) visual = adjacentVisibleVisual(visual, -1);
        else if (!atEnd) return {-1, -1};
        if (visual < 0) return {-1, -1};
        const int next = adjacentVisibleVisual(visual, 1);
        if (next < 0) return {-1, -1};
        return {logicalIndex(visual), logicalIndex(next)};
    }

    QLineEdit *makeEdit() {
        auto *edit = new QLineEdit(this->viewport());
        edit->setPlaceholderText(tr("Filter..."));
        edit->setClearButtonEnabled(true);
        edit->installEventFilter(this);
        connect(edit, &QLineEdit::textChanged, this, [this] { emit filtersChanged(); });
        return edit;
    }

    QLineEdit *editForColumn(int column) const {
        switch (column) {
        case ConnectionsTreeModel::ColTarget:   return target_filter;
        case ConnectionsTreeModel::ColSource:   return source_filter;
        case ConnectionsTreeModel::ColProtocol: return protocol_filter;
        case ConnectionsTreeModel::ColOutbound: return outbound_filter;
        default:                                return nullptr;
        }
    }

    // A hidden column must report no filter, or its stale text would keep filtering the table invisibly.
    QString textFor(int column) const {
        QLineEdit *edit = editForColumn(column);
        if (edit == nullptr || isSectionHidden(column)) return {};
        return edit->text();
    }

    std::array<QLineEdit*, 4> filterEdits() const {
        return {target_filter, source_filter, protocol_filter, outbound_filter};
    }

    static bool isTextEditingKey(QKeyEvent *key) {
        if (!(key->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            return key->key() < Qt::Key_F1 || key->key() > Qt::Key_F35;
        }
        for (auto standard : {QKeySequence::SelectAll, QKeySequence::Copy, QKeySequence::Cut,
                              QKeySequence::Paste, QKeySequence::Undo, QKeySequence::Redo,
                              QKeySequence::MoveToStartOfLine, QKeySequence::MoveToEndOfLine,
                              QKeySequence::SelectStartOfLine, QKeySequence::SelectEndOfLine,
                              QKeySequence::DeleteStartOfWord, QKeySequence::DeleteEndOfWord}) {
            if (key->matches(standard)) return true;
        }
        return false;
    }

    QLineEdit *target_filter;
    QLineEdit *source_filter;
    QLineEdit *protocol_filter;
    QLineEdit *outbound_filter;
    bool m_filtersVisible = false;
    int m_sortSection = -1;
    bool m_sortDescending = false;
    QHash<int, int> m_grownWidths;
    QHash<int, int> m_userWidths;
    GripDrag m_drag;
};
