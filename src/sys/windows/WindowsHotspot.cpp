#include "include/sys/WindowsHotspot.h"

#include <QCoreApplication>
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
        // Probe is read-only. Never kill a session operation that could still
        // turn the hotspot on; cancellation/parent watch owns its cleanup.
        m_probe.kill();
        update(false, false, false, tr("Windows did not answer the hotspot availability check."));
    });
    connect(&m_probe, &QProcess::readyReadStandardOutput, this, [this] { consume(&m_probe, m_probeBuffer, false); });
    connect(m_session.get(), &QProcess::readyReadStandardOutput, this, [this] { consume(m_session.get(), m_sessionBuffer, true); });
    connect(&m_probe, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
        consume(&m_probe, m_probeBuffer, false);
        m_probeDeadline.stop();
        if (!m_probeReply && m_session->state() == QProcess::NotRunning)
            update(false, false, false, tr("Could not check Windows mobile hotspot support."));
    });
    connect(&m_probe, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_probeDeadline.stop();
        if (m_session->state() == QProcess::NotRunning)
            update(false, false, false, tr("Could not run Windows PowerShell."));
    });
    connect(m_session.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        consume(m_session.get(), m_sessionBuffer, true);
        const auto error = m_sessionError;
        m_stopDirectory.reset();
        m_stopping = false;
        update(false, false, false, tr("Checking mobile hotspot availability…"));
        if (!m_quitting && (code != 0 || status != QProcess::NormalExit))
            emit problem(error.isEmpty() ? tr("The hotspot helper stopped unexpectedly. Check Windows mobile hotspot settings.") : error);
        if (!m_quitting) probe();
    });
    connect(m_session.get(), &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        update(false, false, false, tr("Could not run Windows PowerShell."));
        if (!m_quitting) emit problem(m_hint);
    });
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        m_quitting = true;
        m_poll.stop();
        stop();
    });
    update(false, false, false, tr("Enable TUN and connect a profile first."));
    m_poll.start();
}

WindowsHotspot::~WindowsHotspot() {
    m_quitting = true;
    m_poll.stop();
    m_probeDeadline.stop();
    disconnect(&m_probe, nullptr, this, nullptr);
    disconnect(m_session.get(), nullptr, this, nullptr);
    if (m_probe.state() != QProcess::NotRunning) {
        m_probe.kill();
        m_probe.waitForFinished(1000); // Read-only probe; bounded shutdown, no network operation.
    }
    stop();
    // Do not block the GUI or kill a still-pending WinRT operation. The helper
    // reads the cancellation marker and holds a handle to this parent, so it completes cleanup
    // even after the application exits. Leave its QProcess alive until then.
    if (m_session->state() != QProcess::NotRunning) {
        auto *session = m_session.release();
        disconnect(session, nullptr, this, nullptr);
        connect(session, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), session, &QObject::deleteLater);
    }
}

void WindowsHotspot::update(bool available, bool checked, bool busy, const QString &hint) {
    m_available = available;
    m_checked = checked;
    m_busy = busy;
    m_hint = hint;
    if (!m_quitting) emit changed();
}

void WindowsHotspot::setTunActive(bool active) {
    if (m_tunActive == active) return;
    m_tunActive = active;
    if (!active) {
        m_probe.kill();
        if (m_session->state() != QProcess::NotRunning) stop();
        else update(false, false, false, tr("Enable TUN and connect a profile first."));
    } else probe();
}

void WindowsHotspot::setEnabled(bool enabled) {
    if (enabled) {
        if (m_available && !m_busy && !m_checked && m_tunActive) start();
    } else stop();
}

QByteArray WindowsHotspot::command(const QString &action) const {
    // qCompress = 4-byte size + zlib header + raw DEFLATE + Adler32.
    // Embed the helper in memory, avoiding an elevated process executing a
    // mutable script from a user-writable temporary directory.
    const auto compressed = qCompress(m_helper, 9);
    const auto payload = compressed.mid(6, compressed.size() - 10).toBase64();
    const auto bootstrap = QStringLiteral(
        "$b=[Convert]::FromBase64String('%1');"
        "$s=New-Object IO.MemoryStream(,$b);"
        "$d=New-Object IO.Compression.DeflateStream($s,[IO.Compression.CompressionMode]::Decompress);"
        "$r=New-Object IO.StreamReader($d,[Text.Encoding]::UTF8);"
        "& ([scriptblock]::Create($r.ReadToEnd())) -Action %2 -ParentProcessId %3 -StopFile '%4'")
        .arg(QString::fromLatin1(payload), action, QString::number(QCoreApplication::applicationPid()),
             m_stopDirectory ? QString(m_stopDirectory->filePath(QStringLiteral("stop"))).replace('\'', QStringLiteral("''")) : QString());
    const auto utf16 = bootstrap.utf16();
    return QByteArray(reinterpret_cast<const char *>(utf16), bootstrap.size() * 2).toBase64();
}

