#include "include/engine/services/SettingsRegistry.h"

#include <QHostAddress>
#include <QJsonValue>
#include <QList>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "include/api/remote/Overlay.hpp"
#include "include/configs/generate.h"
#include "include/database/SettingsSchema.h"
#include "include/engine/EditLocks.h"
#include "include/engine/ThreadCheck.h"
#include "include/engine/services/SettingsService.h"
#include "include/global/Configs.hpp"
#include "include/global/Validation.hpp"

namespace Engine::SettingsRegistryImpl {
    using Settings = Configs::SettingsValues;
    namespace Schema = Configs::SettingsSchema;

    constexpr int kMinAutoUpdate = 30;
    constexpr int kMaxAutoUpdate = 525600;

    template <typename T>
    const Schema::Key *schemaKey(T Settings::*member) {
        for (const auto &key : Schema::Keys()) {
            if (const auto *held = std::get_if<T Settings::*>(&key.member); held != nullptr && *held == member) return &key;
        }
        return nullptr;
    }

    template <typename T>
    struct Write {
        QString key;
        T Settings::*member;
        T value;
    };

    // The member writes of one PUT, each under the API key that made it; value() reads them over the stored settings.
    class Patch {
    public:
        explicit Patch(const Settings &settings) : settings_(settings) {}

        template <typename T>
        void set(const QString &key, T Settings::*member, std::type_identity_t<T> value) {
            auto &writes = std::get<QList<Write<T>>>(writes_);
            writes.removeIf([member](const Write<T> &write) { return write.member == member; });
            writes.append({key, member, std::move(value)});
            if (!keys_.contains(key)) keys_ << key;
        }

        template <typename T>
        T value(T Settings::*member) const {
            for (const auto &write : std::get<QList<Write<T>>>(writes_)) {
                if (write.member == member) return write.value;
            }
            return settings_.*member;
        }

        template <typename T>
        bool changes(T Settings::*member) const {
            return value(member) != settings_.*member;
        }

        bool touches(const QString &key) const {
            return keys_.contains(key);
        }

        // The stored settings with every write made.
        Settings applied() const {
            Settings values = settings_;
            forEachWrite([&values](const auto &write) { values.*write.member = write.value; });
            return values;
        }

        // The database keys of the written members.
        QStringList dbKeys() const {
            QStringList keys;
            forEachWrite([&keys](const auto &write) {
                if (const auto *key = schemaKey(write.member)) keys << key->name;
            });
            return keys;
        }

        // The keys, in the order they were first set, whose writes changed one of the database keys in changed.
        QStringList keysChanging(const QStringList &changed) const {
            QSet<QString> hit;
            forEachWrite([&changed, &hit](const auto &write) {
                const auto *key = schemaKey(write.member);
                if (key != nullptr && changed.contains(key->name)) hit.insert(write.key);
            });
            QStringList keys;
            for (const auto &key : keys_) {
                if (hit.contains(key)) keys << key;
            }
            return keys;
        }

    private:
        template <typename Visit>
        void forEachWrite(const Visit &visit) const {
            const auto visitAll = [&visit](const auto &writes) {
                for (const auto &write : writes) visit(write);
            };
            std::apply([&visitAll](const auto &...writes) { (visitAll(writes), ...); }, writes_);
        }

        const Settings &settings_;
        std::tuple<QList<Write<bool>>, QList<Write<int>>, QList<Write<QString>>> writes_;
        QStringList keys_;
    };

    struct Key {
        QString name;
        std::function<QJsonValue(const Settings &)> get;
        // check and set are called only for a value that differs from get(); check returns the problem naming the key, or empty.
        std::function<QString(const QJsonValue &)> check;
        std::function<void(const QJsonValue &, Patch *)> set;
    };

    ServiceError badRequest(const QString &message) {
        return {400, QStringLiteral("bad_request"), message};
    }

    // Like the Router's readInteger: a JSON number without a fraction that a double holds exactly.
    bool wholeNumber(const QJsonValue &value, qint64 *out) {
        constexpr double kExactLimit = 9007199254740992.0;
        const double number = value.toDouble();
        if (!value.isDouble() || std::trunc(number) != number || std::abs(number) > kExactLimit) return false;
        *out = static_cast<qint64>(number);
        return true;
    }

