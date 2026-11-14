#pragma once

#include <QEvent>
#include <QPainterPath>
#include <QRegion>
#include <QWidget>

// Clip the content stack and all its descendants, leaving the tab pane's border intact.
class PaneMask : public QObject {
public:
    explicit PaneMask(QWidget *widget, int radius) : QObject(widget), m_widget(widget), m_radius(radius) {
        widget->installEventFilter(this);
        updateMask();
    }

    void setRadius(int radius) {
        m_radius = radius;
        updateMask();
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::Resize) updateMask();
        return false;
    }

private:
    void updateMask() {
        if (m_radius <= 0) {
            m_widget->clearMask();
            return;
        }
        QPainterPath path;
        const QRectF content = QRectF(m_widget->rect()).adjusted(1, 1, -1, -1);
        if (!content.isEmpty()) path.addRoundedRect(content, m_radius, m_radius);
        m_widget->setMask(QRegion(path.toFillPolygon().toPolygon()));
    }

    QWidget *m_widget;
    int m_radius;
};
