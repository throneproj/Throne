#include "include/ui/mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMutexLocker>
#include <QPainter>
#include <QScrollBar>
#include <QTextCursor>

#include <algorithm>

#include "3rdparty/qv2ray/v2/ui/LogHighlighter.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/utils/ThemedIcon.hpp"

using Qv2ray::ui::LogLevel;

namespace {
    constexpr quint8 ALL_LOG_LEVELS = 0x0F;
    constexpr qsizetype MAX_PENDING_LOG_CHARS = 2 * 1024 * 1024;
    // Slack so a fractional layout height still counts as scrolled to the bottom.
    constexpr int LOG_BOTTOM_SLACK = 4;
    constexpr int LOG_SEARCH_DEBOUNCE_MS = 150;

    int logLineLimit() {
        const int limit = Configs::dataManager->settingsRepo->max_log_line;
        return limit > 0 ? limit : 500;
    }

    // Every log line must stay exactly one block: the view maps visible m_logLines to blocks by count.
    void appendDocumentLines(QTextDocument *doc, const QString &lines) {
        if (lines.isEmpty()) return;
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::End);
        cursor.beginEditBlock();
        if (!doc->isEmpty()) cursor.insertBlock();
        cursor.insertText(lines);
        cursor.endEditBlock();
    }

    void removeLeadingBlocks(QTextDocument *doc, int count) {
        if (count <= 0) return;
        if (count >= doc->blockCount()) {
            doc->clear();
            return;
        }
        QTextCursor cursor(doc);
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor, count);
        cursor.removeSelectedText();
    }

    quint8 levelBit(LogLevel level) {
        return static_cast<quint8>(1u << static_cast<int>(level));
    }

    QColor mixColors(const QColor &a, const QColor &b, float amount) {
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * amount, a.greenF() + (b.greenF() - a.greenF()) * amount,
                                a.blueF() + (b.blueF() - a.blueF()) * amount);
    }

    // Level colours read calmer desaturated and drawn toward the text colour.
    QColor calmLogColor(const QColor &color, const QColor &text, float maxSaturation = 0.38f) {
        float h = 0, s = 0, l = 0, a = 1;
        color.getHslF(&h, &s, &l, &a);
        return mixColors(QColor::fromHslF(h, std::min(s, maxSaturation), l, a), text, 0.2f);
    }

    // Debug, Info, Warn, Error.
    std::array<QColor, 4> logLevelColors(const ThemeTokens &tokens) {
        return {tokens.muted, calmLogColor(tokens.info, tokens.onSurface), calmLogColor(tokens.warning, tokens.onSurface, 0.55f),
                calmLogColor(tokens.danger, tokens.onSurface, 0.55f)};
    }

    QPixmap levelDotPixmap(const QColor &color, int size, qreal dpr) {
        QPixmap pixmap(QSize(size, size) * dpr);
        pixmap.setDevicePixelRatio(dpr);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QRectF(1, 1, size - 2, size - 2));
        return pixmap;
    }

    QIcon levelDotIcon(const QColor &color) {
        QColor faded = color;
        faded.setAlphaF(0.35f);
        QIcon icon;
        for (const qreal dpr : {1.0, 2.0}) {
            icon.addPixmap(levelDotPixmap(color, 10, dpr), QIcon::Normal, QIcon::On);
            icon.addPixmap(levelDotPixmap(faded, 10, dpr), QIcon::Normal, QIcon::Off);
        }
        return icon;
    }
}

void MainWindow::applyLogBrowserFont() {
    const auto &settings = Configs::dataManager->settingsRepo;
    // The whole list, not just the resolved family, so glyphs missing from the first font still fall back in order.
    QFont logFont;
    logFont.setFamilies(LogFontFamilies(settings->log_font_family));
    logFont.setStyleHint(QFont::Monospace);
    logFont.setFixedPitch(true);
    int pt = settings->log_font_size;
    if (pt <= 0) pt = qApp->font().pointSize();
    if (pt <= 0) pt = settings->font_size;
    if (pt > 0) logFont.setPointSize(pt);
    ui->masterLogBrowser->setFont(logFont);
}

void MainWindow::setLogHighlighter(bool darkMode) {
    const auto &tokens = themeManager()->tokens;
    const auto levels = logLevelColors(tokens);
    Qv2ray::ui::LogColors colors;
    colors.text = tokens.onSurface;
    colors.muted = levels[0];
    colors.info = levels[1];
    colors.warn = levels[2];
    colors.error = levels[3];
    colors.ok = tokens.success;
    colors.host = mixColors(tokens.info, tokens.onSurface, 0.35f);
    colors.inbound = tokens.chartSeries[2];
    colors.outbound = tokens.chartSeries[4];
    colors.dns = tokens.chartSeries[1];
    colors.router = tokens.chartSeries[3];
    colors.other = tokens.chartContext;
    colors.searchBackground = darkMode ? QColor(150, 110, 0) : QColor(255, 225, 90);
    colors.searchForeground = darkMode ? QColor(Qt::white) : QColor(Qt::black);

    // A QSyntaxHighlighter is never evicted by constructing another, so the old one must be deleted.
    delete logHighlighter;
    logHighlighter = new Qv2ray::ui::SyntaxHighlighter(colors, qvLogDocument);
    logHighlighter->setSearchPattern(m_logSearch);
    refreshLogLevelChips();
}

