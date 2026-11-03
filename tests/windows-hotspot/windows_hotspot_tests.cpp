#include "include/sys/WindowsHotspot.h"
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class WindowsHotspotTests : public QObject {
    Q_OBJECT
    QByteArray helper(const QByteArray &run, bool available = true) {
        return "param([string]$Action,[int]$ParentProcessId,[string]$StopFile)\n"
               "[Console]::OutputEncoding=New-Object Text.UTF8Encoding($false)\n"
               "function Send($s){[Console]::WriteLine('THRONE_HOTSPOT:'+$s);[Console]::Out.Flush()}\n"
               "if($Action -eq 'Probe'){Send '{\"event\":\"probe\",\"available\":" +
               (available ? QByteArray("true") : QByteArray("false")) +
               ",\"reason\":\"test capability\"}';exit 0}\n" + run;
    }
    const QByteArray normal = "Send '{\"event\":\"ready\",\"ssid\":\"Test Wi-Fi\"}'\n"
                              "while(-not [IO.File]::Exists($StopFile)){Start-Sleep -Milliseconds 25}\nSend '{\"event\":\"stopped\"}'\n";
private slots:
    void disabledWithoutTun() {
        WindowsHotspot hotspot(nullptr, helper(normal));
        QVERIFY(!hotspot.available());
        hotspot.setEnabled(true);
        QVERIFY(!hotspot.checked());
        QVERIFY(!hotspot.busy());
    }
    void capabilityGatesStartup() {
        WindowsHotspot hotspot(nullptr, helper(normal, false));
        QSignalSpy changed(&hotspot, &WindowsHotspot::changed);
        hotspot.setTunActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(changed.count() > 0, 15000);
        QVERIFY(!hotspot.available());
        hotspot.setEnabled(true);
        QVERIFY(!hotspot.checked());
    }
    void startAndStop() {
        WindowsHotspot hotspot(nullptr, helper(normal));
        QSignalSpy problem(&hotspot, &WindowsHotspot::problem);
        hotspot.setTunActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(hotspot.available(), 15000);
        hotspot.setEnabled(true);
        QVERIFY(hotspot.busy());
        hotspot.setEnabled(true); // A double click must not create another session.
        QTRY_VERIFY_WITH_TIMEOUT(hotspot.checked() && !hotspot.busy(), 15000);
        QVERIFY(hotspot.hint().contains(QStringLiteral("Test Wi-Fi")));
        hotspot.setEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(!hotspot.checked(), 15000);
        QCOMPARE(problem.count(), 0);
    }
    void failedStartupUnchecks() {
        WindowsHotspot hotspot(nullptr, helper("Send '{\"event\":\"error\",\"reason\":\"adapter unavailable\"}'\nexit 1\n"));
        QSignalSpy problem(&hotspot, &WindowsHotspot::problem);
        hotspot.setTunActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(hotspot.available(), 15000);
        hotspot.setEnabled(true);
        QTRY_COMPARE_WITH_TIMEOUT(problem.count(), 1, 15000);
        QVERIFY(!hotspot.checked());
        QCOMPARE(problem[0][0].toString(), QStringLiteral("adapter unavailable"));
    }
    void tunStopsPendingStartup() {
        WindowsHotspot hotspot(nullptr, helper("Start-Sleep -Milliseconds 900\n" + normal));
        hotspot.setTunActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(hotspot.available(), 15000);
        bool presentedReady = false;
        connect(&hotspot, &WindowsHotspot::changed, this, [&] {
            if (hotspot.checked() && !hotspot.busy()) presentedReady = true;
        });
        hotspot.setEnabled(true);
        hotspot.setTunActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(!hotspot.checked(), 15000);
        QVERIFY(!presentedReady);
        QVERIFY(!hotspot.available());
    }
    void destructorLetsHelperCleanUp() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto marker = dir.filePath(QStringLiteral("stopped.txt"));
        auto escaped = marker;
        escaped.replace('\\', '/');
        escaped.replace('\'', QStringLiteral("''"));
        const auto body = normal + "[IO.File]::WriteAllText('" + escaped.toUtf8() + "','stopped')\n";
        auto *hotspot = new WindowsHotspot(nullptr, helper(body));
        hotspot->setTunActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(hotspot->available(), 15000);
        hotspot->setEnabled(true);
        QTRY_VERIFY_WITH_TIMEOUT(hotspot->checked() && !hotspot->busy(), 15000);
        delete hotspot;
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(marker), 15000);
    }
};

QTEST_GUILESS_MAIN(WindowsHotspotTests)
#include "windows_hotspot_tests.moc"
