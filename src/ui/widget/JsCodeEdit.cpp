#include "include/ui/widget/JsCodeEdit.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QSet>
#include <QSyntaxHighlighter>
#include <QTextBlock>

namespace {
    constexpr int kJsCodeEditGutterPad = 12;

    const QSet<QString> &jsCodeEditKeywords() {
        static const QSet<QString> words = {
            QStringLiteral("async"),    QStringLiteral("await"),     QStringLiteral("break"),
            QStringLiteral("case"),     QStringLiteral("catch"),     QStringLiteral("class"),
            QStringLiteral("const"),    QStringLiteral("continue"),  QStringLiteral("default"),
            QStringLiteral("delete"),   QStringLiteral("do"),        QStringLiteral("else"),
            QStringLiteral("export"),   QStringLiteral("extends"),   QStringLiteral("false"),
            QStringLiteral("finally"),  QStringLiteral("for"),       QStringLiteral("function"),
            QStringLiteral("if"),       QStringLiteral("import"),    QStringLiteral("in"),
            QStringLiteral("instanceof"), QStringLiteral("let"),     QStringLiteral("new"),
            QStringLiteral("null"),     QStringLiteral("of"),        QStringLiteral("return"),
            QStringLiteral("super"),    QStringLiteral("switch"),    QStringLiteral("this"),
            QStringLiteral("throw"),    QStringLiteral("true"),      QStringLiteral("try"),
            QStringLiteral("typeof"),   QStringLiteral("undefined"), QStringLiteral("var"),
            QStringLiteral("void"),     QStringLiteral("while"),     QStringLiteral("yield"),
        };
        return words;
    }

    const QSet<QString> &jsCodeEditBuiltins() {
        static const QSet<QString> words = {
            QStringLiteral("fetch"),   QStringLiteral("http"),    QStringLiteral("tcpPing"),
            QStringLiteral("sleep"),   QStringLiteral("ctx"),     QStringLiteral("console"),
            QStringLiteral("atob"),    QStringLiteral("btoa"),    QStringLiteral("JSON"),
            QStringLiteral("Math"),    QStringLiteral("Date"),    QStringLiteral("Promise"),
            QStringLiteral("RegExp"),  QStringLiteral("Array"),   QStringLiteral("Object"),
            QStringLiteral("String"),  QStringLiteral("Number"),  QStringLiteral("Error"),
        };
        return words;
    }

    class JsCodeEditHighlighter final : public QSyntaxHighlighter {
    public:
        explicit JsCodeEditHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}

        void setDark(const bool dark) {
            m_keyword.setForeground(dark ? QColor(0xc7, 0x92, 0xea) : QColor(0x70, 0x48, 0xe8));
            m_builtin.setForeground(dark ? QColor(0x66, 0xd9, 0xe8) : QColor(0x0b, 0x72, 0x85));
            m_string.setForeground(dark ? QColor(0x8c, 0xe9, 0x9a) : QColor(0x2b, 0x8a, 0x3e));
            m_number.setForeground(dark ? QColor(0xff, 0xc0, 0x78) : QColor(0xe8, 0x59, 0x0c));
            m_comment.setForeground(QColor(0x86, 0x8e, 0x96));
            m_comment.setFontItalic(true);
            rehighlight();
        }

    protected:
        void highlightBlock(const QString &text) override {
            int i = 0;
            if (previousBlockState() == 1) {
                const int end = text.indexOf(QStringLiteral("*/"));
                if (end < 0) {
                    setFormat(0, static_cast<int>(text.size()), m_comment);
                    setCurrentBlockState(1);
                    return;
                }
                setFormat(0, end + 2, m_comment);
                i = end + 2;
            }
            setCurrentBlockState(0);

            const auto &keywords = jsCodeEditKeywords();
            const auto &builtins = jsCodeEditBuiltins();

            while (i < text.size()) {
                const QChar c = text.at(i);
                if (c == QLatin1Char('/') && i + 1 < text.size()) {
                    if (text.at(i + 1) == QLatin1Char('/')) {
                        setFormat(i, static_cast<int>(text.size()) - i, m_comment);
                        return;
                    }
                    if (text.at(i + 1) == QLatin1Char('*')) {
                        const int end = text.indexOf(QStringLiteral("*/"), i + 2);
                        if (end < 0) {
                            setFormat(i, static_cast<int>(text.size()) - i, m_comment);
                            setCurrentBlockState(1);
                            return;
                        }
                        setFormat(i, end + 2 - i, m_comment);
                        i = end + 2;
                        continue;
                    }
                }
                if (c == QLatin1Char('"') || c == QLatin1Char('\'') || c == QLatin1Char('`')) {
                    const QChar quote = c;
                    const int start = i;
                    ++i;
                    while (i < text.size()) {
                        if (text.at(i) == QLatin1Char('\\')) {
                            i += 2;
                            continue;
                        }
                        if (text.at(i) == quote) break;
                        ++i;
                    }
                    i = qMin(i + 1, static_cast<int>(text.size()));
                    setFormat(start, i - start, m_string);
                    continue;
                }
                if (c.isDigit()) {
                    const int start = i;
                    ++i;
                    while (i < text.size() && (text.at(i).isLetterOrNumber() || text.at(i) == QLatin1Char('.'))) {
                        ++i;
                    }
                    setFormat(start, i - start, m_number);
                    continue;
                }
                if (c.isLetter() || c == QLatin1Char('_') || c == QLatin1Char('$')) {
                    const int start = i;
                    ++i;
                    while (i < text.size() && (text.at(i).isLetterOrNumber() || text.at(i) == QLatin1Char('_') || text.at(i) == QLatin1Char('$'))) {
                        ++i;
                    }
                    const QString word = text.mid(start, i - start);
                    if (keywords.contains(word)) setFormat(start, i - start, m_keyword);
                    else if (builtins.contains(word)) setFormat(start, i - start, m_builtin);
                    continue;
                }
                ++i;
            }
        }

