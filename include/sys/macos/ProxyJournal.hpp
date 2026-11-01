#pragma once
#include "ProxyTransaction.hpp"
#include <utility>

namespace MacSystemProxy {
    // The caller holds the per-user QLockFile for the entire lifetime of an applied transaction.
    class FileJournal final : public Journal {
        QString path;
    public:
        explicit FileJournal(QString path) : path(std::move(path)) {}
        QString load(QVector<Entry> &entries) override;
        QString save(const QVector<Entry> &entries) override;
    };
}
