#pragma once

#include <QLayout>
#include <QList>
#include <QWidget>

#include "include/global/Validation.hpp"
#include "include/ui/setting/route/RouteEditorCommon.h"

namespace RouteEditor {
    // Lays its items out like words in a paragraph, wrapping at the available width.
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

    private:
        int doLayout(const QRect& rect, bool testOnly) const;

        QList<QLayoutItem*> items;
        int hSpace;
        int vSpace;
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
} // namespace RouteEditor
