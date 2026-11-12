#pragma once

#include <QLabel>

// A plain-text label a layout may squeeze below its text width: each line elides, and the full text becomes the tooltip.
class ElidingLabel : public QLabel {
public:
    using QLabel::QLabel;

    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QRect textRect() const;
    QString fittedText(bool *elided) const;
};
