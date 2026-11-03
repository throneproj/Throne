#pragma once
#include "include/configs/common/Outbound.h"
#include "include/configs/common/ResolverSource.h"

namespace Configs
{
    class masterdns : public outbound
    {
        public:
        QStringList domains;
        ResolverSource resolvers;
        QString mode = "socks5";
        QString encryption_method = "xor";
        QString encryption_key;
        bool base64_response = false;
        // Empty or 0 leaves the core's default.
        QString upload_compression;
        QString download_compression;
        QString balancing_strategy;
        int packet_duplication = 0;
        int setup_packet_duplication = 0;
        int mtu_min_upload = 0;
        int mtu_max_upload = 0;
        int mtu_min_download = 0;
        int mtu_max_download = 0;
        int mtu_test_retries = 0;
        QString mtu_test_timeout;
        int mtu_test_parallelism = 0;
        bool mtu_auto_remove_low = true;

        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;

        QString DisplayAddress() override;
        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
        void Validate(Validation::Issues& issues) override;

        bool HasServerEndpoint() override { return false; }
    };

    inline const QStringList masterDnsModes = {"socks5", "tcp"};
    // Indexed by MasterDnsVPN's DATA_ENCRYPTION_METHOD.
    inline const QStringList masterDnsEncryptionMethods = {"none", "xor", "chacha20", "aes-128-gcm", "aes-192-gcm", "aes-256-gcm"};
    // Indexed by its UPLOAD_ and DOWNLOAD_COMPRESSION_TYPE.
    inline const QStringList masterDnsCompressions = {"off", "zstd", "lz4", "zlib"};
    inline const QStringList masterDnsBalancingStrategies = {"round_robin", "random", "least_loss", "lowest_latency", "hybrid",
                                                             "loss_then_latency", "least_loss_top_random", "least_loss_top_round_robin"};

    namespace Checks {
        QString MasterDnsEncryptionKey(const QString& method, const QString& key);
        QString MasterDnsDuplication(int duplication);
        QString MasterDnsMtuRange(int min, int max, int defaultMin, int defaultMax);
    }
}
