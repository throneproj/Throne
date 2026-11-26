#pragma once

#include <QFont>
#include <QPlainTextEdit>

class QSyntaxHighlighter;

class JsCodeEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit JsCodeEdit(QWidget *parent = nullptr);

    void applyMonospaceFont(const QFont &font);

    [[nodiscard]] int gutterWidth() const;
    void paintGutter(QPaintEvent *event);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void refreshDecorations();
    void applyTheme();
    bool handleAutoEdit(QKeyEvent *event);
    [[nodiscard]] QChar charBeforeCursor() const;
    [[nodiscard]] QChar charAfterCursor() const;

    QWidget *m_gutter = nullptr;
    QSyntaxHighlighter *m_highlighter = nullptr;
};