void WindowsHotspot::launch(QProcess *process, const QString &action) {
    process->setProgram(qEnvironmentVariable("SystemRoot", "C:/Windows") + QStringLiteral("/System32/WindowsPowerShell/v1.0/powershell.exe"));
    process->setArguments({QStringLiteral("-NoLogo"), QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                           QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"), QStringLiteral("-EncodedCommand"),
                           QString::fromLatin1(command(action))});
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW; no console flashes for polling.
    });
    process->start();
}

void WindowsHotspot::probe() {
    if (m_quitting || !m_tunActive || m_probe.state() != QProcess::NotRunning || m_session->state() != QProcess::NotRunning) return;
    if (m_helper.isEmpty()) {
        update(false, false, false, tr("The hotspot helper is missing."));
        return;
    }
    m_probeBuffer.clear();
    m_probeReply = false;
    launch(&m_probe, QStringLiteral("Probe"));
    m_probeDeadline.start();
}

void WindowsHotspot::start() {
    if (m_session->state() != QProcess::NotRunning) return;
    m_probe.kill();
    m_probeDeadline.stop();
    m_sessionBuffer.clear();
    m_sessionError.clear();
    m_stopping = false;
    m_stopDirectory = std::make_shared<QTemporaryDir>();
    if (!m_stopDirectory->isValid()) {
        m_stopDirectory.reset();
        update(false, false, false, tr("Could not create the hotspot cancellation marker."));
        emit problem(m_hint);
        return;
    }
    // Retain the directory until the helper exits even if this controller is
    // destroyed. Only a marker is stored here; executable code stays embedded.
    connect(m_session.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished), m_session.get(),
            [directory = m_stopDirectory](int, QProcess::ExitStatus) {}, Qt::SingleShotConnection);
    update(false, true, true, tr("Starting mobile hotspot…"));
    launch(m_session.get(), QStringLiteral("Run"));
}

void WindowsHotspot::stop() {
    if (m_session->state() == QProcess::NotRunning || m_stopping) return;
    if (m_stopDirectory) {
        QFile marker(m_stopDirectory->filePath(QStringLiteral("stop")));
        if (!marker.open(QIODevice::WriteOnly)) {
            if (!m_quitting) {
                update(true, true, false, tr("Could not request hotspot shutdown. Turn it off in Windows settings."));
                emit problem(m_hint);
            }
            return;
        }
    }
    m_stopping = true;
    update(false, true, true, tr("Stopping mobile hotspot…"));
}

void WindowsHotspot::consume(QProcess *process, QByteArray &buffer, bool session) {
    buffer += process->readAllStandardOutput();
    // Cap unrecognised output while preserving complete protocol lines.
    if (buffer.size() > 65536) buffer = buffer.right(65536);
    int newline;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const auto line = buffer.left(newline).trimmed();
        buffer.remove(0, newline + 1);
        constexpr char prefix[] = "THRONE_HOTSPOT:";
        if (!line.startsWith(prefix)) continue;
        const auto json = QJsonDocument::fromJson(line.mid(sizeof(prefix) - 1));
        if (!json.isObject()) continue;
        const auto value = json.object();
        const auto event = value.value(QStringLiteral("event")).toString();
        if (!session && event == QLatin1String("probe")) {
            m_probeReply = true;
            if (m_tunActive && m_session->state() == QProcess::NotRunning)
                update(value.value(QStringLiteral("available")).toBool(), false, false,
                       value.value(QStringLiteral("reason")).toString());
        } else if (session && event == QLatin1String("ready")) {
            if (m_stopping || !m_tunActive || m_quitting) stop();
            else update(true, true, false, tr("Sharing through TUN. Network: %1").arg(value.value(QStringLiteral("ssid")).toString()));
        } else if (session && event == QLatin1String("error")) {
            m_sessionError = value.value(QStringLiteral("reason")).toString();
        }
    }
    process->readAllStandardError(); // PowerShell CLIXML is not the public protocol.
}
