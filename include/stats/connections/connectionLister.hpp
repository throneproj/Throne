#pragma once
#include <QMutex>
#include <QString>

namespace Stats
{
    constexpr int IDKEY = 242315;

    enum ConnectionSort
    {
        Default,
        ByDownload,
        ByUpload,
        ByProcess,
        ByTraffic,
        ByOutbound,
        ByProtocol
    };

    class ConnectionMetadata
    {
        public:
        QString id;
        long long createdAtMs;
        long long upload;
        long long download;
        QString outbound;
        QString network;
        QString dest;
        QString protocol;
        QString domain;
        QString process;
    };

    class ConnectionLister
    {
    public:
        ConnectionLister();

        bool suspend = true;

        void Loop();

        void ForceUpdate();

        void stopLoop();

        void setSort(ConnectionSort newSort);

    private:
        void update();

        QMutex mu;

        bool stop = false;

        std::shared_ptr<QSet<QString>> state;

        ConnectionSort sort = Default;

        bool asc = false;
    };

    extern ConnectionLister* connection_lister;
}
