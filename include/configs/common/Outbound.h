#pragma once
#include <QHostInfo>
#include <utility>
#include "DialFields.h"
#include "QUICFields.h"
#include "multiplex.h"
#include "TLS.h"
#include "transport.h"
#include "xrayMultiplex.h"
#include "xrayStreamSetting.h"
#include "include/configs/baseConfig.h"
#include "include/global/Validation.hpp"

namespace Configs
{
    inline QStringList vPacketEncoding = {"", "packetaddr", "xudp"};

    // openvpn/openconnect main profile: ignore the pushed resolvers, use them for the names they claim, or also for every remote query.
    inline constexpr auto kTunnelDnsNone = "none";
    inline constexpr auto kTunnelDnsPrefer = "prefer";
    inline constexpr auto kTunnelDnsStrict = "strict";

    // Ordered worst-to-best; the ordering is relied on when sorting by security.
    enum class SecurityLevel {
        Unknown = 0,
        None,
        Weak,
        Secure,
    };

    struct SecurityInfo {
        QString label;
        QString transport;
        SecurityLevel level = SecurityLevel::Unknown;
        // Secure only through the CA check, which the global skip_cert setting turns off.
        bool caVerified = false;
        // Weakened by that global setting rather than by the profile itself.
        bool compromised = false;
        // Set by a weaker leg to a public server, which a private profile address does not make moot.
        bool publicLeg = false;

        bool isDangerous() const {
            return level == SecurityLevel::None || level == SecurityLevel::Weak;
        }

        // What "Remove insecure" acts on: a global setting is no reason to delete a profile.
        bool isInsecure() const { return isDangerous() && !compromised; }
    };

    // Empty for the transports not worth showing (plain tcp / xray raw).
    QString DisplayTransportName(const QString& type);

    // Traffic to a private address never crosses the internet, so its protection is moot.
    SecurityInfo WithPrivateServer(SecurityInfo info, const QString& host);

    // The XHTTP download leg (XrayDownloadLeg) carries what the server sends back, so a weaker one to a public server sets the label.
    SecurityInfo WithXrayDownloadLeg(SecurityInfo info, const QString& host, const QList<QJsonObject>& leg);

    namespace Checks {
        // The global custom TLS fragment size, "n" or "min-max" bytes: a max of 0 never advances, one above 1019 panics the core.
        QString TlsFragmentSize(const QString& spec);
        // A warning for a max above 400: five chunks can then overflow the core's 2048-byte queue on a very large ClientHello.
        QString TlsFragmentSizeLarge(const QString& spec);
        QString TlsFragmentSleep(const QString& spec);
    }

    // Why the global custom TLS-fragment settings fail every fragmenting TCP build; empty when they are usable.
    QString FragmentSettingsError();

    class outbound : public baseConfig
    {
    public:
        QString name;
        QString server;
        int server_port = 0;
        bool invalid = false;
        // Owning profile, stamped by ProfilesRepo; not part of the serialised outbound.
        int profile_id = -1;
        std::shared_ptr<DialFields> dialFields = std::make_shared<DialFields>();

        void ResolveDomainToIP(const std::function<void()> &onFinished) {
            bool noResolve = false;
            auto serverAddr = GetAddress();
            if (IsIpAddress(serverAddr) || serverAddr.isEmpty()) noResolve = true;
            if (noResolve) {
                onFinished();
                return;
            }
            QHostInfo::lookupHost(serverAddr, QApplication::instance(), [=, this](const QHostInfo &host) {
                auto addrs = host.addresses();
                if (!addrs.isEmpty()) SetAddress(addrs.first().toString());
                onFinished();
            });
        }

        virtual void SetAddress(QString newAddr) {
            server = std::move(newAddr);
        }

        virtual QString GetAddress()
        {
            return server;
        }

        virtual void SetPort(int newPort) {
            server_port = newPort;
        }

        virtual QString GetPort() {
            return QString::number(server_port);
        }

        virtual QString DisplayAddress()
        {
            return ::DisplayAddress(server, server_port);
        }

        virtual QString DisplayName()
        {
            if (name.isEmpty()) {
                return DisplayAddress();
            }
            return name;
        }

        virtual QString DisplayType() { return {}; };

        QString DisplayTypeAndName()
        {
            return QString("[%1] %2").arg(DisplayType(), DisplayName());
        }

        // What the profile's own settings give; consumers want EffectiveSecurity().
        virtual SecurityInfo GetSecurity();

        // GetSecurity() plus what the profile can't see: a private server and the global skip_cert.
        SecurityInfo EffectiveSecurity();

        QString DisplaySecurity();

    protected:
        SecurityInfo SecurityFromTLS(const QString& transport);

    public:

        virtual bool IsXray() { return false; }

        // Owns the stream GetXrayStream() returns; a custom Xray outbound runs on Xray but has none.
        virtual bool HasXrayStream() { return false; }

        virtual bool IsExtraCore() { return false; }

        virtual bool IsXrayFullConfig() { return false; }

        virtual bool HasMux() { return false; }

        virtual bool HasTransport() { return false; }

        virtual bool HasTLS() { return false; }

        virtual bool MustTLS() { return false; }

        virtual bool LimitedTLS() { return false; }

        virtual bool HasQUIC() { return false; }

        virtual std::shared_ptr<TLS> GetTLS() { return std::make_shared<TLS>(); }

        virtual std::shared_ptr<QUICFields> GetQUIC() { return std::make_shared<QUICFields>(); }

        virtual std::shared_ptr<Transport> GetTransport() { return std::make_shared<Transport>(); }

        virtual std::shared_ptr<Multiplex> GetMux() { return std::make_shared<Multiplex>(); }

        virtual std::shared_ptr<xrayStreamSetting> GetXrayStream() { return std::make_shared<xrayStreamSetting>(); }

        virtual std::shared_ptr<xrayMultiplex> GetXrayMultiplex() { return std::make_shared<xrayMultiplex>(); }

        virtual bool IsEndpoint() { return false; };

        virtual bool SupportsCredentialStrip() const { return false; }

        virtual void StripCredentials() {}

        virtual BuildResult BuildXray() { return {}; }

        // Runs after every import parse; Error issues refuse the profile. Overrides call the base first and never mutate.
        virtual void Validate(Validation::Issues &issues);

        // Called on every import, before Validate: drops state that only means something on this installation or would run a program.
        virtual void StripForeignState(Validation::Issues &issues) {}

        // Base Validate's server checks: types without a server of their own, and inputs (port ranges, realm, server lists) that supply it.
        virtual bool HasServerEndpoint() { return true; }

        virtual bool ServerPortOptional() { return false; }

        virtual bool ServerAddressOptional() { return false; }

        QString ExportJsonLink(bool stripMetadata = false) {
            auto json = ExportToJson();
            if (stripMetadata) {
                json.remove("tag");
            }
            const auto b64 = QJsonObject2QString(json, true)
                                .toUtf8()
                                .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
            return QStringLiteral("throne://add/") + QString::fromLatin1(b64);
        }

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        bool ParseFromClash(const clash::Proxies& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;
    };

    // Whether outbound::Build emits the dialer-level ("custom") TLS fragment, which also drops tcp_fast_open.
    bool CustomFragmentApplies(outbound& out);
}
