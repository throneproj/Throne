#include "include/sys/WindowsHotspot.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

WindowsHotspot::WindowsHotspot(QObject *parent, const QByteArray &helperOverride) : QObject(parent), m_helper(helperOverride) {
    if (m_helper.isEmpty()) {
        QFile resource(QStringLiteral(":/windows/windows_hotspot_ui.ps1"));
        if (resource.open(QIODevice::ReadOnly)) m_helper = resource.readAll();
    }
    m_poll.setInterval(10000);
    m_probeDeadline.setSingleShot(true);
    m_probeDeadline.setInterval(20000);
    connect(&m_poll, &QTimer::timeout, this, &WindowsHotspot::probe);
    connect(&m_probeDeadline, &QTimer::timeout, this, [this] {
        m_probe.kill();
        if (!active()) update(false, false, false, tr("Windows did not answer the hotspot availability check."));
    });
    connect(&m_probe, &QProcess::readyReadStandardOutput, this, [this] { consume(&m_probe, m_probeBuffer, QStringLiteral("Probe")); });
    connect(m_session.get(), &QProcess::readyReadStandardOutput, this, [this] { consume(m_session.get(), m_sessionBuffer, QStringLiteral("Run")); });
    connect(m_stop.get(), &QProcess::readyReadStandardOutput, this, [this] { consume(m_stop.get(), m_stopBuffer, QStringLiteral("Stop")); });
    connect(&m_probe, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
        consume(&m_probe, m_probeBuffer, QStringLiteral("Probe"));
        m_probeDeadline.stop();
        if (!m_probeReply && !active()) update(false, false, false, tr("Could not check Windows mobile hotspot support."));
    });
    connect(&m_probe, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_probeDeadline.stop();
        if (!active()) update(false, false, false, tr("Could not run Windows PowerShell."));
    });
    connect(m_session.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        consume(m_session.get(), m_sessionBuffer, QStringLiteral("Run"));
        if (m_stopConfirmed) {
            const bool report = !m_stopping && !m_sessionError.isEmpty() && !m_quitting;
            const auto error = m_sessionError;
            finishStopped();
            if (report) { emit logLine(tr("<<<<<<<< HOT Spot error: %1").arg(error)); emit problem(error); }
            return;
        }
        if (!m_stopping) {
            const auto error = m_sessionError.isEmpty() ? tr("The hotspot helper stopped unexpectedly. Check Windows mobile hotspot settings.") : m_sessionError;
            emit logLine(tr("<<<<<<<< HOT Spot error: %1").arg(error));
            if (!m_quitting && (code != 0 || status != QProcess::NormalExit)) emit problem(error);
            stop(); // Recover an owned session whose monitor exited without confirming Off.
        }
    });
    connect(m_stop.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
        consume(m_stop.get(), m_stopBuffer, QStringLiteral("Stop"));
        if (m_stopConfirmed) finishStopped();
        else stopFailed(m_stopError.isEmpty() ? tr("Windows did not confirm that HOT Spot is off. The VPN remains enabled.") : m_stopError);
    });
    connect(m_session.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        m_stopConfirmed = true; // The start worker never ran.
        finishStopped();
        if (!m_quitting) emit problem(tr("Could not run Windows PowerShell."));
    });
    connect(m_stop.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) stopFailed(tr("Could not run Windows PowerShell."));
    });
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { m_quitting = true; m_poll.stop(); stop(); });
    update(false, false, false, tr("Enable TUN and connect a profile first."));
    m_poll.start();
}

WindowsHotspot::~WindowsHotspot() {
    m_quitting = true;
    m_poll.stop(); m_probeDeadline.stop();
    disconnect(&m_probe, nullptr, this, nullptr);
    if (m_probe.state() != QProcess::NotRunning) { m_probe.kill(); m_probe.waitForFinished(1000); }
    stop();
    for (auto *holder : {&m_session, &m_stop}) {
        disconnect(holder->get(), nullptr, this, nullptr);
        if ((*holder)->state() != QProcess::NotRunning) {
            auto *worker = holder->release();
            connect(worker, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), worker, &QObject::deleteLater);
        }
    }
}