    QString integerProblem(const QString &name, const QJsonValue &value, qint64 min, qint64 max) {
        qint64 number = 0;
        if (!wholeNumber(value, &number)) return QStringLiteral("\"%1\" must be an integer").arg(name);
        if (number < min || number > max) {
            return QStringLiteral("\"%1\" must be from %2 to %3").arg(name, QString::number(min), QString::number(max));
        }
        return {};
    }

    Key boolKey(const QString &name, bool Settings::*member, bool inverted = false) {
        return {name,
                [member, inverted](const Settings &settings) { return QJsonValue(inverted ? !(settings.*member) : settings.*member); },
                [name](const QJsonValue &value) {
                    return value.isBool() ? QString() : QStringLiteral("\"%1\" must be true or false").arg(name);
                },
                [name, member, inverted](const QJsonValue &value, Patch *patch) {
                    patch->set(name, member, inverted ? !value.toBool() : value.toBool());
                }};
    }

    // Accepts the schema's range, which the settings windows enforce too.
    Key intKey(const QString &name, int Settings::*member) {
        const auto *key = schemaKey(member);
        const auto range = key != nullptr && key->range ? *key->range
                                                       : Schema::IntRange{std::numeric_limits<int>::min(), std::numeric_limits<int>::max()};
        return {name,
                [member](const Settings &settings) { return QJsonValue(settings.*member); },
                [name, range](const QJsonValue &value) { return integerProblem(name, value, range.min, range.max); },
                [name, member](const QJsonValue &value, Patch *patch) {
                    patch->set(name, member, static_cast<int>(value.toDouble()));
                }};
    }

    // rule completes "\"<name>\" ...". Stored trimmed, as the dialogs store them.
    Key stringKey(const QString &name, QString Settings::*member, std::function<bool(const QString &)> valid, const QString &rule) {
        return {name,
                [member](const Settings &settings) { return QJsonValue(settings.*member); },
                [name, valid = std::move(valid), rule](const QJsonValue &value) {
                    if (!value.isString()) return QStringLiteral("\"%1\" must be a string").arg(name);
                    return valid(value.toString().trimmed()) ? QString() : QStringLiteral("\"%1\" %2").arg(name, rule);
                },
                [name, member](const QJsonValue &value, Patch *patch) { patch->set(name, member, value.toString().trimmed()); }};
    }

    Key enumKey(const QString &name, QString Settings::*member, const QStringList &allowed) {
        return stringKey(name, member, [allowed](const QString &text) { return allowed.contains(text); },
                         QStringLiteral("must be one of: %1").arg(allowed.join(QStringLiteral(", "))));
    }

    // Stored negative = off, keeping the minutes for when it is turned back on; a magnitude below 30 counts as off.
    int autoUpdateOff(int stored, int fallbackMinutes) {
        if (stored >= kMinAutoUpdate) return -stored;
        return stored <= -kMinAutoUpdate ? stored : -fallbackMinutes;
    }

    // API minutes: 0 = off, else 30..525600; 1..29 would silently read as off, so they are refused.
    Key autoUpdateKey(const QString &name, int Settings::*member, int fallbackMinutes) {
        return {name,
                [member](const Settings &settings) {
                    return QJsonValue(settings.*member >= kMinAutoUpdate ? settings.*member : 0);
                },
                [name](const QJsonValue &value) {
                    qint64 minutes = 0;
                    if (!wholeNumber(value, &minutes)) return QStringLiteral("\"%1\" must be an integer").arg(name);
                    if (minutes != 0 && (minutes < kMinAutoUpdate || minutes > kMaxAutoUpdate)) {
                        return QStringLiteral("\"%1\" must be 0 (off) or from %2 to %3 minutes")
                            .arg(name, QString::number(kMinAutoUpdate), QString::number(kMaxAutoUpdate));
                    }
                    return QString();
                },
                [name, member, fallbackMinutes](const QJsonValue &value, Patch *patch) {
                    const int minutes = static_cast<int>(value.toDouble());
                    patch->set(name, member, minutes != 0 ? minutes : autoUpdateOff(patch->value(member), fallbackMinutes));
                }};
    }

