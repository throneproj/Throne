#include "include/sys/WindowsHotspot.h"
#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

// Explicitly opt-in integration test: starts/stops the REAL saved Windows
// hotspot. It refuses an existing hotspot and needs an active elevated TUN.
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    WindowsHotspot hotspot;
    enum Phase { Probe, Starting, Running, Stopping, Finished } phase = Probe;
    QObject::connect(&hotspot, &WindowsHotspot::problem, &app, [&](const QString &error) {
        qCritical().noquote() << error;
        app.exit(1);
    });
    QObject::connect(&hotspot, &WindowsHotspot::changed, &app, [&] {
        if (phase == Probe && hotspot.available()) {
            phase = Starting;
            hotspot.setEnabled(true);
        } else if (phase == Starting && hotspot.checked() && !hotspot.busy()) {
            phase = Running;
            qInfo().noquote() << "REAL HOTSPOT READY:" << hotspot.hint();
            QTimer::singleShot(5000, &app, [&] { phase = Stopping; hotspot.setEnabled(false); });
        } else if (phase == Stopping && !hotspot.checked()) {
            phase = Finished;
            qInfo() << "REAL HOTSPOT STOPPED";
            app.quit();
        }
    });
    QTimer::singleShot(60000, &app, [&] {
        qCritical().noquote() << "Integration test timeout:" << hotspot.hint();
        app.exit(2);
    });
    hotspot.setTunActive(true);
    return app.exec();
}
