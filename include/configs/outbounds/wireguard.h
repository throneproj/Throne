#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    // Shared by the WireGuard editors (widget text) and wireguard::Validate (bean fields).
    namespace Checks {
        // Three bytes separated by ',' or '-', as the editor and links write them.
        QString WireguardReserved(const QString &text, QList<int> *bytes = nullptr);
        // Seconds, or an AmneziaWG range such as 22-30.
        QString WireguardKeepalive(const QString &text);
        // wireguard-go UintRange: n or lo-hi, both uint32.
        QString AmneziaRange(const QString &text);
        // H1-H4 in order; unset ones are 1, 2, 3 and 4 in wireguard-go.
        QString AmneziaHeaders(const QStringList &headers);
        QString AmneziaHeaderProtection(const QString &key, const QList<qint64> &paddings);
        QString AmneziaJunkSizes(qint64 jmin, qint64 jmax);
        // wireguard-go newObfChain: <b 0xHEX>, <r N>, <rc N>, <rd N>, <dz N>, <t>, <d>, <ds>; text outside tags is ignored.
        QString AmneziaSignature(const QString &spec);
    }

    class Peer : public baseConfig
    {
        public:
        QString address;
        int port = 0;
        QString public_key;
        QString pre_shared_key;
        QList<int> reserved;
        // Seconds, or an AmneziaWG 3.0 range such as "22-30".
        QString persistent_keepalive;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        private:
        void WriteKeepalive(QJsonObject& object) const;
    };

    class wireguard : public outbound
    {
        public:
        QString private_key;
        std::shared_ptr<Peer> peer = std::make_shared<Peer>();
        QStringList address;
        int mtu = 1420;
        bool system = false;
        int worker_count = 0;
        QString udp_timeout;

        // AmneziaWG: jc/jmin/jmax and s1-s4 are integers; h1-h4 (magic headers) and i1-i5 (signature packets) are strings.
        bool enable_amnezia = false;
        int jc = 0;
        int jmin = 0;
        int jmax = 0;
        int s1 = 0;
        int s2 = 0;
        int s3 = 0;
        int s4 = 0;
        QString h1;
        QString h2;
        QString h3;
        QString h4;
        QString i1;
        QString i2;
        QString i3;
        QString i4;
        QString i5;

        // AmneziaWG 3.0: header_protection_key is a base64 32-byte key; the rest are ranges ("30" or "22-30").
        QString header_protection_key;
        QString content_padding_addition;
        QString rekey_after_time;
        QString rekey_timeout;
        QString reject_after_time;
        QString keepalive_timeout;
        QString max_handshake_attempts;

        // AmneziaWG 3.1
        bool random_trailers = false;
        bool disable_cookies = false;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        void SetPort(int newPort) override;
        QString GetPort() override;
        void SetAddress(QString newAddr) override;
        QString GetAddress() override;
        QString DisplayAddress() override;
        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
        bool IsEndpoint() override;

        void Validate(Validation::Issues& issues) override;

        // The AmneziaWG part of Validate, keyed by amnezia_wg field; it applies only with enable_amnezia.
        Validation::Issues AmneziaIssues() const;

        // Bare IPs get /32 or /128; the core needs a prefix.
        void FixAddress();

        private:
        QJsonObject AmneziaToJson();
        void AmneziaFromJson(const QJsonObject& object);
        static QString AmneziaRangeFromJson(const QJsonValue& value);
    };
}


