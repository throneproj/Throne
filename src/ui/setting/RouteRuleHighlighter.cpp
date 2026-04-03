#include "include/ui/setting/RouteRuleHighlighter.h"
#include <QRegularExpression>

RouteRuleHighlighter::RouteRuleHighlighter(QTextDocument *parent, bool showErrorHighlight)
    : QSyntaxHighlighter(parent), highlightErrors(showErrorHighlight) {
    // Настраиваем формат: тёмно-зелёный цвет + курсив для строк-комментариев
    commentFormat.setForeground(Qt::darkGreen);
    commentFormat.setFontItalic(true);

    // Настраиваем формат ошибок: красный цвет текста
    errorFormat.setForeground(Qt::red);
    errorFormat.setFontWeight(QFont::Bold);
}

void RouteRuleHighlighter::setErrorHighlightEnabled(bool enable)
{
    highlightErrors = enable;
    rehighlight(); // Перечитать весь документ
}

bool RouteRuleHighlighter::isErrorHighlightEnabled() const
{
    return highlightErrors;
}

bool RouteRuleHighlighter::isValidRule(const QString& text) const
{
    if (text.trimmed().isEmpty()) return true;
    if (text.trimmed().startsWith('#')) return true; // Комментарии всегда валидны

    // Проверяем известные префиксы
    static const QStringList validPrefixes = {
        "domain:", "suffix:", "keyword:", "regex:",
        "ip:", "processName:", "processPath:",
        "ruleset:geoip", "ruleset:geosite",
        "ruleset:http://", "ruleset:https://", "ruleset:file:",
        "ruleset:changelog" // для правил вида ruleset:https://...
    };

    // Проверяем начинается ли с валидного префикса
    for (const auto& prefix : validPrefixes) {
        if (text.trimmed().startsWith(prefix, Qt::CaseInsensitive)) {
            return true;
        }
    }

    // Проверяем если строка содержит / или похожа на CIDR (для IP без префикса)
    // или домен без префикса (содержит точку)
    QString trimmed = text.trimmed();
    if (trimmed.contains('/') && !trimmed.contains("://")) {
        // Может быть CIDR без префикса - валидно для IP поля
        return true;
    }

    // Если домен без префикса
    if (trimmed.contains('.') && !trimmed.contains(':') && !trimmed.contains('/')) {
        return true;
    }

    // Если имя процесса .exe
    if (trimmed.endsWith(".exe", Qt::CaseInsensitive)) {
        return true;
    }

    // Если ничего не подошло - это ошибка
    return false;
}

void RouteRuleHighlighter::highlightBlock(const QString &text) {
    // Если строка начинается с # — применяем формат комментария ко всей строке
    if (text.trimmed().startsWith('#')) {
        setFormat(0, text.length(), commentFormat);
        return;
    }

    // Если включена подсветка ошибок - проверяем валидность
    if (highlightErrors && !text.trimmed().isEmpty()) {
        if (!isValidRule(text)) {
            // Подсвечиваем всю строку красным
            setFormat(0, text.length(), errorFormat);
        }
    }
}
