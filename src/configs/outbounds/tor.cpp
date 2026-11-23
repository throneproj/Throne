#include "include/configs/outbounds/tor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"
#include "include/global/Utils.hpp"

namespace Configs {
    namespace {
        const QStringList kTorTransports = {"obfs4", "webtunnel", "meek_lite", "snowflake"};

        const QRegularExpression &torWhitespace() {
            static const QRegularExpression whitespace(QStringLiteral("\\s+"));
            return whitespace;
        }

        bool torIsHex(QStringView text) {
            return std::ranges::all_of(text, [](QChar c) {
                return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F');
            });
        }

        QString torMissingBridges() {
            return QCoreApplication::translate("Validation", "Add at least one bridge line, or choose another bridge mode.");
        }

        // "IPv4:port" or "[IPv6]:port"; Tor takes no host names here.
        QString torBridgeAddress(const QString &token) {
            QString host;
            QString port;
            auto family = Validation::Family::V4;
            if (token.startsWith(u'[')) {
                const auto close = token.indexOf(QLatin1String("]:"));
                if (close > 1) {
                    host = token.mid(1, close - 1);
                    port = token.mid(close + 2);
                }
                family = Validation::Family::V6;
            } else if (const auto colon = token.lastIndexOf(u':'); colon > 0) {
                host = token.left(colon);
                port = token.mid(colon + 1);
            }
            if (host.isEmpty() || !Validation::Ip(host, family).isEmpty() || !Validation::Port(port).isEmpty())
                return QCoreApplication::translate("Validation", "The bridge address must be an IP address with a port, like 192.0.2.1:443 or [2001:db8::1]:443.");
            return {};
        }

        QStringList torNormalizedLines(const QStringList &lines) {
            QStringList normalized;
            for (const auto &line : lines) {
                if (const auto text = NormalizeTorBridgeLine(line); !text.isEmpty() && !normalized.contains(text)) normalized << text;
            }
            return normalized;
        }
    }

    QString tor::ExecutablePath() {
#ifdef Q_OS_WIN
        return QDir(QStringLiteral("tor")).absoluteFilePath(QStringLiteral("tor.exe"));
#else
        return QDir(QStringLiteral("tor")).absoluteFilePath(QStringLiteral("tor"));
#endif
    }

    bool tor::Installed() {
        return QFileInfo(ExecutablePath()).isFile();
    }

    QStringList tor::EffectiveBridges() const {
        if (bridge_mode == QLatin1String("custom")) return torNormalizedLines(bridges);
        if (bridge_mode == QLatin1String("builtin") && BuiltinBridges) return BuiltinBridges(builtin_type);
        return {};
    }

    bool tor::ParseFromJson(const QJsonObject &object) {
        if (object.isEmpty() || object["type"].toString() != "tor") return false;
        outbound::ParseFromJson(object);
        server.clear();
        server_port = 0;
        bridges = torNormalizedLines(JsonStringList(object["bridges"], "bridges"));
        if (object.contains("bridge_mode")) {
            bridge_mode = object["bridge_mode"].toString().trimmed().toLower();
            builtin_type = object["builtin_type"].toString(builtin_type).trimmed().toLower();
            exit_country = object["exit_country"].toString().trimmed().toLower();
            return true;
        }
        // A sing-box tor outbound.
        bridge_mode = bridges.isEmpty() ? QStringLiteral("none") : QStringLiteral("custom");
        const auto countries = JsonStringList(object["exit_countries"], "exit_countries");
        exit_country = countries.value(0).trimmed().toLower();
        if (countries.size() > 1) {
            ParseDiag::Warn("exit_countries", QCoreApplication::translate("Validation", "Only the first exit country was imported."),
                            exit_country);
        }
        QStringList ignored;
        for (const auto *key : {"executable_path", "extra_args", "data_directory", "torrc"}) {
            if (object.contains(QLatin1String(key))) ignored << QString::fromLatin1(key);
        }
        if (!ignored.isEmpty()) {
            ParseDiag::Warn(ignored.first(),
                            QCoreApplication::translate("Validation", "Ignored %1: Tor profiles take only bridges and an exit country.")
                                .arg(ignored.join(QStringLiteral(", "))));
        }
        return true;
    }

