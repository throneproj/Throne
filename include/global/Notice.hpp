#pragma once

#include <functional>
#include <QString>

enum class NoticeLevel {
    Info,
    Warning,
    Error,
};

struct Notice {
    NoticeLevel level = NoticeLevel::Warning;
    QString title;
    QString text;
};

// Thread-safe, never blocks; notices posted before a sink exists are buffered and replayed to it.
void PostNotice(const Notice &notice);

// The sink is invoked on the main thread.
void SetNoticeSink(std::function<void(const Notice &)> sink);
