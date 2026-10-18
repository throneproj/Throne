#include "include/global/Notice.hpp"
#include "include/global/Logger.hpp"

#include <QCoreApplication>

#include <deque>
#include <memory>
#include <mutex>
#include <utility>

namespace {
    using NoticeSink = std::function<void(const Notice &)>;

    constexpr size_t kMaxPendingNotices = 32;

    std::mutex g_noticeMu;
    std::shared_ptr<const NoticeSink> g_noticeSink;
    std::deque<Notice> g_pendingNotices;

    void logNotice(const Notice &notice) {
        const QString line = QString("notice: %1: %2").arg(notice.title, notice.text);
        switch (notice.level) {
            case NoticeLevel::Info: LOG_INFO(line); break;
            case NoticeLevel::Warning: LOG_WARN(line); break;
            case NoticeLevel::Error: LOG_ERROR(line); break;
        }
    }

    // Caller holds g_noticeMu.
    void bufferLocked(const Notice &notice) {
        if (g_pendingNotices.size() >= kMaxPendingNotices) g_pendingNotices.pop_front();
        g_pendingNotices.push_back(notice);
    }

    // Caller holds g_noticeMu, so posts and a replay keep their order. The sink is resolved at delivery and called unlocked.
    void postLocked(QCoreApplication *app, const Notice &notice) {
        QMetaObject::invokeMethod(app, [notice] {
            std::shared_ptr<const NoticeSink> sink;
            {
                std::lock_guard lock(g_noticeMu);
                sink = g_noticeSink;
                if (!sink) {
                    bufferLocked(notice);
                    return;
                }
            }
            (*sink)(notice);
        }, Qt::QueuedConnection);
    }
}

void PostNotice(const Notice &notice) {
    logNotice(notice);
    std::lock_guard lock(g_noticeMu);
    auto *app = QCoreApplication::instance();
    if (g_noticeSink && app != nullptr) {
        postLocked(app, notice);
    } else {
        bufferLocked(notice);
    }
}

void SetNoticeSink(std::function<void(const Notice &)> sink) {
    auto next = sink ? std::make_shared<const NoticeSink>(std::move(sink)) : nullptr;
    std::shared_ptr<const NoticeSink> previous; // outlives the lock, so the old sink is never destroyed under it
    std::lock_guard lock(g_noticeMu);
    previous = std::exchange(g_noticeSink, std::move(next));
    auto *app = QCoreApplication::instance();
    if (!g_noticeSink || app == nullptr) return;
    while (!g_pendingNotices.empty()) {
        postLocked(app, g_pendingNotices.front());
        g_pendingNotices.pop_front();
    }
}
