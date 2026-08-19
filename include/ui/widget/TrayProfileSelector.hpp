#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <functional>
#include <utility>

#include "include/ui/widget/TrayPopupFrame.hpp"

class QLabel;
class QListWidgetItem;
class QPushButton;
class QTimer;

// A window, not a tray submenu: Linux SNI/DBusMenu and macOS NSMenu won't reliably expand one populated dynamically.
class TrayProfileSelector : public TrayPopupFrame {
    Q_OBJECT
public:
    enum Mode { Server, Routing };

    struct Callbacks {
        std::function<void(int id)> startProfile;
        std::function<void()>       stopProfile;
        std::function<void(int id)> chooseRoute;
        std::function<bool()>       isRunning;
        std::function<int()>        runningId;
        std::function<int()>        runningGid;
        std::function<QString()>    runningName;
    };

    TrayProfileSelector(Mode mode, Callbacks cb, QWidget *parent = nullptr);

protected:
    bool event(QEvent *e) override;                     // close when the panel loses activation
    bool eventFilter(QObject *obj, QEvent *e) override; // list/search key handling
    void preparePopup() override;
    void afterShow() override;

private:
    void rebuild();
    void activateItem(QListWidgetItem *it);
    void goBackToGroups();
    void changePage(int delta);
    void ensureServerCache();
    void ensureRouteCache();

    Mode m_mode;
    Callbacks m_cb;
    int m_groupId = -1;   // Server mode: -1 = group list; else the drilled-in group id
    int m_page = 0;
    bool m_armed = false;
    QString m_query;

    bool m_serverCacheBuilt = false;
    QList<std::pair<int, QString>> m_serverCache;
    QStringList m_serverCacheLower;
    bool m_routeCacheBuilt = false;
    QList<std::pair<int, QString>> m_routeCache;
    QStringList m_routeCacheLower;

    QTimer *m_debounce = nullptr;
    QPushButton *m_backBtn = nullptr;
    QLabel *m_title = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QLabel *m_pageLabel = nullptr;
    QPushButton *m_nextBtn = nullptr;
};
