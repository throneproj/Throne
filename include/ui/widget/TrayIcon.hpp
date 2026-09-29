#pragma once

#include <QIcon>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

// Native NSStatusItem on macOS: QSystemTrayIcon in Qt 6.11.2 and older aborts on macOS 27 (fixed for Qt 6.12.0).
class TrayIcon : public QObject {
    Q_OBJECT
public:
    explicit TrayIcon(QObject *parent = nullptr);
    ~TrayIcon() override;

    void setIcon(const QIcon &icon);
    void setToolTip(const QString &text);
    void setContextMenu(QMenu *menu);
    void setVisible(bool visible);
    void hide() { setVisible(false); }
    bool isVisible() const;

signals:
    void activated(QSystemTrayIcon::ActivationReason reason);

private:
#ifdef Q_OS_MAC
    void *m_statusItem = nullptr; // NSStatusItem, kept opaque so this header stays C++
    bool m_visible = false;
#else
    QSystemTrayIcon *m_tray;
#endif
};
