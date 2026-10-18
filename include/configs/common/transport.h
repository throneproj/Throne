#pragma once
#include "include/configs/baseConfig.h"
#include "include/global/Validation.hpp"

namespace Configs
{
    // sing-box V2RayTransportOptions types; tcp (or empty) means no transport.
    inline const QStringList singBoxTransports = {"tcp", "ws", "httpupgrade", "http", "grpc", "quic"};

    // sing-box matches types exactly in lower case; websocket is Xray's alias, which share links and v2rayN JSON carry too.
    inline QString SingBoxTransportType(const QString& type)
    {
        const auto t = type.trimmed().toLower();
        return t == QLatin1String("websocket") ? QStringLiteral("ws") : t;
    }

    // sing-quic dials the quic transport with a std TLS config, which a uTLS one refuses: Build drops uTLS there.
    // REALITY keeps it, since it cannot run without uTLS; Validate refuses that profile.
    QJsonObject TlsForTransport(QJsonObject tls, const QString& transportType);

    namespace Checks {
        QString TransportType(const QString& type);
        // A warning: the Host header may carry a port; http takes a comma list.
        QString TransportHost(const QString& host, const QString& type);
        // tls: TLS is on; reality: the Reality key is set. utls is not an error: see QuicTransportUtls.
        QString QuicTransport(bool tls, bool utls, bool reality);
        // A warning: utls is the profile's own fingerprint (not unsafe), which TlsForTransport drops.
        QString QuicTransportUtls(bool utls);
    }

    class Transport : public baseConfig
    {
        public:
        QString type;

        // HTTP
        QString host;
        QString path;
        QString method;
        QStringList headers;
        QString idle_timeout;
        QString ping_timeout;

        // Websocket
        int max_early_data = 0;
        QString early_data_header_name;

        // gRPC
        QString service_name;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QString ExportToLink(bool tlsEnabled);
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;

        // See Checks::QuicTransport for the flags.
        void Validate(Validation::Issues& issues, bool tls, bool utls, bool reality);
    };
}
