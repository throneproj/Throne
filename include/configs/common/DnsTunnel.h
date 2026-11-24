#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include <memory>

namespace Configs {
    class Profile;
    struct ResolverSource;

    // Includes Xray VLESS over mKCP with an xdns mask.
    bool IsDnsTunnelProfile(const Profile &profile);

    // Summed over the profile's chain hops and its group's front and landing proxies.
    int DnsTunnelStartupAllowanceMs(const std::shared_ptr<Profile> &profile);

    ResolverSource *ProfileResolverSource(const Profile &profile);

    struct DnstLink {
        QString tag;
        QString domain;
        QString transport;
        QString backend;
        // Readable-form parameter names; "pk" holds the whole PEM key.
        QHash<QString, QString> params;
        // qThroneMobile's top-level "resolvers" in the JSON form.
        QStringList resolvers;
        bool hasResolvers = false;
    };

    // Reports an unreadable SSH key through ParseDiag and drops it.
    bool DecodeDnstLink(const QString &link, DnstLink &out, QString *error);

    QString DnstLinkTransport(const QString &link);

    QString EncodeDnstLink(const DnstLink &link, bool asJson);

    // pk drops the PEM type, so it is told from the key's DER structure.
    QString DnstPrivateKeyPem(const QString &param);

    // "" for a legacy encrypted key, whose Proc-Type headers pk cannot carry.
    QString DnstPrivateKeyParam(const QString &pem);

    // Either alphabet, padded or not; "" for anything else.
    QByteArray DnsTunnelBase64(const QString &text);

    namespace Checks {
        QString DnsTunnelDomain(const QString &text);
    }
}
