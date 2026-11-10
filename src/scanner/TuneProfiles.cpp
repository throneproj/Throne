#include "include/scanner/TuneProfiles.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRegularExpression>

#include <algorithm>

#include "include/configs/generate.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"

namespace Scanner::Tune {
    namespace {
        // sing-box's WireGuard MTU when the profile leaves it at 0.
        constexpr int kTuneProfilesDefaultWireguardMtu = 1408;

        const QRegularExpression &tuneProfilesSeparators() {
            static const QRegularExpression separators(QStringLiteral("[,;\\s]+"));
            return separators;
        }

        // TLS straight over TCP, the only kind the fingerprint and fragment knobs reach; nullptr otherwise.
        std::shared_ptr<Configs::TLS> tuneProfilesTcpTls(const Configs::Profile &profile) {
            auto *outbound = profile.outbound.get();
            if (outbound == nullptr || outbound->IsXray() || !outbound->HasTLS() || outbound->HasQUIC() || outbound->LimitedTLS())
                return nullptr;
            // QUIC carriers that HasQUIC does not report.
            if (profile.type == QLatin1String("juicity")) return nullptr;
            if (const auto *trustTunnel = profile.TrustTunnel(); trustTunnel != nullptr && trustTunnel->quic) return nullptr;
            if (outbound->HasTransport() && outbound->GetTransport()->type == QLatin1String("quic")) return nullptr;
            auto tls = outbound->GetTLS();
            if (tls == nullptr || (!outbound->MustTLS() && !tls->enabled)) return nullptr;
            return tls;
        }

        bool tuneProfilesRealityOn(const Configs::TLS &tls) {
            return tls.reality->enabled || !tls.reality->public_key.isEmpty();
        }

        // The fork sends the opener from the dialer of anything that opens a UDP flow to the server.
        bool tuneProfilesQuicCarrier(const Configs::Profile &profile) {
            auto *outbound = profile.outbound.get();
            if (outbound == nullptr || outbound->IsXray()) return false;
            if (const auto *masque = profile.Masque(); masque != nullptr) return masque->http_version != 2;
            return outbound->HasQUIC() || profile.Wireguard() != nullptr;
        }