void MainWindow::refreshLogLevelChips() {
    const auto dots = logLevelColors(themeManager()->tokens);
    for (int i = 0; i < 4; ++i) {
        if (logLevelChips[i] != nullptr) logLevelChips[i]->setIcon(levelDotIcon(dots[i]));
    }
}

void MainWindow::pushLogLine(QString &&text) {
    const auto level = Qv2ray::ui::ClassifyLogLine(text);
    ++m_logLevelCounts[static_cast<int>(level)];
    LogLine line{std::move(text), level, true};
    line.visible = logLineVisible(line);
    m_logLines.push_back(std::move(line));
}

bool MainWindow::logLineVisible(const LogLine &line) const {
    if ((m_logLevelFilter & levelBit(line.level)) == 0) return false;
    return m_logSearch.pattern().isEmpty() || m_logSearch.match(line.text).hasMatch();
}

void MainWindow::setupLogView() {
    qvLogDocument->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setDocument(qvLogDocument);
    applyLogBrowserFont();

    // Follow mode tracks where the user leaves the scrollbar; content changes only snap it back down.
    auto bar = ui->masterLogBrowser->verticalScrollBar();
    connect(bar, &QScrollBar::valueChanged, this, [this, bar](int value) {
        const bool atBottom = value >= bar->maximum() - LOG_BOTTOM_SLACK;
        if (atBottom == m_logFollow) return;
        m_logFollow = atBottom;
        if (m_logFollow && !m_logHeld.empty()) {
            // Deferred: merging relayouts the document while the scrollbar is still mid-update.
            QTimer::singleShot(0, this, [this] {
                if (m_logFollow) releaseHeldLogs();
            });
        }
        updateLogStatus();
    });
    // The document lays out lazily, so the range keeps growing after an append.
    connect(bar, &QScrollBar::rangeChanged, this, [this, bar](int, int max) {
        if (m_logFollow) bar->setValue(max);
    });

    // Both sit in the stats tab corner, which setupConnectionFilter() builds.
    logJumpLatestButton = new QToolButton(this);
    logJumpLatestButton->setToolTip(tr("Scroll to the newest logs and keep following them."));
    connect(logJumpLatestButton, &QToolButton::clicked, this, [this] { releaseHeldLogs(); });
    logFilterButton = new QToolButton(this);
    logFilterButton->setIcon(ThemedIcon(QStringLiteral("funnel")));
    logFilterButton->setCheckable(true);
    connect(logFilterButton, &QToolButton::toggled, this, &MainWindow::setLogFilterVisible);

    int chipIndex = 0;
    for (const auto level : {LogLevel::Error, LogLevel::Warn, LogLevel::Info, LogLevel::Debug}) {
        auto *chip = new QToolButton(ui->logToolbar);
        chip->setCheckable(true);
        chip->setChecked(true);
        chip->setAutoRaise(true);
        chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        connect(chip, &QToolButton::toggled, this, [this, level](bool shown) {
            m_logLevelChoice = shown ? (m_logLevelChoice | levelBit(level)) : (m_logLevelChoice & ~levelBit(level));
            applyLogSearch();
        });
        ui->logToolbarLayout->insertWidget(chipIndex++, chip);
        logLevelChips[static_cast<int>(level)] = chip;
    }
    ui->logToolbarLayout->insertSpacing(chipIndex, 8);
    refreshLogLevelChips();
    for (auto *w : ui->logToolbar->findChildren<QWidget*>()) w->installEventFilter(this);

    m_logSearchDebounce = new QTimer(this);
    m_logSearchDebounce->setSingleShot(true);
    m_logSearchDebounce->setInterval(LOG_SEARCH_DEBOUNCE_MS);
    connect(m_logSearchDebounce, &QTimer::timeout, this, [this] { applyLogSearch(); });
    connect(ui->logSearchEdit, &QLineEdit::textChanged, m_logSearchDebounce, qOverload<>(&QTimer::start));
    connect(ui->logSearchCase, &QToolButton::toggled, this, [this] { applyLogSearch(); });
    connect(ui->logSearchRegex, &QToolButton::toggled, this, [this] { applyLogSearch(); });
    setLogFilterVisible(false);
    updateLogStatus();
}

