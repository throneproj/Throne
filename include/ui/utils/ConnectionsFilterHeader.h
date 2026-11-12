#pragma once

#include <array>

#include <QHash>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
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
    // The checkable toolbutton owns the visible state, so it has to be the one to untoggle us.
    void closeRequested();

private:
    // Outbound fits its content into what the stretched Target leaves past the Target's own content, so a long address wins.
    void fitOutboundSection() {
        using Model = ConnectionsTreeModel;
        const auto *view = qobject_cast<QAbstractItemView *>(parent());
        if (view == nullptr || count() < Model::ColumnCount) return;

        const int floor = sectionSizeHint(Model::ColOutbound);
        const int wanted = qMax(floor, view->sizeHintForColumn(Model::ColOutbound));
        int room = viewport()->width() - qMax(sectionSizeHint(Model::ColTarget), view->sizeHintForColumn(Model::ColTarget));
        for (int section = 0; section < count(); ++section) {
            if (section != Model::ColTarget && section != Model::ColOutbound && !isSectionHidden(section))
                room -= sectionSize(section);
        }
        const int width = qBound(floor, room, wanted);
        if (width != sectionSize(Model::ColOutbound)) resizeSection(Model::ColOutbound, width);
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
};
