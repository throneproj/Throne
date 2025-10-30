#pragma once
#include "include/configs/baseConfig.h"
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class Peer : public baseConfig
    {
        public:
        QString address;
        int port = 0;
        QString public_key;
        QString pre_shared_key;
        QList<int> reserved;
        int persistent_keepalive = 0;

        Peer()
        {
            _add(new configItem("address", &address, string));
            _add(new configItem("port", &port, integer));
            _add(new configItem("public_key", &public_key, itemType::string));
            _add(new configItem("pre_shared_key", &pre_shared_key, itemType::string));
            _add(new configItem("reserved", &reserved, itemType::integerList));
            _add(new configItem("persistent_keepalive", &persistent_keepalive, itemType::integer));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class wireguard : public baseConfig, public outboundMeta
    {
        public:
        std::shared_ptr<OutboundCommons> commons = std::make_shared<OutboundCommons>();
        QString private_key;
        std::shared_ptr<Peer> peer = std::make_shared<Peer>();
        QStringList address;
        int mtu = 1420;
        bool system = false;
        int worker_count = 0;
        QString udp_timeout;

        // Amnezia options
        bool enable_amnezia = false;
        int junk_packet_count = 0;
        int junk_packet_min_size = 0;
        int junk_packet_max_size = 0;
        int init_packet_junk_size = 0;
        int response_packet_junk_size = 0;
        int init_packet_magic_header = 0;
        int response_packet_magic_header = 0;
        int underload_packet_magic_header = 0;
        int transport_packet_magic_header = 0;

        wireguard()
        {
            _add(new configItem("commons", dynamic_cast<JsonStore *>(commons.get()), jsonStore));

            _add(new configItem("private_key", &private_key, itemType::string));
            _add(new configItem("peer", dynamic_cast<JsonStore *>(peer.get()), jsonStore));
            _add(new configItem("address", &address, itemType::stringList));
            _add(new configItem("mtu", &mtu, itemType::integer));
            _add(new configItem("system", &system, itemType::boolean));
            _add(new configItem("worker_count", &worker_count, itemType::integer));
            _add(new configItem("udp_timeout", &udp_timeout, itemType::string));

            _add(new configItem("enable_amnezia", &enable_amnezia, itemType::boolean));
            _add(new configItem("junk_packet_count", &junk_packet_count, itemType::integer));
            _add(new configItem("junk_packet_min_size", &junk_packet_min_size, itemType::integer));
            _add(new configItem("junk_packet_max_size", &junk_packet_max_size, itemType::integer));
            _add(new configItem("init_packet_junk_size", &init_packet_junk_size, itemType::integer));
            _add(new configItem("response_packet_junk_size", &response_packet_junk_size, itemType::integer));
            _add(new configItem("init_packet_magic_header", &init_packet_magic_header, itemType::integer));
            _add(new configItem("response_packet_magic_header", &response_packet_magic_header, itemType::integer));
            _add(new configItem("underload_packet_magic_header", &underload_packet_magic_header, itemType::integer));
            _add(new configItem("transport_packet_magic_header", &transport_packet_magic_header, itemType::integer));
        }

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        // outboundMeta overrides
        QString DisplayAddress() override;
        QString DisplayName() override;
        QString DisplayType() override;
        QString DisplayTypeAndName() override;
        bool IsEndpoint() override;
    };
}


