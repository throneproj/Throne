#include <QThread>
#include <core/server/gen/libcore.pb.h>
#include <include/api/gRPC.h>
#include "include/ui/mainwindow_interface.h"
#include <include/stats/connections/connectionLister.hpp>

namespace NekoGui_traffic
{
    ConnectionLister* connection_lister = new ConnectionLister();

    ConnectionLister::ConnectionLister()
    {
        state = std::make_shared<QSet<QString>>();
    }

    void ConnectionLister::ForceUpdate()
    {
        mu.lock();
        update();
        mu.unlock();
    }


    void ConnectionLister::Loop()
    {
        while (true)
        {
            if (stop) return;
            QThread::msleep(1000);

            if (suspend || !NekoGui::dataStore->enable_stats) continue;

            mu.lock();
            update();
            mu.unlock();
        }
    }

    void ConnectionLister::update()
    {
        bool ok;
            libcore::ListConnectionsResp resp = NekoGui_rpc::defaultClient->ListConnections(&ok);
            if (!ok)
            {
                return;
            }

            QMap<QString, ConnectionMetadata> toUpdate;
            QMap<QString, ConnectionMetadata> toAdd;
            QSet<QString> newState;
            QList<ConnectionMetadata> sorted;
            auto conns = resp.connections();
            for (auto conn : conns)
            {
                auto c = ConnectionMetadata();
                c.id = QString(conn.mutable_id()->c_str());
                c.createdAtMs = conn.created_at();
                c.dest = QString(conn.mutable_dest()->c_str());
                c.upload = conn.upload();
                c.download = conn.download();
                c.domain = QString(conn.mutable_domain()->c_str());
                c.network = QString(conn.mutable_network()->c_str());
                c.outbound = QString(conn.mutable_outbound()->c_str());
                c.process = QString(conn.mutable_process()->c_str());
                c.protocol = QString(conn.mutable_protocol()->c_str());
                if (sort == Default)
                {
                    if (state->contains(c.id))
                    {
                        toUpdate[c.id] = c;
                    } else
                    {
                        toAdd[c.id] = c;
                    }
                } else
                {
                    sorted.append(c);
                }
                newState.insert(c.id);
            }

            state->clear();
            for (const auto& id : newState) state->insert(id);

            if (sort == Default)
            {
                runOnUiThread([=] {
                    auto m = GetMainWindow();
                    m->UpdateConnectionList(toUpdate, toAdd);
                });
            } else
            {
                if (sort == ByDownload)
                {
                    std::sort(sorted.begin(), sorted.end(), [=](const ConnectionMetadata& a, const ConnectionMetadata& b)
                    {
                        if (a.download == b.download) return asc ? a.id > b.id : a.id < b.id;
                        return asc ? a.download < b.download : a.download > b.download;
                    });
                }
                if (sort == ByUpload)
                {
                    std::sort(sorted.begin(), sorted.end(), [=](const ConnectionMetadata& a, const ConnectionMetadata& b)
                    {
                       if (a.upload == b.upload) return asc ? a.id > b.id : a.id < b.id;
                       return asc ? a.upload < b.upload : a.upload > b.upload;
                    });
                }
                if (sort == ByProcess)
                {
                    std::sort(sorted.begin(), sorted.end(), [=](const ConnectionMetadata& a, const ConnectionMetadata& b)
                    {
                        if (a.process == b.process) return asc ? a.id > b.id : a.id < b.id;
                        return asc ? a.process > b.process : a.process < b.process;
                    });
                }
                runOnUiThread([=] {
                    auto m = GetMainWindow();
                    m->UpdateConnectionListWithRecreate(sorted);
                });
            }
    }

    void ConnectionLister::stopLoop()
    {
        stop = true;
    }

    void ConnectionLister::setSort(const ConnectionSort newSort)
    {
        if (sort == newSort) asc = !asc;
        else
        {
            sort = newSort;
            asc = false;
        }
    }

}
