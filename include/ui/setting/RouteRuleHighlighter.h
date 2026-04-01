#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/// Подсвечивает строки-комментарии (начинающиеся с #) тёмно-зелёным курсивом
/// в текстовых полях правил маршрутизации
class RouteRuleHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit RouteRuleHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    QTextCharFormat commentFormat;
};