    private:
        QTextCharFormat m_keyword;
        QTextCharFormat m_builtin;
        QTextCharFormat m_string;
        QTextCharFormat m_number;
        QTextCharFormat m_comment;
    };

    class JsCodeEditGutter final : public QWidget {
    public:
        explicit JsCodeEditGutter(JsCodeEdit *editor) : QWidget(editor), m_editor(editor) {}

        [[nodiscard]] QSize sizeHint() const override { return {m_editor->gutterWidth(), 0}; }

    protected:
        void paintEvent(QPaintEvent *event) override { m_editor->paintGutter(event); }

    private:
        JsCodeEdit *m_editor;
    };
} // namespace

JsCodeEdit::JsCodeEdit(QWidget *parent) : QPlainTextEdit(parent) {
    setLineWrapMode(NoWrap);
    m_gutter = new JsCodeEditGutter(this);
    m_highlighter = new JsCodeEditHighlighter(document());

    connect(this, &QPlainTextEdit::blockCountChanged, this, [this] {
        setViewportMargins(gutterWidth(), 0, 0, 0);
    });
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &rect, const int dy) {
        if (dy != 0) {
            m_gutter->scroll(0, dy);
        } else {
            m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());
        }
        if (rect.contains(viewport()->rect())) setViewportMargins(gutterWidth(), 0, 0, 0);
    });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &JsCodeEdit::refreshDecorations);

    setViewportMargins(gutterWidth(), 0, 0, 0);
    applyTheme();
    refreshDecorations();
}

void JsCodeEdit::applyMonospaceFont(const QFont &font) {
    setFont(font);
    if (m_gutter != nullptr) m_gutter->setFont(font);
    setTabStopDistance(2 * QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')));
    setViewportMargins(gutterWidth(), 0, 0, 0);
    if (m_gutter != nullptr) {
        const QRect area = contentsRect();
        m_gutter->setGeometry(QRect(area.left(), area.top(), gutterWidth(), area.height()));
        m_gutter->update();
    }
}

int JsCodeEdit::gutterWidth() const {
    int digits = 2;
    for (int lines = qMax(1, blockCount()); lines >= 100; lines /= 10) ++digits;
    return kJsCodeEditGutterPad + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void JsCodeEdit::paintGutter(QPaintEvent *event) {
    QPainter painter(m_gutter);
    painter.setFont(font());
    painter.fillRect(event->rect(), palette().alternateBase());

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    qreal bottom = top + blockBoundingRect(block).height();

    const QColor numberColor = palette().color(QPalette::Disabled, QPalette::WindowText);
    painter.setPen(numberColor);
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.drawText(0, static_cast<int>(top), m_gutter->width() - 6, fontMetrics().height(),
                             Qt::AlignRight | Qt::AlignVCenter, QString::number(blockNumber + 1));
        }
        block = block.next();
        top = bottom;
        bottom = top + blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void JsCodeEdit::keyPressEvent(QKeyEvent *event) {
    if (!isReadOnly() && handleAutoEdit(event)) return;
    QPlainTextEdit::keyPressEvent(event);
}

void JsCodeEdit::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    const QRect area = contentsRect();
    m_gutter->setGeometry(QRect(area.left(), area.top(), gutterWidth(), area.height()));
}

void JsCodeEdit::changeEvent(QEvent *event) {
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) applyTheme();
}

