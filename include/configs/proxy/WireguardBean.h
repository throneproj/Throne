#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class WireguardBean : public AbstractBean {
    public:
        QString privateKey;
        QString publicKey;
        QString preSharedKey;
        QList<int> reserved;
        int persistentKeepalive = 0;
        QStringList localAddress;
        int MTU = 1420;
        bool useSystemInterface = false;
        int workerCount = 0;

        // Amnezia Options
        bool enable_amnezia = false;
        int junk_packet_count;
        int junk_packet_min_size;
        int junk_packet_max_size;
        int init_packet_junk_size;
        int response_packet_junk_size;
        int init_packet_magic_header;
        int response_packet_magic_header;
        int underload_packet_magic_header;
        int transport_packet_magic_header;

        WireguardBean() : AbstractBean(0) {
            _add(new configItem("private_key", &privateKey, itemType::string));
            _add(new configItem("public_key", &publicKey, itemType::string));
            _add(new configItem("pre_shared_key", &preSharedKey, itemType::string));
            _add(new configItem("reserved", &reserved, itemType::integerList));
            _add(new configItem("persistent_keepalive", &persistentKeepalive, itemType::integer));
            _add(new configItem("local_address", &localAddress, itemType::stringList));
            _add(new configItem("mtu", &MTU, itemType::integer));
            _add(new configItem("use_system_proxy", &useSystemInterface, itemType::boolean));
            _add(new configItem("worker_count", &workerCount, itemType::integer));

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
        };

        QString FormatReserved() {
            QString res = "";
            for (int i=0;i<reserved.size();i++) {
                res += Int2String(reserved[i]);
                if (i != reserved.size() - 1) {
                    res += "-";
                }
            }
            return res;
        }

        QString DisplayType() override { return "Wireguard"; };

        CoreObjOutboundBuildResult BuildCoreObjSingBox() override;

        bool TryParseLink(const QString &link);

        bool TryParseJson(const QJsonObject &obj);

        QString ToShareLink() override;

        bool IsEndpoint() override {return true;}
    };
} // namespace Configs