    // API 0 = off. Stored <= 0 is off; turning it off stores the port negated, as the defaults do, to keep it for later.
    Key portOrOffKey(const QString &name, int Settings::*member) {
        return {name,
                [member](const Settings &settings) { return QJsonValue(settings.*member > 0 ? settings.*member : 0); },
                [name](const QJsonValue &value) {
                    qint64 port = 0;
                    if (!wholeNumber(value, &port)) return QStringLiteral("\"%1\" must be an integer").arg(name);
                    if (port < 0 || port > 65535) return QStringLiteral("\"%1\" must be 0 (off) or from 1 to 65535").arg(name);
                    return QString();
                },
                [name, member](const QJsonValue &value, Patch *patch) {
                    const int port = static_cast<int>(value.toDouble());
                    const int stored = patch->value(member);
                    patch->set(name, member, port != 0 ? port : (stored > 0 ? -stored : stored));
                }};
    }

    const QList<Key> &keys() {
        static const QList<Key> table = [] {
            const QStringList http = {QStringLiteral("http"), QStringLiteral("https")};
            QList<Key> list;
            list << boolKey(QStringLiteral("remember_enable"), &Settings::remember_enable);
            list << enumKey(QStringLiteral("log_level"), &Settings::log_level,
                            QStringLiteral("trace debug info warn error fatal panic").split(QLatin1Char(' ')));
            list << enumKey(QStringLiteral("xray_log_level"), &Settings::xray_log_level, Configs::Xray::XrayLogLevels);
            list << boolKey(QStringLiteral("inbound_enabled"), &Settings::disable_mixed_inbound, true);
            list << stringKey(QStringLiteral("inbound_address"), &Settings::inbound_address,
                              [](const QString &text) { return Validation::Ip(text).isEmpty(); },
                              QStringLiteral("must be an IPv4 or IPv6 address, such as 127.0.0.1 or ::"));

            auto inboundPort = intKey(QStringLiteral("inbound_port"), &Settings::inbound_socks_port);
            // A random port is re-picked at every launch, which would discard the one set here.
            inboundPort.set = [setPort = inboundPort.set](const QJsonValue &value, Patch *patch) {
                setPort(value, patch);
                patch->set(QStringLiteral("inbound_port"), &Settings::random_inbound_port, false);
            };
            list << inboundPort;

            list << stringKey(QStringLiteral("test_url"), &Settings::test_latency_url,
                              [http](const QString &text) { return Validation::Url(text, http, Validation::Empty::Required).isEmpty(); },
                              QStringLiteral("must be an http:// or https:// URL with a host"));
            list << intKey(QStringLiteral("test_concurrency"), &Settings::test_concurrent);
            list << intKey(QStringLiteral("test_timeout_ms"), &Settings::url_test_timeout_ms);
            list << autoUpdateKey(QStringLiteral("sub_auto_update"), &Settings::sub_auto_update, 30);
            list << boolKey(QStringLiteral("sub_respect_server_interval"), &Settings::sub_respect_server_interval);
            list << autoUpdateKey(QStringLiteral("route_auto_update"), &Settings::route_auto_update, 1440);
            list << boolKey(QStringLiteral("kill_switch"), &Settings::kill_switch);
            list << boolKey(QStringLiteral("tun_strict_route"), &Settings::vpn_strict_route);
#ifdef Q_OS_LINUX
            list << boolKey(QStringLiteral("tun_auto_redirect"), &Settings::vpn_auto_redirect);
#endif
            list << intKey(QStringLiteral("tun_mtu"), &Settings::vpn_mtu);
            list << boolKey(QStringLiteral("tun_ipv6"), &Settings::vpn_ipv6);
            list << stringKey(QStringLiteral("tun_ipv4_cidr"), &Settings::vpn_tun_ipv4_cidr,
                              [](const QString &text) { return Validation::Cidr(text, Validation::Family::V4).isEmpty(); },
                              QStringLiteral("must be an IPv4 address with a prefix length, such as 172.19.0.1/24"));
            list << stringKey(QStringLiteral("tun_ipv6_cidr"), &Settings::vpn_tun_ipv6_cidr,
                              [](const QString &text) { return Validation::Cidr(text, Validation::Family::V6).isEmpty(); },
                              QStringLiteral("must be an IPv6 address with a prefix length, such as fdfe:dcba:9876::1/96"));

            list << intKey(QStringLiteral("dns_in_port"), &Settings::core_dns_in_port);
            list << boolKey(QStringLiteral("dns_in_lan"), &Settings::core_dns_in_lan);
            list << portOrOffKey(QStringLiteral("clash_api_port"), &Settings::core_box_clash_api);
            list << stringKey(QStringLiteral("clash_listen_addr"), &Settings::core_box_clash_listen_addr,
                              [](const QString &text) { return Validation::Host(text, Validation::Empty::Allowed).isEmpty(); },
                              QStringLiteral("must be empty (127.0.0.1) or a host name or IP address, without a scheme, port or spaces"));
            list << portOrOffKey(QStringLiteral("singbox_api_port"), &Settings::core_box_api_port);

            const auto dnsAddress = [](const QString &text) { return Configs::DnsAddressListProblem(text).isEmpty(); };
            const auto dnsRule = QStringLiteral("must be one or more comma-separated DNS addresses, each local, dhcp://auto, dhcp://<interface>, "
                                                "udp|tcp|tls|quic://host[:port], https|h3://host[:port][/path] or host[:port]; "
                                                "several are queried together and the first answer wins");
            list << stringKey(QStringLiteral("remote_dns"), &Settings::remote_dns, dnsAddress, dnsRule);
            list << stringKey(QStringLiteral("direct_dns"), &Settings::direct_dns, dnsAddress, dnsRule);

            list << boolKey(QStringLiteral("ntp_enabled"), &Settings::enable_ntp);
            list << stringKey(QStringLiteral("ntp_server"), &Settings::ntp_server_address,
                              [](const QString &text) { return Validation::Host(text, Validation::Empty::Allowed).isEmpty(); },
                              QStringLiteral("must be empty or a host name or IP address, without a scheme, port or spaces"));
            list << stringKey(QStringLiteral("ntp_interval"), &Settings::ntp_interval,
                              [](const QString &text) { return Validation::Duration(text, {Validation::Empty::Required}).isEmpty(); },
                              QStringLiteral("must be a duration such as 30m or 1h (units ns, us, ms, s, m, h, d)"));
            return list;
        }();
        return table;
    }