void MainWindow::append_log(const QString &log) {
    if (log.size() > 20000) {
        append_log(QString("TRUNCATED LONG LOG: ") + log.first(1000) + "...");
        return;
    }
    QMutexLocker locker(&logMutex);
    if (logQueue.size() > 1000) {
        return;
    }
    logQueue.enqueue(log);
    if (logQueue.size() == 1) logWaiter.wakeOne();
}

void MainWindow::log_process_loop() {
    while (true) {
        logMutex.lock();
        while (logQueue.isEmpty()) {
            logWaiter.wait(&logMutex);
        }
        QQueue<QString> pending;
        pending.swap(logQueue);
        const LogFilter filter{
            Configs::dataManager->settingsRepo->log_enable_include,
            Configs::dataManager->settingsRepo->log_enable_exclude,
            includeKeywords, excludeKeywords, includeCombined, excludeCombined,
        };
        logMutex.unlock();

        QString batchToPrint;
        for (const auto& entry : pending) {
            for (auto logLine : entry.split('\n')) {
                if (logLine.endsWith('\r')) logLine.chop(1);
                if (!should_print_log(logLine, filter)) continue;
                // QTextCursor::insertText starts a new block at these; keep each log line a single block.
                for (QChar &c : logLine) {
                    if (c == '\r' || c == QChar::ParagraphSeparator || c == QChar(0xfdd0) || c == QChar(0xfdd1)) c = ' ';
                }
                if (!batchToPrint.isEmpty()) batchToPrint += '\n';
                batchToPrint += logLine;
            }
        }
        if (batchToPrint.isEmpty()) continue;

        bool needsPost;
        {
            QMutexLocker pendingLocker(&logPendingMutex);
            if (!logPendingText.isEmpty()) logPendingText += '\n';
            logPendingText += batchToPrint;
            if (logPendingText.size() > MAX_PENDING_LOG_CHARS) {
                const auto cut = logPendingText.indexOf('\n', logPendingText.size() - MAX_PENDING_LOG_CHARS);
                logPendingText = cut < 0 ? QString() : logPendingText.mid(cut + 1);
            }
            needsPost = !logFlushScheduled;
            logFlushScheduled = true;
        }
        if (needsPost) runOnUiThread([this] { flush_log_batch(); });
    }
}

void MainWindow::flush_log_batch() {
    QString batch;
    {
        QMutexLocker pendingLocker(&logPendingMutex);
        batch.swap(logPendingText);
        logFlushScheduled = false;
    }
    if (batch.isEmpty()) return;

    // Everything passes through the hold queue; while the user is scrolled up it only accumulates
    // (keeping the newest max_log_line) and the view stays untouched.
    auto lines = batch.split('\n', Qt::SkipEmptyParts);
    for (auto &line : lines) m_logHeld.push_back(std::move(line));
    const auto limit = static_cast<size_t>(logLineLimit());
    while (m_logHeld.size() > limit) m_logHeld.pop_front();

    if (m_logFollow) {
        releaseHeldLogs();
    } else {
        updateLogStatus();
    }
}

void MainWindow::releaseHeldLogs() {
    // Set first so the range growth from the append below already snaps to the bottom.
    m_logFollow = true;
    if (!m_logHeld.empty()) {
        QString shown;
        for (auto &text : m_logHeld) {
            pushLogLine(std::move(text));
            const auto &line = m_logLines.back();
            if (line.visible) {
                if (!shown.isEmpty()) shown += '\n';
                shown += line.text;
            }
        }
        m_logHeld.clear();
        appendDocumentLines(qvLogDocument, shown);
        removeLeadingBlocks(qvLogDocument, trimLogLines());
    }
    auto bar = ui->masterLogBrowser->verticalScrollBar();
    bar->setValue(bar->maximum());
    updateLogStatus();
}

int MainWindow::trimLogLines() {
    const auto limit = static_cast<size_t>(logLineLimit());
    int droppedVisible = 0;
    while (m_logLines.size() > limit) {
        const auto &front = m_logLines.front();
        if (front.visible) ++droppedVisible;
        --m_logLevelCounts[static_cast<int>(front.level)];
        m_logLines.pop_front();
    }
    while (m_logHeld.size() > limit) m_logHeld.pop_front();
    return droppedVisible;
}

void MainWindow::rebuildLogView() {
    QString shown;
    for (auto &line : m_logLines) {
        line.visible = logLineVisible(line);
        if (line.visible) {
            if (!shown.isEmpty()) shown += '\n';
            shown += line.text;
        }
    }
    m_logFollow = true;
    qvLogDocument->setPlainText(shown);
    auto bar = ui->masterLogBrowser->verticalScrollBar();
    bar->setValue(bar->maximum());
    updateLogStatus();
}