void JsCodeEdit::refreshDecorations() {
    QTextEdit::ExtraSelection currentLine;
    currentLine.format.setBackground(palette().alternateBase());
    currentLine.format.setProperty(QTextFormat::FullWidthSelection, true);
    currentLine.cursor = textCursor();
    currentLine.cursor.clearSelection();
    setExtraSelections({currentLine});
    m_gutter->update();
}

void JsCodeEdit::applyTheme() {
    static_cast<JsCodeEditHighlighter *>(m_highlighter)->setDark(palette().base().color().lightness() < 128);
}

QChar JsCodeEdit::charBeforeCursor() const {
    const QTextCursor cursor = textCursor();
    if (cursor.positionInBlock() == 0) return {};
    return cursor.block().text().at(cursor.positionInBlock() - 1);
}

QChar JsCodeEdit::charAfterCursor() const {
    const QTextCursor cursor = textCursor();
    const QString text = cursor.block().text();
    if (cursor.positionInBlock() >= text.length()) return {};
    return text.at(cursor.positionInBlock());
}

bool JsCodeEdit::handleAutoEdit(QKeyEvent *event) {
    const int key = event->key();
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (key == Qt::Key_Tab && modifiers == Qt::NoModifier) {
        insertPlainText(QStringLiteral("  "));
        return true;
    }

    const bool plainOrShift = (modifiers & ~Qt::ShiftModifier) == 0;
    if ((key == Qt::Key_Return || key == Qt::Key_Enter) && plainOrShift) {
        QTextCursor cursor = textCursor();
        if (cursor.hasSelection()) return false;
        QString indent;
        for (const QChar c : cursor.block().text()) {
            if (c == QLatin1Char(' ') || c == QLatin1Char('\t')) indent += c;
            else break;
        }
        const QChar before = charBeforeCursor();
        const QChar after = charAfterCursor();
        const bool pair = (before == QLatin1Char('{') && after == QLatin1Char('}')) ||
                          (before == QLatin1Char('[') && after == QLatin1Char(']')) ||
                          (before == QLatin1Char('(') && after == QLatin1Char(')'));
        const bool opens = before == QLatin1Char('{') || before == QLatin1Char('[') || before == QLatin1Char('(');
        cursor.beginEditBlock();
        if (pair) {
            cursor.insertText(QStringLiteral("\n") + indent + QStringLiteral("  ") + QStringLiteral("\n") + indent);
            cursor.movePosition(QTextCursor::Up);
            cursor.movePosition(QTextCursor::EndOfBlock);
        } else if (opens) {
            cursor.insertText(QStringLiteral("\n") + indent + QStringLiteral("  "));
        } else {
            cursor.insertText(QStringLiteral("\n") + indent);
        }
        cursor.endEditBlock();
        setTextCursor(cursor);
        return true;
    }

    if (key == Qt::Key_Backspace && modifiers == Qt::NoModifier) {
        const QChar before = charBeforeCursor();
        const QChar after = charAfterCursor();
        if ((before == QLatin1Char('{') && after == QLatin1Char('}')) ||
            (before == QLatin1Char('[') && after == QLatin1Char(']')) ||
            (before == QLatin1Char('(') && after == QLatin1Char(')')) ||
            (before == QLatin1Char('"') && after == QLatin1Char('"')) ||
            (before == QLatin1Char('\'') && after == QLatin1Char('\''))) {
            QTextCursor cursor = textCursor();
            cursor.deletePreviousChar();
            cursor.deleteChar();
            setTextCursor(cursor);
            return true;
        }
        return false;
    }

    const QString typedText = event->text();
    if (typedText.isEmpty()) return false;
    const QChar typed = typedText.at(0);

    if ((typed == QLatin1Char('}') || typed == QLatin1Char(']') || typed == QLatin1Char(')') ||
         typed == QLatin1Char('"') || typed == QLatin1Char('\'')) &&
        charAfterCursor() == typed) {
        QTextCursor cursor = textCursor();
        cursor.movePosition(QTextCursor::Right);
        setTextCursor(cursor);
        return true;
    }

    if (typed == QLatin1Char('{') || typed == QLatin1Char('[') || typed == QLatin1Char('(') ||
        typed == QLatin1Char('"') || typed == QLatin1Char('\'')) {
        if (textCursor().hasSelection()) return false;
        if ((typed == QLatin1Char('"') || typed == QLatin1Char('\'')) && charAfterCursor().isLetterOrNumber()) return false;
        QChar close = typed;
        if (typed == QLatin1Char('{')) close = QLatin1Char('}');
        else if (typed == QLatin1Char('[')) close = QLatin1Char(']');
        else if (typed == QLatin1Char('(')) close = QLatin1Char(')');
        QTextCursor cursor = textCursor();
        cursor.insertText(QString(typed) + close);
        cursor.movePosition(QTextCursor::Left);
        setTextCursor(cursor);
        return true;
    }

    return false;
}
