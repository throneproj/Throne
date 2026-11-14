#pragma once

#include <array>
#include <memory>

#include <QCoreApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QVector>
#include <QScrollBar>
#include <QToolButton>
#include <QStyle>

#include "include/ui/utils/HeaderSortArrow.h"
#include "include/ui/utils/ProfilesTableModel.h"
#include "include/ui/widget/SubscriptionInfoCard.hpp"

namespace Configs {
    class Group;
}

class ProfilesTableFilterHeader : public QHeaderView {
    Q_OBJECT
public:
    // The band is the header's own child: setOffset() scrolls viewport children.
    explicit ProfilesTableFilterHeader(QWidget *parent = nullptr)
        : QHeaderView(Qt::Horizontal, parent), m_subCard(new SubscriptionInfoCard(this)) {
        setSectionsClickable(true);
        setDefaultAlignment(Qt::AlignHCenter | Qt::AlignTop);

        // Clicks on the band must not reach QHeaderView, which would sort by the section under them.
        m_subCard->setAttribute(Qt::WA_NoMousePropagation, true);
        m_subCard->installEventFilter(this);
        connect(m_subCard, &SubscriptionInfoCard::cardVisibilityChanged, this, [this] {
            emit geometriesChanged();
            adjustPositions();
        });

        type_filter = new QLineEdit(this->viewport()); 
        type_filter->setPlaceholderText(tr("Filter..."));
        type_filter->setClearButtonEnabled(true);
        connect(type_filter, &QLineEdit::textChanged, [this](const QString &text) {
            emit typeFilterChanged(text);
        });

        address_filter = new QLineEdit(this->viewport());
        address_filter->setPlaceholderText(tr("Filter..."));
        address_filter->setClearButtonEnabled(true);
        connect(address_filter, &QLineEdit::textChanged, [this](const QString &text) {
            emit addressFilterChanged(text);
        });

        name_filter = new QLineEdit(this->viewport()); 
        name_filter->setPlaceholderText(tr("Filter..."));
        name_filter->setClearButtonEnabled(true);
        connect(name_filter, &QLineEdit::textChanged, [this](const QString &text) {
            emit nameFilterChanged(text);
        });

        test_filter = new QLineEdit(this->viewport()); 
        test_filter->setPlaceholderText(tr("Filter by country or service..."));
        test_filter->setToolTip(tr("Countries and working services, e.g. \"DE gemini\" or \"ai\".\n"
                                   "\"!netflix\" finds profiles Netflix refused."));
        test_filter->setClearButtonEnabled(true);
        connect(test_filter, &QLineEdit::textChanged, [this](const QString &text) {
            emit testFilterChanged(text);
        });

        for (QLineEdit *edit : filterEdits()) edit->installEventFilter(this);

        connect(this, &QHeaderView::sectionResized, this, &ProfilesTableFilterHeader::adjustPositions);

        setFiltersVisible(false);
    }

    void setGroup(const std::shared_ptr<Configs::Group> &group) {
        m_subCard->setGroup(group);
    }

    void setLastFilterColumn(int column) {
        // The former combined test column (3) held the country filter.
        if (column == ProfilesTableModel::ColLatency) column = ProfilesTableModel::ColExitIP;
        m_lastFilterColumn = editForColumn(column) ? column : ProfilesTableModel::ColName;
    }

    bool filtersVisible() const { return m_filtersVisible; }

    // Section -1 hides the arrow. Returns whether the section moved, which changes the reserved room and so the fitted widths.
    bool setSortSection(int section, bool descending) {
        if (section == m_sortSection && descending == m_sortDescending) return false;
        const int previous = m_sortSection;
        m_sortSection = section;
        m_sortDescending = descending;
        if (previous >= 0) updateSection(previous);
        if (section >= 0) updateSection(section);
        return previous != section;
    }

    void focusLastFilterField() {
        if (!m_filtersVisible) return;
        QLineEdit *edit = editForColumn(m_lastFilterColumn);
        if (!edit || isSectionHidden(columnOf(edit))) {
            edit = nullptr;
            for (auto *candidate : filterEdits()) {
                if (!isSectionHidden(columnOf(candidate))) {
                    edit = candidate;
                    break;
                }
            }
        }
        if (!edit) {
            emit focusTableRequested(false);
            return;
        }
        // Tab/Backtab/Shortcut focus reasons make QLineEdit select all; OtherFocusReason does not.
        edit->setFocus(Qt::OtherFocusReason);
        edit->end(false);
    }

    QSize sizeHint() const override {
        QSize s = QHeaderView::sizeHint();
        // Keep a right-clickable header even when every column has been unchecked.
        s.setHeight(qMax(s.height(), fontMetrics().height() + 2 * style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, this)));
        if (m_filtersVisible) {
            s.setHeight(s.height() + 32);
        }
        if (!m_subCard->isHidden()) {
            s.setHeight(s.height() + m_subCard->height());
        }
        return s;
    }

