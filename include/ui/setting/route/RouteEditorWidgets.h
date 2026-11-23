#pragma once

#include <QLayout>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QWidget>

#include "include/global/Validation.hpp"
#include "include/ui/setting/route/RouteEditorCommon.h"

class QHBoxLayout;
class QSpacerItem;
class QVBoxLayout;

namespace RouteEditor {
    // Lays its items out like words in a paragraph, wrapping at the available width; right to left, it mirrors, margins included.
    class FlowLayout : public QLayout {
    public:
        explicit FlowLayout(QWidget* parent = nullptr, int hSpacing = 6, int vSpacing = 6);
        ~FlowLayout() override;

        void addItem(QLayoutItem* item) override;
        [[nodiscard]] int count() const override;
        [[nodiscard]] QLayoutItem* itemAt(int index) const override;
        QLayoutItem* takeAt(int index) override;
        [[nodiscard]] Qt::Orientations expandingDirections() const override;
        [[nodiscard]] bool hasHeightForWidth() const override;
        [[nodiscard]] int heightForWidth(int width) const override;
        [[nodiscard]] QSize minimumSize() const override;
        [[nodiscard]] QSize sizeHint() const override;
        void setGeometry(const QRect& rect) override;

        // Deletes every item and its widget.
        void clear();

        // Every item starts a line of its own, except a button, which stays beside the item before it.
        void setStacked(bool stacked);

        // The height of the first line when laid out at width, margins included in width.
        [[nodiscard]] int firstLineHeight(int width) const;

    private:
        int doLayout(const QRect& rect, bool testOnly, int* firstLine = nullptr) const;

        QList<QLayoutItem*> items;
        int hSpace;
        int vSpace;
        bool stacked = false;
    };

    // A value with a remove button: the button or Delete removes it, a double-click or Enter asks to edit it.
    class ValueChip : public QWidget {
        Q_OBJECT

    public:
        explicit ValueChip(const QString& text, QWidget* parent = nullptr);

        [[nodiscard]] QString text() const { return value; }

        // A short muted label before the value, such as a quick list entry's type.
        void setKind(const QString& kind);

        void setRemovable(bool removable);

        // Shorter and tighter, for the lines of a condition tree.
        void setCompact(bool compact);

        // Tints the chip and shows message in its tooltip; an empty message clears it.
        void setIssue(Validation::Severity severity, const QString& message);

        [[nodiscard]] QSize sizeHint() const override;
        [[nodiscard]] QSize minimumSizeHint() const override;

    signals:
        void removeRequested();
        void editRequested();

    protected:
        bool event(QEvent* event) override;
        void paintEvent(QPaintEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void mouseDoubleClickEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void leaveEvent(QEvent* event) override;

    private:
        [[nodiscard]] QRect removeRect() const;
        [[nodiscard]] QString shownValue() const;

        QString value;
        QString kindText;
        QString issueText;
        Validation::Severity issueSeverity = Validation::Severity::Error;
        bool removable = true;
        bool compact = false;
        bool hoverRemove = false;
        bool pressedRemove = false;
    };

    // A Then pill as a widget.
    class ThenPill : public QWidget {
        Q_OBJECT

    public:
        explicit ThenPill(QWidget* parent = nullptr);

        void setPill(const QString& text, Tone tone);

        [[nodiscard]] QSize sizeHint() const override;
        [[nodiscard]] QSize minimumSizeHint() const override;

    protected:
        void paintEvent(QPaintEvent* event) override;

    private:
        QString label;
        Tone tone = Tone::Neutral;
    };

    // An operator badge over its branches, joined to them by lines painted on the leading side.
    class TreeNode : public QWidget {
        Q_OBJECT

    public:
        explicit TreeNode(const QString& op, QWidget* parent = nullptr);

        // The badge's line: the badge, then a stretch; the caller may add more after it.
        [[nodiscard]] QHBoxLayout* head() const { return headLayout; }

        // The item hugs its content; note spans the width under it, without a line of its own.
        void addBranch(QWidget* item, QWidget* note = nullptr);
        // Always the last branch, joined by a dashed line; clearBranches keeps it.
        void setTail(QWidget* item);
        // Deletes the branches later, as one of their widgets may be running a signal.
        void clearBranches();

        // Off hides the badge too, but the branches keep their indent.
        void setLinesShown(bool shown);

        // Where a parent's line meets this node: the middle of its badge.
        [[nodiscard]] int anchorY() const;
        // The spine's x in this widget, mirrored right to left.
        [[nodiscard]] int spineX() const;
        // How far the branches sit from the leading edge.
        [[nodiscard]] int indent() const;

    protected:
        bool event(QEvent* event) override;
        void changeEvent(QEvent* event) override;
        bool eventFilter(QObject* watched, QEvent* event) override;
        void paintEvent(QPaintEvent* event) override;

    private:
        struct Branch {
            QPointer<QWidget> slot;
            QPointer<QWidget> item;
        };

        // From the leading edge, the same either way.
        [[nodiscard]] int spineOffset() const;

        QString operatorText;
        QWidget* badge = nullptr;
        QHBoxLayout* headLayout = nullptr;
        QVBoxLayout* kids = nullptr;
        QSpacerItem* indentSpacer = nullptr;
        QList<Branch> branches;
        QPointer<QWidget> tail;
        bool linesShown = true;
    };

    // A condition in a TreeNode: a muted label and its values stacked one per line, in a small box that hugs them.
    class TreeLeaf : public QWidget {
        Q_OBJECT

    public:
        explicit TreeLeaf(QWidget* parent = nullptr);

        // tip, when not empty, shows over the label.
        void setLabel(const QString& text, const QString& tip = {});
        // The labels of the sibling leaves; all of them take the widest one's width.
        void setLabelColumn(const QStringList& labels);

        // Stacked; the leaf's own layout.
        [[nodiscard]] FlowLayout* flow() const { return values; }

        // Where a tree line meets the leaf: the middle of its first line.
        [[nodiscard]] int anchorY() const;

    protected:
        bool event(QEvent* event) override;
        void changeEvent(QEvent* event) override;
        void paintEvent(QPaintEvent* event) override;

    private:
        [[nodiscard]] int labelWidth() const;
        [[nodiscard]] QRect labelRect() const;
        void updateMargins();

        FlowLayout* values = nullptr;
        QString label;
        QString labelTip;
        QStringList column;
    };
} // namespace RouteEditor