    QString tor::ExportToLink() {
        return {};
    }

    QJsonObject tor::ExportToJson() {
        QJsonObject object;
        object["type"] = "tor";
        mergeJsonObjects(object, outbound::ExportToJson());
        object["bridge_mode"] = bridge_mode;
        object["builtin_type"] = builtin_type;
        if (!bridges.isEmpty()) object["bridges"] = QListStr2QJsonArray(bridges);
        if (!exit_country.isEmpty()) object["exit_country"] = exit_country;
        return object;
    }

    QJsonObject tor::ExportIdentity() {
        QJsonObject object{{"type", "tor"}, {"bridge_mode", bridge_mode}};
        if (bridge_mode == QLatin1String("builtin")) object["builtin_type"] = builtin_type;
        if (bridge_mode == QLatin1String("custom")) {
            auto lines = torNormalizedLines(bridges);
            lines.sort();
            object["bridges"] = QListStr2QJsonArray(lines);
        }
        if (!exit_country.isEmpty()) object["exit_country"] = exit_country;
        return object;
    }

    BuildResult tor::Build() {
        QJsonObject object{{"type", "tor"}};
        if (const auto message = Validation::OneOf(bridge_mode, torBridgeModes, Validation::Empty::Required); !message.isEmpty())
            return {{}, message};
        if (bridge_mode == QLatin1String("builtin")) {
            if (const auto message = Validation::OneOf(builtin_type, torBuiltinTypes, Validation::Empty::Required); !message.isEmpty())
                return {{}, message};
            const auto lines = EffectiveBridges();
            if (lines.isEmpty()) return {{}, TorBuiltinBridgesUnavailable()};
            object["bridges"] = QListStr2QJsonArray(lines);
        } else if (bridge_mode == QLatin1String("custom")) {
            const auto lines = EffectiveBridges();
            if (lines.isEmpty()) return {{}, torMissingBridges()};
            for (qsizetype i = 0; i < lines.size(); ++i) {
                if (const auto message = Checks::TorBridgeLine(lines[i]); !message.isEmpty())
                    return {{}, QCoreApplication::translate("Validation", "Bridge line %1: %2").arg(i + 1).arg(message)};
            }
            object["bridges"] = QListStr2QJsonArray(lines);
        }
        if (!exit_country.isEmpty()) {
            if (const auto message = Checks::TorExitCountry(exit_country); !message.isEmpty()) return {{}, message};
            object["exit_countries"] = QJsonArray{exit_country};
        }
        return {object, ""};
    }

    QString tor::DisplayAddress() {
        if (bridge_mode == QLatin1String("builtin")) return TorSettingLabel(builtin_type, true, 0);
        if (bridge_mode != QLatin1String("custom")) return QObject::tr("Tor network");
        QString type;
        bool uniform = true;
        int count = 0;
        for (const auto &line : bridges) {
            const auto text = NormalizeTorBridgeLine(line);
            if (text.isEmpty()) continue;
            ++count;
            const auto lineType = TorBridgeType(text);
            if (type.isEmpty()) type = lineType;
            else if (type != lineType) uniform = false;
        }
        if (count > 0 && uniform && type != QLatin1String("vanilla")) return TorSettingLabel(type, false, count);
        return QObject::tr("%n bridge(s)", nullptr, count);
    }

    QString tor::DisplayType() {
        return QStringLiteral("Tor");
    }

    SecurityInfo tor::GetSecurity() {
        return {QObject::tr("Encrypted"), {}, SecurityLevel::Secure};
    }

