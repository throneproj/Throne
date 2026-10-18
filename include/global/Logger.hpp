#pragma once

#include <QString>
#include <QStringList>

// Nothing here may call qDebug()/qWarning(): the installed Qt message handler feeds back into Write().
namespace Logging {
    enum class Level {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Fatal = 5,
        Off = 6,
    };

    Level LevelFromString(const QString &name);

    QString LevelToString(Level level);

    void InstallQtMessageHandler();

    // Mirrors lines at or above minLevel to stderr (user-log lines count as Info); works before Init(). Fatal always goes there.
    // The sink takes lines by its own level, independent of the file level.
    void EnableStdSink(Level minLevel);

    enum class StdFormat {
        // journald "<N>" priority prefixes only when JOURNAL_STREAM matches stderr.
        Auto,
        Journal,
        Plain,
    };

    // Any thread.
    void SetStdFormat(StdFormat format);

    // Messages logged before this are buffered in memory and replayed.
    void Init(const QString &baseDir);

    // Clears the running marker; anything that misses this is reported as a crash next start.
    void Shutdown();

    void SetLevel(Level level);

    Level GetLevel();

    bool IsEnabled(Level level);

    void Write(Level level, const QString &message, const char *file = nullptr, int line = 0);

    void WriteUserLog(const QString &message);

    constexpr int MAX_RECENT_LINES = 400;

    // Reading the ring allocates nothing, so a crash handler can walk it directly.
    int RecentLinesRaw(const QByteArray **out, int max);

    QStringList RecentLines(int max = 0);

    // Gives up rather than blocking if the faulting thread holds the writer lock.
    void FlushForCrash();

    bool PreviousSessionCrashed();

    QString PreviousSessionLogPath();

    QString LogDir();

    QString CrashDir();

#ifdef Q_OS_WIN
    // Filled during Init() so an exception filter never has to build a path.
    const wchar_t *CrashDirNative();
#endif
} // namespace Logging

#define LOG_TRACE(msg) ::Logging::Write(::Logging::Level::Trace, (msg), __FILE__, __LINE__)
#define LOG_DEBUG(msg) ::Logging::Write(::Logging::Level::Debug, (msg), __FILE__, __LINE__)
#define LOG_INFO(msg)  ::Logging::Write(::Logging::Level::Info,  (msg), __FILE__, __LINE__)
#define LOG_WARN(msg)  ::Logging::Write(::Logging::Level::Warn,  (msg), __FILE__, __LINE__)
#define LOG_ERROR(msg) ::Logging::Write(::Logging::Level::Error, (msg), __FILE__, __LINE__)
#define LOG_FATAL(msg) ::Logging::Write(::Logging::Level::Fatal, (msg), __FILE__, __LINE__)
