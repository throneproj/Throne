#pragma once
#include <QList>
#include <QString>
#include <QStringList>

namespace Configs
{
    struct PredefinedDNSEntry {
        QString domain;
        QStringList v4;
        QStringList v6;
    };

    // Hosts-file syntax: "<address> <domain> [domain...]", '#' comments, repeated domains accumulate.
    bool ParsePredefinedDNS(const QStringList &lines, QList<PredefinedDNSEntry> &out, QString *error = nullptr);

    // sing-box duration grammar: one or more "<number><unit>" with unit ns/us/ms/s/m/h/d.
    bool IsValidDuration(const QString &text);

    // port 0 means the type's default; iface is empty for dhcp://auto.
    struct DnsAddress {
        QString type;
        QString server;
        int port = 0;
        QString path;
        QString iface;
    };

    // "local...", "dhcp://auto|<iface>", "udp|tcp|tls|quic://host[:port]", "https|h3://host[:port][/path]" or a bare "host[:port]" (udp).
    bool ParseDnsAddress(const QString &text, DnsAddress *out, QString *why = nullptr);

    // A DNS setting holds one or more comma-separated addresses; several are raced and the first definitive answer wins.
    QStringList SplitDnsAddresses(const QString &text);

    // Empty when every address parses; otherwise why, naming the bad address when there are several.
    QString DnsAddressListProblem(const QString &text);

    QString NormalizeDnsAddressList(const QString &text);
}