    void tor::Validate(Validation::Issues &issues) {
        outbound::Validate(issues);
        const auto report = [&issues](const QString &field, const QString &message, const QString &value = {}) {
            if (!message.isEmpty()) issues << Validation::Issue{Validation::Severity::Error, field, message, value};
        };
        report("bridge_mode", Validation::OneOf(bridge_mode, torBridgeModes, Validation::Empty::Required), bridge_mode);
        if (bridge_mode == QLatin1String("builtin")) {
            report("builtin_type", Validation::OneOf(builtin_type, torBuiltinTypes, Validation::Empty::Required), builtin_type);
        } else if (bridge_mode == QLatin1String("custom")) {
            const auto lines = torNormalizedLines(bridges);
            if (lines.isEmpty()) report("bridges", torMissingBridges());
            for (const auto &line : lines) {
                if (const auto message = Checks::TorBridgeLine(line); !message.isEmpty()) {
                    report("bridges", message, TorBridgeHint(line));
                    break;
                }
            }
        }
        report("exit_country", Checks::TorExitCountry(exit_country), exit_country);
    }

    QString TorBuiltinBridgesUnavailable() {
        return QCoreApplication::translate("Validation", "The built-in Tor bridges are not available: the core has not reported them.");
    }

    QString TorNotDownloadedError() {
        return QObject::tr("Tor is not downloaded yet; starting a profile that uses it offers the download.");
    }

    QString NormalizeTorBridgeLine(const QString &line) {
        auto tokens = line.split(torWhitespace(), Qt::SkipEmptyParts);
        if (!tokens.isEmpty() && tokens.first().compare(QLatin1String("Bridge"), Qt::CaseInsensitive) == 0) tokens.removeFirst();
        if (!tokens.isEmpty() && !tokens.first().contains(u':')) {
            auto transport = tokens.first().toLower();
            if (transport == QLatin1String("meek")) transport = QStringLiteral("meek_lite");
            tokens.first() = transport;
        }
        return tokens.join(u' ');
    }

    QString TorBridgeHint(const QString &line) {
        return NormalizeTorBridgeLine(line).section(u' ', 0, 1);
    }

    bool LooksLikeTorBridgeLine(const QString &line) {
        static const QStringList known = {"obfs4", "webtunnel", "meek", "meek_lite", "snowflake", "obfs2", "obfs3", "scramblesuit", "conjure"};
        auto tokens = line.split(torWhitespace(), Qt::SkipEmptyParts);
        const bool keyword = !tokens.isEmpty() && tokens.first().compare(QLatin1String("Bridge"), Qt::CaseInsensitive) == 0;
        if (keyword) tokens.removeFirst();
        if (tokens.isEmpty()) return false;
        if (!tokens.first().contains(u':')) {
            // Any transport after the keyword, so a refused one is named rather than skipped.
            if (!keyword && !known.contains(tokens.first().toLower())) return false;
            return tokens.size() >= 2 && torBridgeAddress(tokens[1]).isEmpty();
        }
        if (!torBridgeAddress(tokens.first()).isEmpty()) return false;
        // A bare address could be anything; a plain bridge also names its fingerprint.
        return keyword || (tokens.size() >= 2 && tokens[1].size() == 40 && torIsHex(tokens[1]));
    }

    QStringList ParseTorBridgeList(const QString &text) {
        const auto list = text.trimmed();
        if (list.size() < 2 || !list.startsWith(u'[') || !list.endsWith(u']')) return {};
        const qsizetype end = list.size() - 1;
        qsizetype at = 1;
        const auto skipSpace = [&] {
            while (at < end && list[at].isSpace()) ++at;
        };
        QStringList lines;
        skipSpace();
        while (at < end) {
            const QChar quote = list[at];
            if (quote != u'\'' && quote != u'"') return {};
            const auto close = list.indexOf(quote, at + 1);
            if (close < 0 || close >= end) return {};
            const auto line = list.mid(at + 1, close - at - 1);
            if (!LooksLikeTorBridgeLine(line)) return {};
            lines << line;
            at = close + 1;
            skipSpace();
            if (at < end) {
                if (list[at] != u',') return {};
                ++at;
                skipSpace();
            }
        }
        return lines;
    }

    QStringList SplitTorBridgeText(const QString &text) {
        auto lines = ParseTorBridgeList(text);
        if (lines.isEmpty()) lines = text.split(u'\n');
        return torNormalizedLines(lines);
    }

    QString TorBridgeType(const QString &line) {
        const auto first = line.section(u' ', 0, 0, QString::SectionSkipEmpty);
        if (first.isEmpty() || first.contains(u':')) return QStringLiteral("vanilla");
        const auto transport = first.toLower();
        return transport == QLatin1String("meek_lite") ? QStringLiteral("meek") : transport;
    }

