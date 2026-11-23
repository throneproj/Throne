#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Engine::TorBridges {
    // Main thread, once from EngineHost::create(): hands BuiltinLines to Configs::tor for Build().
    void Install();

    // Any thread, IPC only: the built-in lines of a type from the newer of the core's copy and the downloaded one; empty
    // while the core cannot answer.
    QStringList BuiltinLines(const QString &type);

    // Any thread: the downloaded bridges-v1.json when it is newer than the core's copy, else empty.
    QByteArray NewerDocument();

    // Any thread; refreshes the downloaded copy on a worker thread, at most one attempt a day, failures included.
    void RefreshInBackground();
} // namespace Engine::TorBridges