    const Key *findKey(const QString &name) {
        for (const auto &key : keys()) {
            if (key.name == name) return &key;
        }
        return nullptr;
    }

    std::optional<int> mixedInboundPort(const Settings &values) {
        if (values.disable_mixed_inbound || values.random_inbound_port || values.inbound_socks_port <= 0) return std::nullopt;
        return values.inbound_socks_port;
    }

    std::optional<int> listenerPort(int port) {
        return port > 0 ? std::optional<int>(port) : std::nullopt;
    }

    // EffectiveConfig() once values are saved: only the GUI edits remote_api_*, and it never installs an overlay.
    std::optional<int> throneApiPort(const Settings &values) {
        if (RemoteApi::CurrentOverlay().headless) {
            const auto api = RemoteApi::EffectiveConfig().config;
            return api.enabled ? std::optional<int>(api.port) : std::nullopt;
        }
        if (!values.remote_api_enable) return std::nullopt;
        return IsValidPort(values.remote_api_port) ? values.remote_api_port : RemoteApi::kDefaultPort;
    }

    struct PortUser {
        QString dbKey;
        // Empty when the API cannot set it.
        QString apiKey;
        QString name;
        // The port while in use, before and after the commit.
        std::optional<int> before;
        std::optional<int> after;
        // Turned back on at the port it kept while off.
        bool reenabled = false;
    };

