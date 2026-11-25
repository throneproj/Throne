#pragma once

#include <QList>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <array>

namespace Qv2ray::ui {
    enum class LogLevel : quint8 { Debug, Info, Warn, Error };

    struct LogColors {
        QColor muted, info, warn, error;
        // One is picked per connection id, so every line of a connection shares it.
        QList<QColor> connections;
        QColor searchBackground, searchForeground;
    };

    // Reads only the sing-box, Xray or Throne prefix; Info when none matches.
    LogLevel ClassifyLogLine(const QString &line);

    class SyntaxHighlighter : public QSyntaxHighlighter {
        Q_OBJECT

    public:
        SyntaxHighlighter(const LogColors &colors, QTextDocument *parent);

        // Does not rehighlight; the caller re-renders the document after changing it.
        void setSearchPattern(const QRegularExpression &pattern);

    protected:
        void highlightBlock(const QString &text) override;

    private:
        QTextCharFormat mutedFormat;
        std::array<QTextCharFormat, 4> levelWordFormats;
        std::array<QTextCharFormat, 4> levelTextFormats;
        QList<QTextCharFormat> connectionFormats;
        QTextCharFormat warnTokenFormat, errorTokenFormat;
        QTextCharFormat searchFormat;
        QRegularExpression searchPattern;
    };
} // namespace Qv2ray::ui
