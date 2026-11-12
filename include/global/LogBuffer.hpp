#pragma once

#include <QList>
#include <QString>

// The lines of the user log (MW_show_log) kept in memory for the Throne API's GET /v1/logs. Any thread, also before main().
namespace Logging {
    struct BufferedLine {
        quint64 seq = 0;
        qint64 time = 0;
        QString text;
    };

    struct BufferedSlice {
        QList<BufferedLine> lines;
        // 0 before the first line, and oldestSeq 0 while nothing is held.
        quint64 newestSeq = 0;
        quint64 oldestSeq = 0;
        // Held lines follow the last one in lines.
        bool more = false;
    };

    void BufferUserLog(const QString &chunk);

    // Oldest first, at most limit lines with a seq above after.
    BufferedSlice BufferedLinesAfter(quint64 after, int limit);

    BufferedSlice BufferedTail(int limit);

    quint64 BufferedNewestSeq();
} // namespace Logging