void MainWindow::applyLogSearch() {
    // A hidden bar keeps its text and toggles but stops filtering.
    const bool barShown = logFilterButton->isChecked();
    const QString text = barShown ? ui->logSearchEdit->text() : QString();
    QRegularExpression search;
    if (!text.isEmpty()) {
        search.setPattern(ui->logSearchRegex->isChecked() ? text : QRegularExpression::escape(text));
        if (!ui->logSearchCase->isChecked()) search.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        if (!search.isValid()) {
            // Keep the last valid filter rather than blanking the view while a pattern is half-typed.
            ui->logSearchEdit->setStyleSheet(QStringLiteral("QLineEdit { color: #e05252; }"));
            ui->logSearchEdit->setToolTip(search.errorString());
            return;
        }
        search.optimize();
    }
    ui->logSearchEdit->setStyleSheet({});
    ui->logSearchEdit->setToolTip({});
    const quint8 levels = barShown ? m_logLevelChoice : ALL_LOG_LEVELS;
    if (search == m_logSearch && levels == m_logLevelFilter) return;

    m_logSearch = search;
    m_logLevelFilter = levels;
    if (logHighlighter) logHighlighter->setSearchPattern(m_logSearch);
    // A new filter jumps to the newest results, so fold in whatever was held.
    for (auto &held : m_logHeld) pushLogLine(std::move(held));
    m_logHeld.clear();
    trimLogLines();
    rebuildLogView();
}

void MainWindow::setLogFilterVisible(bool visible) {
    ui->logToolbar->setVisible(visible);
    logFilterButton->setToolTip(visible ? tr("Disable Filter") : tr("Enable Filter"));
    if (visible) ui->logSearchEdit->setFocus(Qt::OtherFocusReason);
    m_logSearchDebounce->stop();
    applyLogSearch();
}

void MainWindow::updateLogStatus() {
    if (m_logSearch.pattern().isEmpty() && m_logLevelFilter == ALL_LOG_LEVELS) {
        ui->logMatchCount->clear();
    } else {
        const auto matched = std::count_if(m_logLines.begin(), m_logLines.end(), [](const LogLine &line) { return line.visible; });
        ui->logMatchCount->setText(QStringLiteral("%1 / %2").arg(matched).arg(m_logLines.size()));
    }
    const std::array<QString, 4> chipTexts = {
        tr("Debug %1").arg(m_logLevelCounts[static_cast<int>(LogLevel::Debug)]),
        tr("Info %1").arg(m_logLevelCounts[static_cast<int>(LogLevel::Info)]),
        tr("Warnings %1").arg(m_logLevelCounts[static_cast<int>(LogLevel::Warn)]),
        tr("Errors %1").arg(m_logLevelCounts[static_cast<int>(LogLevel::Error)]),
    };
    for (int i = 0; i < 4; ++i) {
        if (logLevelChips[i] != nullptr) logLevelChips[i]->setText(chipTexts[i]);
    }
    logJumpLatestButton->setVisible(!m_logFollow);
    logJumpLatestButton->setText(m_logHeld.empty() ? tr("Jump to latest") : tr("Jump to latest (%1 new)").arg(m_logHeld.size()));
}

bool MainWindow::should_print_log(const QString &log, const LogFilter &filter) {
    if (QStringView(log).trimmed().isEmpty()) return false;
    bool result = true;
    if (filter.enableInclude) {
        result = false;
        for (const auto& includeKeyword : filter.includeKeywords) {
            if (log.contains(includeKeyword)) {
                result = true;
                break;
            }
        }
        if (!result && !filter.includeCombined.pattern().isEmpty() && filter.includeCombined.match(log).hasMatch()) {
            result = true;
        }
    }
    if (result && filter.enableExclude) {
        for (const auto& excludeKeyword : filter.excludeKeywords) {
            if (log.contains(excludeKeyword)) {
                result = false;
                break;
            }
        }
        if (result && !filter.excludeCombined.pattern().isEmpty() && filter.excludeCombined.match(log).hasMatch()) {
            result = false;
        }
    }
    return result;
}

void MainWindow::on_masterLogBrowser_customContextMenuRequested(const QPoint &pos) {
    QMenu *menu = ui->masterLogBrowser->createStandardContextMenu();

    auto sep = new QAction(this);
    sep->setSeparator(true);
    menu->addAction(sep);

    auto action_clear = new QAction(this);
    action_clear->setText(tr("Clear"));
    connect(action_clear, &QAction::triggered, this, [=,this] {
        {
            // Otherwise a flush already in flight repaints what was just cleared.
            QMutexLocker pendingLocker(&logPendingMutex);
            logPendingText.clear();
        }
        m_logLines.clear();
        m_logHeld.clear();
        m_logLevelCounts.fill(0);
        qvLogDocument->clear();
        m_logFollow = true;
        updateLogStatus();
    });
    menu->addAction(action_clear);

    menu->exec(ui->masterLogBrowser->viewport()->mapToGlobal(pos));
}
