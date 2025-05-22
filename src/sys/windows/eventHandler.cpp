#include "include/sys/windows/eventHandler.h"

#include <QDebug>
#include <windows.h>

bool PowerOffTaskkillFilter::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);

        if (msg->message == WM_QUERYENDSESSION) {
            qDebug() << "WM_QUERYENDSESSION received";
            *result = TRUE;
            return true;
        } else if (msg->message == WM_ENDSESSION) {
            if (msg->wParam)
            {
                qDebug() << "WM_ENDSESSION received, calling cleanUpFunc";
                cleanUpFunc(0);
                return true;
            }
        }
    }
    return false;
}