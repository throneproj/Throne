#pragma once

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <functional>

class PowerOffTaskkillFilter : public QAbstractNativeEventFilter
{
public:
    PowerOffTaskkillFilter(const std::function<void(int)>& f)
    {
        cleanUpFunc = f;
    };

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
private:
    std::function<void(int)> cleanUpFunc;
};