    PortUser portUser(const QString &dbKey, const QString &apiKey, const QString &name, std::optional<int> before, std::optional<int> after,
                      bool portKept) {
        return {dbKey, apiKey, name, before, after, !before && after && portKept};
    }

    std::optional<ServiceError> portConflict(const Settings &current, const Settings &next, bool apiNames) {
        const QList<PortUser> users = {
            portUser(QStringLiteral("inbound_socks_port"), QStringLiteral("inbound_port"), QStringLiteral("the mixed inbound"),
                     mixedInboundPort(current), mixedInboundPort(next), current.inbound_socks_port == next.inbound_socks_port),
            portUser(QStringLiteral("core_dns_in_port"), QStringLiteral("dns_in_port"), QStringLiteral("the core's DNS server"),
                     listenerPort(current.core_dns_in_port), listenerPort(next.core_dns_in_port),
                     current.core_dns_in_port == next.core_dns_in_port),
            portUser(QStringLiteral("core_box_clash_api"), QStringLiteral("clash_api_port"), QStringLiteral("the Clash API"),
                     listenerPort(current.core_box_clash_api), listenerPort(next.core_box_clash_api),
                     current.core_box_clash_api == next.core_box_clash_api),
            portUser(QStringLiteral("core_box_api_port"), QStringLiteral("singbox_api_port"), QStringLiteral("the sing-box API"),
                     listenerPort(current.core_box_api_port), listenerPort(next.core_box_api_port),
                     current.core_box_api_port == next.core_box_api_port),
            portUser(QStringLiteral("remote_api_port"), {}, QStringLiteral("the Throne API"), throneApiPort(current), throneApiPort(next),
                     current.remote_api_port == next.remote_api_port),
        };
        const auto keyOf = [apiNames](const PortUser &user) { return apiNames && !user.apiKey.isEmpty() ? user.apiKey : user.dbKey; };
        const auto labelOf = [apiNames, &keyOf](const PortUser &user) {
            return apiNames && user.apiKey.isEmpty() ? user.name : QStringLiteral("%1 (%2)").arg(user.name, keyOf(user));
        };
        for (const auto &user : users) {
            // While a listener was off, others could take its port unchecked, so turning it back on is checked too.
            if (!user.after || user.after == user.before) continue;
            const auto holder = std::find_if(users.cbegin(), users.cend(), [&user](const PortUser &other) {
                return other.dbKey != user.dbKey && other.after == user.after;
            });
            if (holder == users.cend()) continue;
            const auto port = QString::number(*user.after);
            if (apiNames) {
                const auto message = user.reenabled && user.dbKey == QLatin1String("inbound_socks_port")
                                         ? QStringLiteral("\"inbound_enabled\" would turn the mixed inbound on at port %1, which %2 already "
                                                          "uses; choose a different inbound_port")
                                               .arg(port, labelOf(*holder))
                                         : QStringLiteral("\"%1\" %2 is already used by %3; choose a different port")
                                               .arg(keyOf(user), port, labelOf(*holder));
                return badRequest(message);
            }
            const auto message = user.reenabled ? QStringLiteral("Turning %1 on at port %2 would clash with %3; choose a different %4")
                                                      .arg(user.name, port, labelOf(*holder), user.dbKey)
                                                : QStringLiteral("\"%1\" %2 is already used by %3; choose a different port")
                                                      .arg(user.dbKey, port, labelOf(*holder));
            return ServiceError{400, QStringLiteral("port_conflict"), message};
        }
        return std::nullopt;
    }

