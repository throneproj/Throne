#pragma once
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>

class WindowsHotspot : public QObject {
    Q_OBJECT
public:
    explicit WindowsHotspot(QObject *parent = nullptr, const QByteArray &helperOverride = {});
    ~WindowsHotspot() override;
    void setTunActive(bool active);
    void setEnabled(bool enabled);
    // Main thread; nested event loop keeps the UI responsive. False means the
    // caller must keep the VPN/TUN alive. Headless callers have no controller.
    bool ensureStopped();
    bool available() const { return m_available; }
    bool checked() const { return m_checked; }
    bool busy() const { return m_busy; }
    QString hint() const { return m_hint; }
signals:
    void changed();
    void problem(const QString &message);
    void logLine(const QString &message);
private:
    void probe();
    void start();
    void stop();
    void finishStopped();
    void stopFailed(const QString &error);
    void consume(QProcess *process, QByteArray &buffer, const QString &action);
    void launch(QProcess *process, const QString &action);
    QByteArray command(const QString &action) const;
    void update(bool available, bool checked, bool busy, const QString &hint);
    bool active() const;
    QProcess m_probe;
    std::unique_ptr<QProcess> m_session = std::make_unique<QProcess>();
    std::unique_ptr<QProcess> m_stop = std::make_unique<QProcess>();
    std::shared_ptr<QTemporaryDir> m_stopDirectory;
    QTimer m_poll, m_probeDeadline;
    QByteArray m_helper, m_probeBuffer, m_sessionBuffer, m_stopBuffer;
    bool m_tunActive = false, m_available = false, m_checked = false, m_busy = false;
    bool m_stopping = false, m_quitting = false, m_probeReply = false;
    bool m_stopConfirmed = true, m_lastStopFailed = false, m_waiting = false;
    QString m_hint, m_sessionError, m_stopError;
};
