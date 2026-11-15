#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <array>

namespace Qv2ray::ui {
    enum class LogLevel : quint8 { Debug, Info, Warn, Error };

    struct LogColors {
        QColor text, muted, info, warn, error, ok, host;
        QColor inbound, outbound, dns, router, other;
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
        QTextCharFormat inboundFormat, outboundFormat, dnsFormat, routerFormat, appFormat, otherFormat;
        QTextCharFormat hostPortFormat, hostFormat, protocolFormat;
        QTextCharFormat okFormat, warnTokenFormat, errorTokenFormat, infoTokenFormat;
        QTextCharFormat searchFormat;
        QRegularExpression searchPattern;
    };
} // namespace Qv2ray::ui
