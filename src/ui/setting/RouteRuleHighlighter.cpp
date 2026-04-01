#include "include/ui/setting/RouteRuleHighlighter.h"

RouteRuleHighlighter::RouteRuleHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
    // Настраиваем формат: тёмно-зелёный цвет + курсив для строк-комментариев
    commentFormat.setForeground(Qt::darkGreen);
    commentFormat.setFontItalic(true);
}

void RouteRuleHighlighter::highlightBlock(const QString &text) {
    // Если строка начинается с # — применяем формат комментария ко всей строке
    if (text.trimmed().startsWith('#')) {
        setFormat(0, text.length(), commentFormat);
    }
}