    // The Clash API secret is set only in the GUI, so the API never opens the Clash API to other hosts without one.
    QString clashExposure(const Patch &pending, const Settings &settings) {
        if (!pending.touches(QStringLiteral("clash_api_port")) && !pending.touches(QStringLiteral("clash_listen_addr"))) return {};
        if (pending.value(&Settings::core_box_clash_api) <= 0 || !settings.core_box_clash_api_secret.isEmpty()) return {};
        // Already on at this address: moving its port opens nothing new.
        if (settings.core_box_clash_api > 0 && !pending.changes(&Settings::core_box_clash_listen_addr)) return {};
        QString host = pending.value(&Settings::core_box_clash_listen_addr);
        if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']'))) host = host.mid(1, host.size() - 2);
        if (host.isEmpty() || host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0 || QHostAddress(host).isLoopback()) {
            return {};
        }
        return QStringLiteral("\"clash_listen_addr\" %1 would open the Clash API to other hosts without a secret; keep it on 127.0.0.1, "
                              "or set the Clash API secret in Throne's settings first")
            .arg(host);
    }
} // namespace Engine::SettingsRegistryImpl

namespace Engine::SettingsRegistry {
    QJsonObject Values() {
        const auto &settings = *Configs::dataManager->settingsRepo;
        QJsonObject values;
        for (const auto &key : SettingsRegistryImpl::keys()) values.insert(key.name, key.get(settings));
        return values;
    }

    std::optional<ServiceError> Put(const QJsonObject &patch, const Context &context, PutResult *result) {
        using Settings = SettingsRegistryImpl::Settings;
        ENGINE_CHECK_MAIN_THREAD();
        if (result != nullptr) *result = {};
        const auto &settings = *Configs::dataManager->settingsRepo;
        if (settings.prepare_exit || settings.noSave) {
            return ServiceError{503, QStringLiteral("exiting"), QStringLiteral("Throne is shutting down")};
        }
        if (IsBeingEdited(EditTarget::Settings)) {
            return ServiceError{409, QStringLiteral("editing"), QStringLiteral("Throne's settings window is open; close it and try again")};
        }

        SettingsRegistryImpl::Patch pending(settings);
        for (auto it = patch.constBegin(); it != patch.constEnd(); ++it) {
            const auto *key = SettingsRegistryImpl::findKey(it.key());
            if (key == nullptr) return SettingsRegistryImpl::badRequest(QStringLiteral("Unknown setting \"%1\"").arg(it.key()));
            const QJsonValue value = it.value();
            // GET returns stored values even when they are invalid, so one sent back unchanged passes and writes nothing.
            if (value.isNull() || value == key->get(settings)) continue;
            if (const auto problem = key->check(value); !problem.isEmpty()) return SettingsRegistryImpl::badRequest(problem);
            key->set(value, &pending);
        }

        if (auto conflict = PortConflict(settings, pending.applied(), true)) return conflict;
        if (const auto problem = SettingsRegistryImpl::clashExposure(pending, settings); !problem.isEmpty()) {
            return SettingsRegistryImpl::badRequest(problem);
        }
        if (context.systemProxyOn && pending.changes(&Settings::disable_mixed_inbound) && pending.value(&Settings::disable_mixed_inbound)) {
            return ServiceError{409, QStringLiteral("mixed_inbound_disabled"),
                                QStringLiteral("The system proxy needs Throne's mixed inbound; turn the system proxy off first")};
        }
        // The core refuses an empty NTP interval, which would fail every start.
        if ((pending.touches(QStringLiteral("ntp_enabled")) || pending.touches(QStringLiteral("ntp_interval"))) &&
            pending.value(&Settings::enable_ntp) && pending.value(&Settings::ntp_interval).isEmpty()) {
            pending.set(QStringLiteral("ntp_interval"), &Settings::ntp_interval, QStringLiteral("30m"));
        }

        SettingsService::ApplyResult applied;
        if (auto refused = SettingsService::Apply(pending.applied(), pending.dbKeys(), {}, &applied)) return refused;
        if (result != nullptr) {
            result->changed = pending.keysChanging(applied.changed);
            result->changed.sort();
        }
        return std::nullopt;
    }

    std::optional<ServiceError> PortConflict(const Configs::SettingsValues &current, const Configs::SettingsValues &next, bool apiNames) {
        return SettingsRegistryImpl::portConflict(current, next, apiNames);
    }

    const QStringList &HostKeys() {
        static const QStringList keys = Configs::SettingsSchema::KeysIn(Configs::SettingsSchema::Scope::Host);
        return keys;
    }
} // namespace Engine::SettingsRegistry
