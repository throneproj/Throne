#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QTemporaryDir>
#include <memory>

// GUI-owned Windows hotspot session. The embedded PowerShell helper keeps its
// WinRT manager alive, watches this process/TUN, and stops only its own session.
class WindowsHotspot : public QObject {
    Q_OBJECT
public:
    explicit WindowsHotspot(QObject *parent = nullptr, const QByteArray &helperOverride = {});
    ~WindowsHotspot() override;
    void setTunActive(bool active);
    void setEnabled(bool enabled);
    bool available() const { return m_available; }
    bool checked() const { return m_checked; }
    bool busy() const { return m_busy; }
    QString hint() const { return m_hint; }

signals:
    void changed();
    void problem(const QString &message);

private:
    void probe();
    void start();
    void stop();
    void consume(QProcess *process, QByteArray &buffer, bool session);
    void launch(QProcess *process, const QString &action);
    QByteArray command(const QString &action) const;
    void update(bool available, bool checked, bool busy, const QString &hint);

    QProcess m_probe;
    std::unique_ptr<QProcess> m_session = std::make_unique<QProcess>();
    std::shared_ptr<QTemporaryDir> m_stopDirectory;
    QTimer m_poll;
    QTimer m_probeDeadline;
    QByteArray m_helper;
    QByteArray m_probeBuffer;
    QByteArray m_sessionBuffer;
    bool m_tunActive = false;
    bool m_available = false;
    bool m_checked = false;
    bool m_busy = false;
    bool m_stopping = false;
    bool m_quitting = false;
    bool m_probeReply = false;
    QString m_hint;
    QString m_sessionError;
};