protected:
    QSize sectionSizeFromContents(int logicalIndex) const override {
        QSize s = QHeaderView::sectionSizeFromContents(logicalIndex);
        if (logicalIndex == m_sortSection) s.rwidth() += 2 * HeaderSortArrow::kRoom;
        return s;
    }

    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override {
        const int bandHeight = m_subCard->isHidden() ? 0 : m_subCard->height();
        const QRect sectionRect = rect.adjusted(0, bandHeight, 0, 0);
        QHeaderView::paintSection(painter, sectionRect, logicalIndex);
        if (logicalIndex == m_sortSection) HeaderSortArrow::paint(this, painter, sectionRect, logicalIndex, m_sortDescending);
    }

    void updateGeometries() override {
        QHeaderView::updateGeometries();
        adjustPositions();
    }

    bool eventFilter(QObject *obj, QEvent *event) override {
        // WA_NoMousePropagation stops the wheel at the band too.
        if (obj == m_subCard && event->type() == QEvent::Wheel) {
            if (auto *view = qobject_cast<QAbstractScrollArea*>(parentWidget())) return QCoreApplication::sendEvent(view->viewport(), event);
        }
        if (!qobject_cast<QLineEdit*>(obj)) return QHeaderView::eventFilter(obj, event);

        // Window shortcuts resolve before the key reaches the field, so bare Return/Del would fire menu actions.
        if (event->type() == QEvent::ShortcutOverride) {
            if (!isTextEditingKey(static_cast<QKeyEvent*>(event))) {
                return QHeaderView::eventFilter(obj, event);
            }
            event->accept();
            return true;
        }

        if (event->type() == QEvent::KeyPress) {
            switch (static_cast<QKeyEvent*>(event)->key()) {
            case Qt::Key_Escape:
                emit closeRequested();
                return true;
            case Qt::Key_Down:
                emit focusTableRequested(true);
                return true;
            default:
                break;
            }
        }
        return QHeaderView::eventFilter(obj, event);
    }

public slots:
    void setFiltersVisible(bool visible) {
        m_filtersVisible = visible;

        QLineEdit *focused = focusedEdit();
        if (!visible) {
            if (focused) {
                m_lastFilterColumn = columnOf(focused);
                emit lastFilterColumnChanged(m_lastFilterColumn);
            }
            for (QLineEdit *edit : filterEdits()) edit->clear();
        }

        if (auto btn = qobject_cast<QToolButton*>(sender())) {
            btn->setToolTip(QString("%1\n%2").arg(visible ? tr("Disable Filter") : tr("Enable Filter"), QKeySequence(QKeySequence::Find).toString(QKeySequence::NativeText)));
        }

        for (QLineEdit *edit : filterEdits()) edit->setVisible(visible);

        emit geometriesChanged();
        adjustPositions();

        if (visible) focusLastFilterField();
        else if (focused) emit focusTableRequested(false);
    }

    void adjustPositions() {
        if (!m_subCard->isHidden()) {
            m_subCard->setGeometry(0, 0, width(), m_subCard->height());
            m_subCard->raise();
        }

        if (!m_filtersVisible || !address_filter || !name_filter || !type_filter
            || !test_filter || count() < ProfilesTableModel::ColumnCount) {
	        return;
	    }

        const int editHeight = 24;
        const int topPos = height() - editHeight - 4;
        QLineEdit *hiddenFocus = nullptr;

        auto place = [&](QLineEdit *edit, int section) {
            const bool visible = !isSectionHidden(section);
            if (!visible) {
                if (edit->hasFocus()) hiddenFocus = edit;
                edit->clear();
            }
            edit->setVisible(visible);
            if (!visible) return;
            edit->setGeometry(sectionViewportPosition(section) + 2, topPos, sectionSize(section) - 4, editHeight);
        };
        place(type_filter, ProfilesTableModel::ColType);
        place(address_filter, ProfilesTableModel::ColAddress);
        place(name_filter, ProfilesTableModel::ColName);
        place(test_filter, ProfilesTableModel::ColExitIP);
        if (hiddenFocus) focusLastFilterField();
    }

signals:
    void typeFilterChanged(const QString &text);
    void addressFilterChanged(const QString &text);
    void nameFilterChanged(const QString &text);
    void testFilterChanged(const QString &text);
    void focusTableRequested(bool selectFirst);
    void lastFilterColumnChanged(int column);
    // The checkable toolbutton owns the visible state, so it has to be the one to untoggle us.
    void closeRequested();

private:
    std::array<QLineEdit*, 4> filterEdits() const {
        return {type_filter, address_filter, name_filter, test_filter};
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

    QLineEdit *focusedEdit() const {
        for (QLineEdit *edit : filterEdits()) {
            if (edit->hasFocus()) return edit;
        }
        return nullptr;
    }

    QLineEdit *editForColumn(int column) const {
        switch (column) {
        case ProfilesTableModel::ColType:       return type_filter;
        case ProfilesTableModel::ColAddress:    return address_filter;
        case ProfilesTableModel::ColName:       return name_filter;
        case ProfilesTableModel::ColExitIP:     return test_filter;
        default:                                return nullptr;
        }
    }

    int columnOf(QLineEdit *edit) const {
        if (edit == type_filter)    return ProfilesTableModel::ColType;
        if (edit == address_filter) return ProfilesTableModel::ColAddress;
        if (edit == name_filter)    return ProfilesTableModel::ColName;
        if (edit == test_filter)    return ProfilesTableModel::ColExitIP;
        return -1;
    }

    SubscriptionInfoCard *m_subCard;
    QLineEdit* type_filter;
    QLineEdit* address_filter;
    QLineEdit* name_filter;
    QLineEdit* test_filter;
    bool m_filtersVisible = false;
    int m_lastFilterColumn = ProfilesTableModel::ColName;
    int m_sortSection = -1;
    bool m_sortDescending = false;
};
