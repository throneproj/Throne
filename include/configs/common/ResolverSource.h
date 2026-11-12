#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include <memory>

namespace Configs {
    // The IP list is read when a config is built, never stored in it.
    struct ResolverSource {
        QStringList lines;
        int ipListId = -1;

        // Only blank and # lines, and no IP list.
        [[nodiscard]] bool IsEmpty() const;

        bool operator==(const ResolverSource &other) const = default;
    };

    enum class ResolverFlavor {
        Dnstt,
        MasterDns,
        // Lines and list entries become xdns mask addresses: no scheme means udp://, no udp or tcp port means 53.
        Xdns,
    };

    struct ResolverBuild {
        QJsonArray resolvers;
        QString error;
    };

    // An empty source builds as system, except for Xdns, which builds as empty.
    ResolverBuild BuildResolvers(const ResolverSource &source, ResolverFlavor flavor, const QString &owner);

    QString DnsttResolverObjectLine(const QJsonObject &object, bool *tcpFallback, QString *why);

    QStringList ImportResolverLines(const QStringList &lines, ResolverFlavor flavor, const QString &field);

    // The address BuildResolvers gives an Xdns line; "" when the line adds none.
    QString XdnsResolverAddress(const QString &line);

    struct ResolverSourceCache;

    // Nested scopes on one thread share the outermost; without one every build reads its lists afresh and logs nothing.
    class ResolverSourceScope {
    public:
        ResolverSourceScope();
        ~ResolverSourceScope();
        ResolverSourceScope(const ResolverSourceScope &) = delete;
        ResolverSourceScope &operator=(const ResolverSourceScope &) = delete;

    private:
        std::unique_ptr<ResolverSourceCache> cache_;
        ResolverSourceCache *previous_;
    };

    namespace Checks {
        QString DnsttResolverLine(const QString &line);
        QString MasterDnsResolverLine(const QString &line);
        QString XdnsResolverLine(const QString &line);
    }
}