        bool tuneProfilesIsDomain(QString host) {
            host = host.trimmed();
            if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']'))) host = host.mid(1, host.size() - 2);
            return !host.isEmpty() && QHostAddress(host).isNull();
        }

        Knob tuneProfilesSwitch(const char *key, bool on) {
            return {key, on ? kOn : kOff, {on ? kOff : kOn}};
        }

        QString tuneProfilesOnOff(const QString &value, const char *on, const char *off) {
            return QCoreApplication::translate("Scanner::Tune", value == QLatin1String(kOn) ? on : off);
        }
    } // namespace

    QString Unsupported(const Configs::Profile &profile) {
        if (profile.outbound == nullptr) return QCoreApplication::translate("Scanner::Tune", "The profile could not be read");
        if (profile.Chain() != nullptr)
            return QCoreApplication::translate("Scanner::Tune", "A chain cannot be searched as a whole; run the search on the profiles of its hops");
        if (profile.outbound->IsXray())
            return QCoreApplication::translate("Scanner::Tune", "Xray profiles are not supported yet: the search changes sing-box settings");
        if (SpaceFor(profile, Configs::ScanTuneOptions{}).IsEmpty())
            return QCoreApplication::translate("Scanner::Tune",
                                               "This profile has no setting the search can change: that needs TLS over TCP, QUIC or WireGuard");
        return {};
    }

    Space SpaceFor(const Configs::Profile &profile, const Configs::ScanTuneOptions &options) {
        Space space;
        if (profile.outbound == nullptr || profile.Chain() != nullptr || profile.outbound->IsXray()) return space;
        if (const auto tls = tuneProfilesTcpTls(profile); tls != nullptr) {
            const bool reality = tuneProfilesRealityOn(*tls);
            if (options.fingerprints && tls->utls->supported) {
                auto current = tls->utls->Build().object.value(QLatin1String("fingerprint")).toString();
                if (current.isEmpty()) current = reality ? QStringLiteral("random") : QString(kOff);
                Knob knob{kFingerprint, current, {}};
                for (const auto &name : ParseFingerprints(options.fingerprintList)) {
                    // REALITY always runs uTLS.
                    if (name != current && !(reality && name == QLatin1String(kOff))) knob.values << name;
                }
                space.knobs << knob;
            }
            const bool fragmentOn = tls->FragmentEffectivelyOn();
            if (options.fragment) {
                space.knobs << tuneProfilesSwitch(kFragment, fragmentOn);
                // The custom implementation fragments at the dialer with the global size and sleep, so no delay applies.
                if (Configs::dataManager->settingsRepo->fragment_implementation != QLatin1String("custom")) {
                    space.delay.key = kFragmentDelay;
                    space.delay.current = tls->fragment_fallback_delay.trimmed();
                    for (const auto &delay : ParseDelays(options.fragmentDelays)) {
                        if (delay != space.delay.current) space.delay.values << delay;
                    }
                    space.fragmentOn = fragmentOn;
                }
            }
            if (options.recordFragment) space.knobs << tuneProfilesSwitch(kRecordFragment, tls->record_fragment);
            const auto sni = tls->server_name.trimmed().isEmpty() ? profile.outbound->GetAddress() : tls->server_name;
            if (options.mixedCaseSni && !reality && !tls->disable_sni && tuneProfilesIsDomain(sni))
                space.knobs << tuneProfilesSwitch(kMixedCaseSni, tls->TlsTricksEffectivelyOn());
        }
        if (options.quicOpener && tuneProfilesQuicCarrier(profile))
            space.knobs << tuneProfilesSwitch(kQuicOpener, profile.outbound->dialFields->quic_opener);
        if (const auto *wireguard = profile.Wireguard(); wireguard != nullptr && options.mtu) {
            space.mtuCurrent = wireguard->mtu > 0 ? wireguard->mtu : kTuneProfilesDefaultWireguardMtu;
            space.mtuMin = options.mtuMin;
            space.mtuMax = options.mtuMax > 0 ? options.mtuMax : space.mtuCurrent;
            space.mtuStep = std::max(1, options.mtuStep);
            space.mtu = space.mtuMax > space.mtuMin;
        }
        return space;
    }

    Limits LimitsFor(const Configs::ScanTuneOptions &options) {
        Limits limits;
        limits.attempts = std::max(1, options.attempts);
        limits.maxAttempts = std::max(1, options.maxAttempts);
        limits.mtuAttempts = std::clamp(options.attempts / 2, 1, 3);
        limits.confirmTop = 3;
        return limits;
    }

    bool Apply(Configs::Profile &profile, const Assignment &assignment, QString *error) {
        const auto fail = [error](const QString &text) {
            if (error != nullptr) *error = text;
            return false;
        };
        if (profile.outbound == nullptr) return fail(QCoreApplication::translate("Scanner::Tune", "The profile could not be read"));
        for (auto it = assignment.cbegin(); it != assignment.cend(); ++it) {
            const auto &key = it.key();
            const auto &value = it.value();
            const bool on = value == QLatin1String(kOn);
            if (key == QLatin1String(kQuicOpener)) {
                profile.outbound->dialFields->quic_opener = on;
                continue;
            }
            if (key == QLatin1String(kMtu)) {
                auto *wireguard = profile.Wireguard();
                bool ok = false;
                const int mtu = value.toInt(&ok);
                if (wireguard == nullptr || !ok || mtu <= 0)
                    return fail(QCoreApplication::translate("Scanner::Tune", "The MTU applies only to WireGuard profiles"));
                wireguard->mtu = mtu;
                continue;
            }
            const auto tls = tuneProfilesTcpTls(profile);
            if (tls == nullptr) return fail(QCoreApplication::translate("Scanner::Tune", "The profile no longer uses TLS over TCP"));
            if (key == QLatin1String(kFingerprint)) {
                if (!tls->utls->supported)
                    return fail(QCoreApplication::translate("Scanner::Tune", "This type of profile has no uTLS fingerprint"));
                tls->utls->enabled = true;
                // "unsafe" overrides the global fingerprint, which "off" alone would fall back to.
                tls->utls->fingerPrint = value == QLatin1String(kOff) ? Configs::tlsUnsafeFingerprint : value;
            } else if (key == QLatin1String(kFragment)) {
                tls->fragment = on;
                tls->fragment_unspecified = false;
            } else if (key == QLatin1String(kFragmentDelay)) {
                tls->fragment_fallback_delay = value;
            } else if (key == QLatin1String(kRecordFragment)) {
                tls->record_fragment = on;
            } else if (key == QLatin1String(kMixedCaseSni)) {
                tls->tls_tricks = on;
                tls->tls_tricks_unspecified = false;
            } else {
                return fail(QCoreApplication::translate("Scanner::Tune", "Unknown setting “%1”").arg(key));
            }
        }
        return true;
    }

    std::shared_ptr<Configs::Profile> Copy(const Configs::Profile &profile) {
        if (profile.outbound == nullptr) return nullptr;
        auto copy = Configs::ProfilesRepo::NewProfile(profile.type);
        if (copy == nullptr || copy->outbound == nullptr || copy->outbound->invalid) return nullptr;
        if (!copy->outbound->ParseFromJson(profile.outbound->ExportToJson())) return nullptr;
        copy->id = -1;
        copy->gid = profile.gid;
        copy->name = profile.name;
        copy->endpoint = profile.endpoint;
        // A scan base is itself a copy that still names the profile it came from.
        copy->outbound->profile_id = profile.id >= 0 ? profile.id : profile.outbound->profile_id;
        return copy;
    }

    QString Describe(const Assignment &assignment) {
        if (assignment.isEmpty()) return QCoreApplication::translate("Scanner::Tune", "The profile as it is");
        QStringList parts;
        for (const char *key : {kFingerprint, kFragment, kFragmentDelay, kRecordFragment, kMixedCaseSni, kQuicOpener, kMtu}) {
            const auto it = assignment.constFind(QLatin1String(key));
            if (it == assignment.cend()) continue;
            const auto &value = it.value();
            if (it.key() == QLatin1String(kFingerprint)) {
                parts << (value == QLatin1String(kOff) ? QCoreApplication::translate("Scanner::Tune", "uTLS off")
                                                       : QCoreApplication::translate("Scanner::Tune", "Fingerprint %1").arg(value));
            } else if (it.key() == QLatin1String(kFragment)) {
                parts << tuneProfilesOnOff(value, QT_TRANSLATE_NOOP("Scanner::Tune", "TLS fragment on"),
                                           QT_TRANSLATE_NOOP("Scanner::Tune", "TLS fragment off"));
            } else if (it.key() == QLatin1String(kFragmentDelay)) {
                parts << QCoreApplication::translate("Scanner::Tune", "Fragment delay %1")
                             .arg(value.isEmpty() ? QCoreApplication::translate("Scanner::Tune", "default") : value);
            } else if (it.key() == QLatin1String(kRecordFragment)) {
                parts << tuneProfilesOnOff(value, QT_TRANSLATE_NOOP("Scanner::Tune", "Record fragment on"),
                                           QT_TRANSLATE_NOOP("Scanner::Tune", "Record fragment off"));
            } else if (it.key() == QLatin1String(kMixedCaseSni)) {
                parts << tuneProfilesOnOff(value, QT_TRANSLATE_NOOP("Scanner::Tune", "Mixed-case SNI on"),
                                           QT_TRANSLATE_NOOP("Scanner::Tune", "Mixed-case SNI off"));
            } else if (it.key() == QLatin1String(kQuicOpener)) {
                parts << tuneProfilesOnOff(value, QT_TRANSLATE_NOOP("Scanner::Tune", "QUIC opener on"),
                                           QT_TRANSLATE_NOOP("Scanner::Tune", "QUIC opener off"));
            } else {
                parts << QCoreApplication::translate("Scanner::Tune", "MTU %1").arg(value);
            }
        }
        return parts.join(QStringLiteral(", "));
    }

    QStringList ParseFingerprints(const QString &text, QString *error) {
        const auto report = [error](const QString &problem) {
            if (error != nullptr && error->isEmpty()) *error = problem;
        };
        QStringList names;
        for (const auto &part : text.split(tuneProfilesSeparators(), Qt::SkipEmptyParts)) {
            const auto name = part.toLower();
            if (names.contains(name)) continue;
            if (name == QLatin1String("random") || name == QLatin1String("randomized")) {
                report(QCoreApplication::translate("Scanner::Tune", "“%1” changes from one start to the next, so it cannot be tried").arg(part));
                continue;
            }
            if (name != QLatin1String(kOff) && (!Configs::tlsFingerprints.contains(name) || name == Configs::tlsUnsafeFingerprint)) {
                report(QCoreApplication::translate("Scanner::Tune", "“%1” is not a uTLS fingerprint").arg(part));
                continue;
            }
            names << name;
        }
        return names;
    }

    QStringList ParseDelays(const QString &text, QString *error) {
        QStringList delays;
        for (const auto &part : text.split(tuneProfilesSeparators(), Qt::SkipEmptyParts)) {
            if (const auto problem = Validation::Duration(part, {Validation::Empty::Required}); !problem.isEmpty()) {
                if (error != nullptr && error->isEmpty())
                    *error = QCoreApplication::translate("Scanner::Tune", "Fragment delay “%1”: %2").arg(part, problem);
                continue;
            }
            if (!delays.contains(part)) delays << part;
        }
        return delays;
    }

    QString CheckOptions(const Configs::ScanTuneOptions &options) {
        QString error;
        if (options.fingerprints) {
            (void) ParseFingerprints(options.fingerprintList, &error);
            if (!error.isEmpty()) return error;
        }
        if (options.fragment) {
            (void) ParseDelays(options.fragmentDelays, &error);
            if (!error.isEmpty()) return error;
        }
        if (options.mtu && options.mtuMax > 0 && options.mtuMax <= options.mtuMin)
            return QCoreApplication::translate("Scanner::Tune", "The largest MTU must be above the smallest");
        return {};
    }

    QString DialHost(const std::shared_ptr<Configs::Profile> &profile) {
        if (profile == nullptr || profile->outbound == nullptr) return {};
        if (const auto host = Configs::EffectiveEndpointHost(profile); !host.isEmpty()) return host;
        return profile->outbound->GetAddress();
    }

    QString SpecHash(const Configs::Profile &profile, const QString &dialHost, const Configs::ScanTuneOptions &options) {
        Configs::ScanConfig config;
        config.tune = options;
        auto tune = config.ToJson().value(QLatin1String("tune")).toObject();
        // Pacing and caps only bound the run; the planner continues under any values.
        for (const char *key : {"perMinute", "maxAttempts", "maxMinutes"}) tune.remove(QLatin1String(key));
        const auto &settings = *Configs::dataManager->settingsRepo;
        QJsonObject object;
        object["type"] = profile.type;
        object["outbound"] = profile.outbound != nullptr ? profile.outbound->ExportToJson() : QJsonObject();
        object["host"] = dialHost;
        object["options"] = tune;
        object["utlsFingerprint"] = settings.utlsFingerprint;
        object["fragmentDefaultOn"] = settings.fragment_default_on;
        object["fragmentImplementation"] = settings.fragment_implementation;
        object["tlsTricksDefaultOn"] = settings.tls_tricks_default_on;
        const auto text = QJsonDocument(object).toJson(QJsonDocument::Compact);
        return QString::fromLatin1(QCryptographicHash::hash(text, QCryptographicHash::Sha1).toHex());
    }

    QString MtuTestUrl(const Configs::ScanTuneOptions &options) {
        if (const auto url = options.mtuUrl.trimmed(); !url.isEmpty()) return url;
        return QStringLiteral("https://speed.cloudflare.com/__down?bytes=%1").arg(options.mtuBytes);
    }
} // namespace Scanner::Tune
