#pragma once

#include <QTextDocument>
#include <QTextEdit>

// A QTextEdit that asks for a height measured in text lines.
//
// QTextEdit inherits QAbstractScrollArea's invented 256x192 size hint, which is
// unrelated to its content (these editors' real minimum is 58x58). Four of them
// on the Logging tab made that page 620px tall against 318-420 for every other
// page, and since a QTabWidget sizes to its tallest page, the whole Basic
// Settings dialog was padded out to match -- leaving the shorter tabs mostly
// empty. This only lowers the *preferred* height; the layout can still stretch
// these editors past it when the dialog is made taller.
class MyTextEdit : public QTextEdit {
public:
    explicit MyTextEdit(QWidget *parent = nullptr) : QTextEdit(parent) {
    }

    // Text lines to ask for. <= 0 restores QTextEdit's default hint.
    int visibleLines() const {
        return m_visibleLines;
    }

    void setVisibleLines(int lines) {
        m_visibleLines = lines;
        updateGeometry();
    }

    QSize sizeHint() const override {
        const QSize base = QTextEdit::sizeHint();
        if (m_visibleLines <= 0) return base;
        const QMargins m = contentsMargins();
        const int h = fontMetrics().lineSpacing() * m_visibleLines
                      + static_cast<int>(document()->documentMargin()) * 2
                      + m.top() + m.bottom()
                      + frameWidth() * 2;
        return {base.width(), h};
    }

private:
    int m_visibleLines = 5;
};
