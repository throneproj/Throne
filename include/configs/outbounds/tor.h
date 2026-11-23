#pragma once
#include <functional>

#include "include/configs/common/Outbound.h"

namespace Configs {
    // A Tor client the core runs; bridge lines are stored without the "Bridge" keyword.
    class tor : public outbound {
    public:
        // "none" | "builtin" | "custom"
        QString bridge_mode = "builtin";
        // "snowflake" | "obfs4" | "meek"; the lines come from Engine::TorBridges.
        QString builtin_type = "snowflake";
        // Kept across mode switches; only built for "custom".
        QStringList bridges;
        // Lowercase ISO 3166-1 alpha-2; empty for any exit.
        QString exit_country;

        // Any thread; under tor/ in the config folder, where the core pins it.
        static QString ExecutablePath();

        static bool Installed();

        // Set once at engine start by Engine::TorBridges: the built-in lines of a type, empty when unknown. Any thread, no network I/O.
        inline static std::function<QStringList(const QString &type)> BuiltinBridges;

        // The lines Build() emits; built-ins through BuiltinBridges.
        QStringList EffectiveBridges() const;

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

    inline const QStringList torBridgeModes = {"none", "builtin", "custom"};
    inline const QStringList torBuiltinTypes = {"snowflake", "obfs4", "meek"};
    inline const QStringList torExitCountries = {"de", "nl", "us", "fr", "ch", "se", "fi", "gb", "ca", "at",
                                                 "ro", "pl", "lu", "no", "cz", "es", "it", "bg", "sg", "jp"};

    // Build()'s error while the core has not reported its built-in lines; IsValid counts it as unchecked, not invalid.
    QString TorBuiltinBridgesUnavailable();

    // Why a config with a tor outbound cannot run before the download.
    QString TorNotDownloadedError();

    // Drops a leading "Bridge" keyword, collapses whitespace and spells the transport as the core does (meek -> meek_lite).
    QString NormalizeTorBridgeLine(const QString &line);

    // The transport and address: enough to point at a line without showing its keys.
    QString TorBridgeHint(const QString &line);

    // Strict, for telling bridge lines from other pasted text: a known transport or the "Bridge" keyword, then IP:port; a plain
    // bridge also needs its fingerprint.
    bool LooksLikeTorBridgeLine(const QString &line);

    // BridgeDB's QR form, ['obfs4 ...', 'obfs4 ...']; empty unless every item looks like a bridge line.
    QStringList ParseTorBridgeList(const QString &text);

    // Pasted bridge text, one line each or BridgeDB's QR form, normalized and without duplicates.
    QStringList SplitTorBridgeText(const QString &text);

    // "vanilla", "obfs4", "webtunnel", "meek", "snowflake" or the line's own transport, as TorBridgeSetting.type names them.
    QString TorBridgeType(const QString &line);

    QString TorTypeLabel(const QString &type);

    // "WebTunnel, 2 bridges" or "Snowflake, built-in".
    QString TorSettingLabel(const QString &type, bool builtin, int count);

    // The English territory name, or the code itself when Qt does not know it.
    QString TorCountryName(const QString &code);

    namespace Checks {
        // One bridge line, with or without the "Bridge" keyword.
        QString TorBridgeLine(const QString &line);
        // SplitTorBridgeText's lines: at least one, the first bad one named.
        QString TorBridgeText(const QString &text);
        QString TorExitCountry(const QString &code);
    }
}