bool WindowsHotspot::active() const {
    return m_checked || m_session->state() != QProcess::NotRunning || m_stop->state() != QProcess::NotRunning;
}
void WindowsHotspot::update(bool available, bool checked, bool busy, const QString &hint) {
    m_available = available; m_checked = checked; m_busy = busy; m_hint = hint;
    if (!m_quitting) emit changed();
}
void WindowsHotspot::setTunActive(bool activeTun) {
    if (m_tunActive == activeTun) return;
    m_tunActive = activeTun;
    if (!activeTun) {
        m_probe.kill(); m_probeDeadline.stop();
        if (active()) stop();
        else update(false, false, false, tr("Enable TUN and connect a profile first."));
    } else probe();
}
void WindowsHotspot::setEnabled(bool enabled) {
    if (enabled) { if (m_available && !m_busy && !m_checked && m_tunActive) start(); }
    else stop();
}

bool WindowsHotspot::ensureStopped() {
    if (!active()) return true;
    if (m_waiting) return false;
    m_waiting = true;
    emit logLine(tr(">>>>>>>> HOT Spot: stop before disabling VPN"));
    stop();
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &WindowsHotspot::changed, &loop, [this, &loop] {
        if ((!active() && m_stopConfirmed) || m_lastStopFailed) loop.quit();
    });
    deadline.start(60000);
    if (active() && !m_lastStopFailed) loop.exec();
    m_waiting = false;
    if (!active() && m_stopConfirmed) return true;
    emit logLine(tr("<<<<<<<< VPN kept enabled: HOT Spot shutdown was not confirmed"));
    if (!m_lastStopFailed && !m_quitting) emit problem(tr("Windows did not confirm that HOT Spot is off. The VPN remains enabled."));
    return false;
}