    QString TorTypeLabel(const QString &type) {
        if (type == QLatin1String("snowflake")) return QStringLiteral("Snowflake");
        if (type == QLatin1String("webtunnel")) return QStringLiteral("WebTunnel");
        if (type == QLatin1String("vanilla")) return QObject::tr("Plain");
        return type;
    }

    QString TorSettingLabel(const QString &type, bool builtin, int count) {
        if (builtin) return QObject::tr("%1, built-in").arg(TorTypeLabel(type));
        return QObject::tr("%1, %n bridge(s)", nullptr, count).arg(TorTypeLabel(type));
    }

    QString TorCountryName(const QString &code) {
        return TerritoryName(code);
    }

    namespace Checks {
        QString TorBridgeLine(const QString &line) {
            const auto tokens = NormalizeTorBridgeLine(line).split(u' ', Qt::SkipEmptyParts);
            if (tokens.isEmpty()) return QCoreApplication::translate("Validation", "The bridge line is empty.");
            qsizetype at = 0;
            QString transport;
            if (!tokens.first().contains(u':')) {
                transport = tokens.first();
                if (!kTorTransports.contains(transport)) {
                    return QCoreApplication::translate("Validation", "%1 bridges are not supported; use obfs4, WebTunnel, Snowflake, meek or plain bridges.")
                        .arg(transport);
                }
                at = 1;
            }
            if (at >= tokens.size()) return QCoreApplication::translate("Validation", "The bridge address is missing.");
            if (const auto message = torBridgeAddress(tokens[at]); !message.isEmpty()) return message;
            ++at;
            if (at < tokens.size() && !tokens[at].contains(u'=')) {
                if (tokens[at].size() != 40 || !torIsHex(tokens[at]))
                    return QCoreApplication::translate("Validation", "The bridge fingerprint must be 40 hexadecimal characters.");
                ++at;
            }
            QSet<QString> keys;
            for (; at < tokens.size(); ++at) {
                const auto equals = tokens[at].indexOf(u'=');
                if (equals <= 0) return QCoreApplication::translate("Validation", "Write bridge options as key=value, like iat-mode=0.");
                keys.insert(tokens[at].left(equals));
            }
            const auto needs = [&transport](const QString &option) {
                return QCoreApplication::translate("Validation", "%1 bridges need the %2 option.").arg(TorTypeLabel(TorBridgeType(transport)), option);
            };
            if (transport == QLatin1String("obfs4")) {
                if (!keys.contains("cert") && !(keys.contains("node-id") && keys.contains("public-key"))) return needs("cert");
                if (!keys.contains("iat-mode")) return needs("iat-mode");
            } else if (transport == QLatin1String("webtunnel") || transport == QLatin1String("meek_lite")) {
                if (!keys.contains("url")) return needs("url");
            } else if (transport == QLatin1String("snowflake")) {
                if (keys.contains("sqsqueue")) {
                    if (!keys.contains("sqscreds")) return needs("sqscreds");
                } else if (!keys.contains("url")) {
                    return needs("url");
                }
            }
            return {};
        }

        QString TorBridgeText(const QString &text) {
            const auto lines = SplitTorBridgeText(text);
            if (lines.isEmpty()) return torMissingBridges();
            for (qsizetype i = 0; i < lines.size(); ++i) {
                if (const auto message = TorBridgeLine(lines[i]); !message.isEmpty())
                    return QCoreApplication::translate("Validation", "Bridge line %1: %2").arg(i + 1).arg(message);
            }
            return {};
        }

        QString TorExitCountry(const QString &code) {
            if (code.isEmpty()) return {};
            const bool letters = code.size() == 2 && std::ranges::all_of(code, [](QChar c) { return c >= u'a' && c <= u'z'; });
            if (!letters || QLocale::codeToTerritory(code) == QLocale::AnyTerritory)
                return QCoreApplication::translate("Validation", "Use a two-letter country code, like de.");
            return {};
        }
    }
}
