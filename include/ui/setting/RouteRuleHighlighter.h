#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/// Подсвечивает строки-комментарии (начинающиеся с #) тёмно-зелёным курсивом
/// и ошибки синтаксиса красным цветом
/// в текстовых полях правил маршрутизации
class RouteRuleHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit RouteRuleHighlighter(QTextDocument *parent = nullptr, bool showErrorHighlight = false);

    // Включить/выключить подсветку ошибок
    void setErrorHighlightEnabled(bool enable);
    bool isErrorHighlightEnabled() const;

protected:
    void highlightBlock(const QString &text) override;

private:
    QTextCharFormat commentFormat;
    QTextCharFormat errorFormat;
    bool highlightErrors = false;

    bool isValidRule(const QString& text) const;
};