QByteArray WindowsHotspot::command(const QString &action) const {
    const auto compressed = qCompress(m_helper, 9);
    const auto payload = compressed.mid(6, compressed.size() - 10).toBase64();
    const auto bootstrap = QStringLiteral(
        "$b=[Convert]::FromBase64String('%1');$s=New-Object IO.MemoryStream(,$b);"
        "$d=New-Object IO.Compression.DeflateStream($s,[IO.Compression.CompressionMode]::Decompress);"
        "$r=New-Object IO.StreamReader($d,[Text.Encoding]::UTF8);"
        "& ([scriptblock]::Create($r.ReadToEnd())) -Action %2 -ParentProcessId %3 -StopFile '%4'")
        .arg(QString::fromLatin1(payload), action, QString::number(QCoreApplication::applicationPid()),
             m_stopDirectory ? QString(m_stopDirectory->filePath(QStringLiteral("stop"))).replace('\'', QStringLiteral("''")) : QString());
    return QByteArray(reinterpret_cast<const char *>(bootstrap.utf16()), bootstrap.size() * 2).toBase64();
}
void WindowsHotspot::launch(QProcess *process, const QString &action) {
    process->setProgram(qEnvironmentVariable("SystemRoot", "C:/Windows") + QStringLiteral("/System32/WindowsPowerShell/v1.0/powershell.exe"));
    process->setArguments({QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                           QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-EncodedCommand"), QString::fromLatin1(command(action))});
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) { args->flags |= 0x08000000; });
    if (action != QLatin1String("Probe"))
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
                [directory = m_stopDirectory](int, QProcess::ExitStatus) {}, Qt::SingleShotConnection);
    process->start();
}
void WindowsHotspot::probe() {
    if (m_quitting || !m_tunActive || m_probe.state() != QProcess::NotRunning || active()) return;
    if (m_helper.isEmpty()) { update(false, false, false, tr("The hotspot helper is missing.")); return; }
    m_probeBuffer.clear(); m_probeReply = false;
    launch(&m_probe, QStringLiteral("Probe")); m_probeDeadline.start();
}
void WindowsHotspot::start() {
    if (active()) return;
    m_probe.kill(); m_probeDeadline.stop();
    m_sessionBuffer.clear(); m_sessionError.clear();
    m_stopping = false; m_lastStopFailed = false; m_stopConfirmed = false;
    m_stopDirectory = std::make_shared<QTemporaryDir>();
    if (!m_stopDirectory->isValid()) {
        m_stopConfirmed = true; m_stopDirectory.reset();
        update(false, false, false, tr("Could not create the hotspot cancellation marker.")); emit problem(m_hint); return;
    }
    emit logLine(tr(">>>>>>>> Starting HOT Spot"));
    update(false, true, true, tr("Starting mobile hotspot…"));
    launch(m_session.get(), QStringLiteral("Run"));
}
void WindowsHotspot::stop() {
    if (!active() || m_stopping) return;
    if (!m_stopDirectory) { stopFailed(tr("Could not request hotspot shutdown. Turn it off in Windows settings.")); return; }
    QFile marker(m_stopDirectory->filePath(QStringLiteral("stop")));
    if (!marker.open(QIODevice::WriteOnly)) { stopFailed(tr("Could not request hotspot shutdown. Turn it off in Windows settings.")); return; }
    marker.close();
    m_stopping = true; m_lastStopFailed = false; m_stopConfirmed = false;
    m_stopBuffer.clear(); m_stopError.clear();
    emit logLine(tr(">>>>>>>> Stopping HOT Spot"));
    update(false, true, true, tr("Stopping mobile hotspot…"));
    launch(m_stop.get(), QStringLiteral("Stop"));
}
void WindowsHotspot::finishStopped() {
    if (!m_stopConfirmed || m_session->state() != QProcess::NotRunning || m_stop->state() != QProcess::NotRunning) return;
    const bool wasActive = m_checked || m_stopping;
    m_stopping = false; m_lastStopFailed = false; m_stopDirectory.reset();
    update(false, false, false, m_tunActive ? tr("Checking mobile hotspot availability…") : tr("Enable TUN and connect a profile first."));
    if (wasActive) emit logLine(tr("<<<<<<<< HOT Spot stopped"));
    if (!m_quitting) probe();
}
void WindowsHotspot::stopFailed(const QString &error) {
    m_stopping = false; m_lastStopFailed = true;
    emit logLine(tr("<<<<<<<< HOT Spot error: %1").arg(error));
    update(true, true, false, error);
    if (!m_quitting) emit problem(error);
}
void WindowsHotspot::consume(QProcess *process, QByteArray &buffer, const QString &action) {
    buffer += process->readAllStandardOutput();
    if (buffer.size() > 65536) buffer = buffer.right(65536);
    int newline;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const auto line = buffer.left(newline).trimmed(); buffer.remove(0, newline + 1);
        constexpr char prefix[] = "THRONE_HOTSPOT:";
        if (!line.startsWith(prefix)) continue;
        const auto json = QJsonDocument::fromJson(line.mid(sizeof(prefix) - 1));
        if (!json.isObject()) continue;
        const auto value = json.object();
        const auto event = value.value(QStringLiteral("event")).toString();
        if (action == QLatin1String("Probe") && event == QLatin1String("probe")) {
            m_probeReply = true;
            if (m_tunActive && !active()) update(value.value(QStringLiteral("available")).toBool(), false, false, value.value(QStringLiteral("reason")).toString());
        } else if (event == QLatin1String("ready")) {
            if (m_stopping || !m_tunActive || m_quitting) stop();
            else {
                emit logLine(tr(">>>>>>>> HOT Spot started"));
                update(true, true, false, tr("Sharing through TUN. Network: %1").arg(value.value(QStringLiteral("ssid")).toString()));
            }
        } else if (event == QLatin1String("stopped")) {
            m_stopConfirmed = true;
        } else if (event == QLatin1String("error")) {
            if (action == QLatin1String("Stop")) m_stopError = value.value(QStringLiteral("reason")).toString();
            else {
                m_sessionError = value.value(QStringLiteral("reason")).toString();
                if (value.value(QStringLiteral("off")).toBool()) {
                    m_stopConfirmed = true;
                }
            }
        }
    }
    process->readAllStandardError();
}
