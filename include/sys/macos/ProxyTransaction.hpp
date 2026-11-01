#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QVector>

namespace MacSystemProxy {
    // Only these keys are written. PAC, bypass lists, DNS, credentials and every other key remain untouched.
    inline QStringList keys(int protocol) {
        const QString prefix = QStringList{"HTTP", "HTTPS", "SOCKS"}.at(protocol);
        return {prefix + "Enable", prefix + "Proxy", prefix + "Port", prefix + "ProxyAuthenticated"};
    }
    inline QJsonObject select(const QJsonObject &object, const QStringList &fields) {
        QJsonObject result;
        for (const auto &key : fields) if (object.contains(key)) result.insert(key, object.value(key));
        return result;
    }
    inline bool validState(const QJsonObject &object, const QStringList &fields) {
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (!fields.contains(it.key())) return false;
            if (it.key() == fields[1]) {
                if (!it.value().isString() || it.value().toString().size() > 4096) return false;
            } else if (it.value().isBool() && (it.key() == fields[0] || it.key() == fields[3])) {
                // Preserve the type of legacy Boolean flags too.
            } else if (!it.value().isDouble() || it.value().toDouble() != it.value().toInt(-1)) return false;
        }
        return true;
    }
    struct Entry {
        QString service;
        int protocol;
        QJsonObject before;
        QJsonObject applied;
    };
    enum class Change { Done, Conflict, Missing, Failed };
    class Backend {
    public:
        virtual ~Backend() = default;
        virtual QString services(QStringList &ids) = 0;
        virtual QString read(const QString &id, QJsonObject &proxy, bool &exists) = 0;
        // Compare and merge under one system preferences lock. restore also accepts before already committed,
        // to retry ApplyChanges after a crash/failure between commit and activation without rewriting anything.
        virtual Change change(const Entry &entry, bool restore, QString &error) = 0;
    };
    class Journal {
    public:
        virtual ~Journal() = default;
        virtual QString load(QVector<Entry> &entries) = 0;
        virtual QString save(const QVector<Entry> &entries) = 0;
    };
    class Transaction {
        Backend &backend;
        Journal &journal;
    public:
        Transaction(Backend &backend, Journal &journal) : backend(backend), journal(journal) {}
        QString recover() {
            QVector<Entry> entries;
            if (auto error = journal.load(entries); !error.isEmpty()) return error;
            if (entries.isEmpty()) return {};
            QVector<Entry> pending;
            QStringList errors;
            for (const auto &entry : entries) {
                QString error;
                if (backend.change(entry, true, error) == Change::Failed) {
                    pending << entry;
                    errors << error;
                }
                // Missing services and third-party edits are deliberately relinquished.
            }
            if (auto error = journal.save(pending); !error.isEmpty()) errors << error;
            return errors.join("; ");
        }
        QString apply(const QString &host, int port) {
            if (host.isEmpty() || port < 1 || port > 65535) return "invalid system proxy endpoint";
            // Repeated Apply must never take a Throne endpoint for the user's original proxy.
            if (auto error = recover(); !error.isEmpty()) return error;
            QStringList services;
            if (auto error = backend.services(services); !error.isEmpty()) return error;
            QVector<Entry> entries;
            for (const auto &id : services) {
                QJsonObject original;
                bool exists = false;
                if (auto error = backend.read(id, original, exists); !error.isEmpty()) return error;
                if (!exists) continue;
                for (int protocol = 0; protocol != 3; ++protocol) {
                    const auto fields = keys(protocol);
                    const QJsonObject applied{{fields[0], 1}, {fields[1], host}, {fields[2], port}, {fields[3], 0}};
                    const auto before = select(original, fields);
                    if (!validState(before, fields)) return "unsupported original system proxy configuration";
                    if (before != applied) entries << Entry{id, protocol, before, applied};
                }
            }
            if (entries.isEmpty()) return {};
            // A durable write-ahead record precedes even the first system mutation.
            if (auto error = journal.save(entries); !error.isEmpty()) return error;
            for (const auto &entry : entries) {
                QString error;
                const auto result = backend.change(entry, false, error);
                if (result != Change::Done) {
                    if (error.isEmpty()) error = "network service changed while applying system proxy";
                    const auto rollback = recover();
                    return rollback.isEmpty() ? error : error + "; rollback: " + rollback;
                }
            }
            return {};
        }
    };
}
