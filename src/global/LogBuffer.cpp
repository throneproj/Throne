#include "include/global/LogBuffer.hpp"

#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

#include <algorithm>
#include <deque>
#include <utility>

namespace Logging::LogBufferImpl {
    constexpr size_t kMaxLines = 5000;
    // UTF-16 bytes of text held.
    constexpr qsizetype kMaxBytes = 4 * 1024 * 1024;
    constexpr qsizetype kMaxLineLength = 8192;

    struct Ring {
        QMutex mutex;
        // Held seqs are contiguous: lines[i].seq == lines.front().seq + i.
        std::deque<BufferedLine> lines;
        qsizetype bytes = 0;
        quint64 lastSeq = 0;
    };

    // Leaked: static destructors may still log after main() returns.
    Ring &ring() {
        static auto *const instance = new Ring;
        return *instance;
    }

    qsizetype textBytes(const QString &text) {
        return text.size() * qsizetype(sizeof(QChar));
    }

    QString clipped(QStringView line) {
        if (line.size() <= kMaxLineLength) return line.toString();
        qsizetype keep = kMaxLineLength - 1;
        if (line.at(keep - 1).isHighSurrogate()) --keep;
        return line.left(keep).toString() + QChar(0x2026);
    }

    // The caller holds ring.mutex.
    BufferedSlice slice(const Ring &ring, size_t first, int limit) {
        BufferedSlice out;
        out.newestSeq = ring.lastSeq;
        if (!ring.lines.empty()) out.oldestSeq = ring.lines.front().seq;
        const size_t end = std::min(ring.lines.size(), first + size_t(std::max(limit, 0)));
        for (size_t i = first; i < end; ++i) out.lines.append(ring.lines[i]);
        out.more = end < ring.lines.size();
        return out;
    }
}

namespace Logging {
    void BufferUserLog(const QString &chunk) {
        QStringList texts;
        for (auto line : QStringView(chunk).split(u'\n')) {
            if (line.endsWith(u'\r')) line.chop(1);
            if (!line.isEmpty()) texts.append(LogBufferImpl::clipped(line));
        }
        if (texts.isEmpty()) return;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        auto &ring = LogBufferImpl::ring();
        QMutexLocker lock(&ring.mutex);
        for (auto &text : texts) {
            ring.bytes += LogBufferImpl::textBytes(text);
            ring.lines.push_back(BufferedLine{++ring.lastSeq, now, std::move(text)});
            while (ring.lines.size() > LogBufferImpl::kMaxLines || ring.bytes > LogBufferImpl::kMaxBytes) {
                ring.bytes -= LogBufferImpl::textBytes(ring.lines.front().text);
                ring.lines.pop_front();
            }
        }
    }

    BufferedSlice BufferedLinesAfter(quint64 after, int limit) {
        auto &ring = LogBufferImpl::ring();
        QMutexLocker lock(&ring.mutex);
        if (ring.lines.empty() || after >= ring.lastSeq) return LogBufferImpl::slice(ring, ring.lines.size(), limit);
        const quint64 oldest = ring.lines.front().seq;
        return LogBufferImpl::slice(ring, after < oldest ? 0 : size_t(after - oldest + 1), limit);
    }

    BufferedSlice BufferedTail(int limit) {
        auto &ring = LogBufferImpl::ring();
        QMutexLocker lock(&ring.mutex);
        const size_t count = std::min(ring.lines.size(), size_t(std::max(limit, 0)));
        return LogBufferImpl::slice(ring, ring.lines.size() - count, limit);
    }

    quint64 BufferedNewestSeq() {
        auto &ring = LogBufferImpl::ring();
        QMutexLocker lock(&ring.mutex);
        return ring.lastSeq;
    }
} // namespace Logging
