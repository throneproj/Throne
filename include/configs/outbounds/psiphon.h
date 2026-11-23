#pragma once

#include "include/configs/common/Outbound.h"

namespace Configs {
    // A Psiphon client the core runs on Psiphon Inc.'s public network; the core pins its data folder.
    class psiphon : public outbound {
    public:
        // Uppercase ISO 3166-1 alpha-2, as Psiphon spells it; empty for the best-performing region.
        QString egress_region;

        bool ParseFromJson(const QJsonObject &object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        QJsonObject ExportIdentity() override;
        BuildResult Build() override;

        QString DisplayAddress() override;
        QString DisplayType() override;
        SecurityInfo GetSecurity() override;
        void Validate(Validation::Issues &issues) override;

        bool HasServerEndpoint() override { return false; }
    };

    // What Psiphon offered when Throne last shipped, for while the core has not reported the live list.
    inline const QStringList psiphonRegions = {"AT", "AU", "BE", "CA", "CH", "CZ", "DE", "DK", "ES", "FI", "FR", "GB", "ID",
                                               "IE", "IN", "IT", "JP", "LT", "NL", "NO", "PL", "RO", "RS", "SE", "SG", "US"};

    // The English territory name; "Best performance" for an empty code.
    QString PsiphonRegionName(const QString &code);

    namespace Checks {
        QString PsiphonRegion(const QString &code);
    }
}
