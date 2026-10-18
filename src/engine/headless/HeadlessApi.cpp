#include "include/engine/headless/HeadlessApi.h"

#include <memory>
#include <optional>
#include <utility>

#include <QElapsedTimer>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "include/api/remote/Server.hpp"
#include "include/engine/app/ExitCodes.h"
#include "include/engine/headless/HeadlessCli.h"
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

namespace Engine::HeadlessApi {
    namespace {
        constexpr int kHeadlessApiRetryAttempts = 30;
        constexpr int kHeadlessApiRetryIntervalMs = 1000;
        constexpr int kHeadlessApiQuietRetryMs = 30 * 1000;
        constexpr int kHeadlessApiWatchIntervalMs = 20 * 1000;
        constexpr int kHeadlessApiProbeMs = 1000;

        struct HeadlessApiState {
            bool retrying = false;
            // Bind() left a default or settings listener that failed to bind; StartQuietRetry picks it up.
            bool quietRetry = false;
            // The explicit listener's address left every interface; the listener stays bound to it.
            bool addressMissing = false;
            // Why the key could not be loaded or created; the HEALTH error while the API stays off for it.
            QString keyError;
        };

        HeadlessApiState &headlessApiState() {
            static HeadlessApiState state;
            return state;
        }

        bool headlessApiParseBool(const QString &text, bool *value) {
            const auto lower = text.trimmed().toLower();
            if (lower == QLatin1String("1") || lower == QLatin1String("true") || lower == QLatin1String("yes") || lower == QLatin1String("on")) {
                *value = true;
                return true;
            }
            if (lower == QLatin1String("0") || lower == QLatin1String("false") || lower == QLatin1String("no") || lower == QLatin1String("off")) {
                *value = false;
                return true;
            }
            return false;
        }

        struct HeadlessApiValue {
            QString text;
            // "--api-listen" or "THRONE_API_LISTEN", for messages.
            QString origin;
            RemoteApi::ConfigSource source = RemoteApi::ConfigSource::Default;
        };

        // The flag's value, else a non-empty environment variable (a set but empty one too with keepEmpty).
        std::optional<HeadlessApiValue> headlessApiLookup(const QCommandLineParser &parser, const QProcessEnvironment &env,
                                                          const char *option, const char *variable, bool keepEmpty = false) {
            const auto name = QString::fromLatin1(option);
            if (parser.isSet(name)) return HeadlessApiValue{parser.value(name), QStringLiteral("--") + name, RemoteApi::ConfigSource::Flags};
            const auto envName = QString::fromLatin1(variable);
            const auto text = env.value(envName);
            if (text.isEmpty() && !(keepEmpty && env.contains(envName))) return std::nullopt;
            return HeadlessApiValue{text, envName, RemoteApi::ConfigSource::Environment};
        }

        QString headlessApiSourceName(RemoteApi::ConfigSource source) {
            switch (source) {
                case RemoteApi::ConfigSource::Flags: return QStringLiteral("flags");
                case RemoteApi::ConfigSource::Environment: return QStringLiteral("env");
                case RemoteApi::ConfigSource::Default: return QStringLiteral("default");
                case RemoteApi::ConfigSource::Settings: break;
            }
            return QStringLiteral("settings");
        }

        QHostAddress headlessApiBindAddress(const RemoteApi::Config &config) {
            if (!config.address.isNull()) return config.address;
            return config.lan ? QHostAddress(QHostAddress::Any) : QHostAddress(QHostAddress::LocalHost);
        }

        // RemoteApi::ParseListen reads it back: "*:P" (dual stack), "[V6]:P", "V4:P".
        QString headlessApiListenText(const QHostAddress &address, quint16 port) {
            if (address == QHostAddress::Any) return QStringLiteral("*:%1").arg(port);
            if (address.protocol() == QAbstractSocket::IPv6Protocol) return QStringLiteral("[%1]:%2").arg(address.toString()).arg(port);
            return QStringLiteral("%1:%2").arg(address.toString()).arg(port);
        }

        QString headlessApiDescribe(const RemoteApi::Effective &effective) {
            const auto key = effective.keyFile.isEmpty() ? QStringLiteral("the settings") : effective.keyFile;
            return QStringLiteral("source %1, key from %2").arg(headlessApiSourceName(effective.source), key);
        }

        bool headlessApiAddressPresent(const QHostAddress &address) {
            if (address.isNull() || address == QHostAddress::Any || address == QHostAddress::AnyIPv4 || address == QHostAddress::AnyIPv6) return true;
            auto wanted = address;
            wanted.setScopeId(QString());
            for (auto candidate : QNetworkInterface::allAddresses()) {
                candidate.setScopeId(QString());
                if (candidate.isEqual(wanted)) return true;
            }
            return false;
        }

        // One address that can leave the host; loopback never does, and 127.0.0.2.. are not listed per interface.
        bool headlessApiWatchable(const QHostAddress &address) {
            return !address.isNull() && address != QHostAddress::Any && address != QHostAddress::AnyIPv4
                   && address != QHostAddress::AnyIPv6 && !address.isLoopback();
        }

        QHostAddress headlessApiProbeTarget(const QHostAddress &bindAddress) {
            if (bindAddress == QHostAddress::Any || bindAddress == QHostAddress::AnyIPv4) return QHostAddress(QHostAddress::LocalHost);
            if (bindAddress == QHostAddress::AnyIPv6) return QHostAddress(QHostAddress::LocalHostIPv6);
            return bindAddress;
        }

        // A Throne API speaks first: exactly kServerHelloSize random bytes right after the accept, then it waits for the client.
        bool headlessApiThroneAnswers(const QHostAddress &bindAddress, quint16 port) {
            QTcpSocket socket;
            socket.setProxy(QNetworkProxy::NoProxy);
            socket.connectToHost(headlessApiProbeTarget(bindAddress), port);
            if (!socket.waitForConnected(kHeadlessApiProbeMs)) {
                socket.abort();
                return false;
            }
            QElapsedTimer clock;
            clock.start();
            while (socket.bytesAvailable() < RemoteApi::kServerHelloSize) {
                const int left = int(kHeadlessApiProbeMs - clock.elapsed());
                if (left <= 0 || !socket.waitForReadyRead(left)) break;
            }
            const bool throne = socket.bytesAvailable() == RemoteApi::kServerHelloSize;
            socket.abort();
            return throne;
        }

        // A throwaway listener, closed at once: only a free port reaches the worker, which logs every failed bind.
        bool headlessApiPortFree(const QHostAddress &bindAddress, quint16 port) {
            QTcpServer probe;
            if (!probe.listen(bindAddress, port)) return false;
            probe.close();
            return true;
        }

        // Stricter than remote_api_allow, which drops bad entries.
        bool headlessApiValidAllow(const QString &text, QString *why) {
            static const QRegularExpression separators(QStringLiteral("[,\\s]+"));
            const auto entries = text.split(separators, Qt::SkipEmptyParts);
            if (entries.isEmpty()) {
                *why = QStringLiteral("it is empty; give at least one IPv4/IPv6 address or subnet");
                return false;
            }
            for (const auto &entry : entries) {
                if (RemoteApi::ParseAllowList(entry).size() != 1) {
                    *why = QStringLiteral("\"%1\" is not an IPv4/IPv6 address or subnet").arg(entry);
                    return false;
                }
            }
            return true;
        }
    } // namespace

    int ParseOverlay(const QCommandLineParser &parser, const QProcessEnvironment &env, const QDir &launchCwd,
                     RemoteApi::Overlay *out, QString *error) {
        out->headless = true;

        if (const auto listen = headlessApiLookup(parser, env, HeadlessCli::kApiListen, "THRONE_API_LISTEN")) {
            QString why;
            if (!RemoteApi::ParseListen(listen->text, out, &why)) {
                *error = QStringLiteral("%1: %2").arg(listen->origin, why);
                return ExitCode::Usage;
            }
            out->listenSource = listen->source;
        }

        if (const auto keyFile = headlessApiLookup(parser, env, HeadlessCli::kApiKeyFile, "THRONE_API_KEY_FILE")) {
            if (keyFile->text.trimmed().isEmpty()) {
                *error = QStringLiteral("%1 needs a file path").arg(keyFile->origin);
                return ExitCode::Usage;
            }
            out->keyFile = QDir::cleanPath(launchCwd.absoluteFilePath(keyFile->text));
            out->keyFileSource = keyFile->source;
        }

        if (parser.isSet(QString::fromLatin1(HeadlessCli::kApiAdminLan))) {
            out->adminLan = true;
        } else if (const auto text = env.value(QStringLiteral("THRONE_API_ADMIN_LAN")); !text.isEmpty()) {
            bool on = false;
            if (!headlessApiParseBool(text, &on)) {
                *error = QStringLiteral("THRONE_API_ADMIN_LAN: \"%1\" is not 1/true/yes/on or 0/false/no/off").arg(text);
                return ExitCode::Usage;
            }
            out->adminLan = on;
        }

        // An empty value (an unset shell or compose variable) must not silently widen the list to every address.
        if (const auto allow = headlessApiLookup(parser, env, HeadlessCli::kApiAllow, "THRONE_API_ALLOW", true)) {
            QString why;
            if (!headlessApiValidAllow(allow->text, &why)) {
                *error = QStringLiteral("%1: %2").arg(allow->origin, why);
                return ExitCode::Usage;
            }
            out->allow = allow->text;
        }
        return ExitCode::Ok;
    }

    int CheckExplicitKeyFile(RemoteApi::Overlay *overlay, QString *error) {
        if (overlay->keyFile.isEmpty() || (overlay->enabled.has_value() && !*overlay->enabled)) return ExitCode::Ok;
        const auto file = RemoteApi::LoadKeyFile(overlay->keyFile, false);
        if (file.error.isEmpty()) {
            overlay->key = file.key;
            return ExitCode::Ok;
        }
        *error = QStringLiteral("The API key file %1 is not usable: %2. Create one with: thronectl keygen -o %1").arg(overlay->keyFile, file.error);
        return ExitCode::Config;
    }

    void ResolveKeys(RemoteApi::Overlay overlay, const QString &configDir) {
        auto &state = headlessApiState();
        overlay.defaultKeyFile = QDir(configDir).absoluteFilePath(QStringLiteral("api.key"));
        const bool off = overlay.enabled.has_value() && !*overlay.enabled;
        if (!off && !overlay.keyFile.isEmpty()) {
            // Kept from CheckExplicitKeyFile: a second read could see a replaced file.
            const auto file = overlay.key.isEmpty() ? RemoteApi::LoadKeyFile(overlay.keyFile, false) : RemoteApi::KeyFile{overlay.key};
            if (file.error.isEmpty()) {
                overlay.key = file.key;
            } else {
                state.keyError = QStringLiteral("the key file %1 is not usable: %2").arg(overlay.keyFile, file.error);
                LOG_ERROR(QStringLiteral("[API] Not listening: %1").arg(state.keyError));
            }
        } else if (!off && !Configs::dataManager->settingsRepo->remote_api_enable) {
            const auto file = RemoteApi::LoadKeyFile(overlay.defaultKeyFile, true);
            if (file.error.isEmpty()) {
                overlay.key = file.key;
                if (file.replaced) {
                    LOG_WARN(QStringLiteral("[API] The key file %1 held no key (an interrupted first start?); created a new one. Clients "
                                            "need it again: Throne --headless --print-connection").arg(overlay.defaultKeyFile));
                } else if (file.created) {
                    LOG_INFO(QStringLiteral("[API] Created the key file %1 (owner only); Throne --headless --print-connection tells "
                                            "thronectl where to find it.").arg(overlay.defaultKeyFile));
                }
            } else {
                state.keyError = QStringLiteral("the key file %1 is not usable: %2; delete it to have a new key created at the next "
                                                "start, or write one with: thronectl keygen -o %1").arg(overlay.defaultKeyFile, file.error);
                LOG_WARN(QStringLiteral("[API] Not listening: %1").arg(state.keyError));
            }
        }
        RemoteApi::SetOverlay(overlay);
    }

    BindResult Bind() {
        const auto effective = RemoteApi::EffectiveConfig();
        const auto &config = effective.config;
        if (config.enabled && !config.lan && RemoteApi::CurrentOverlay().adminLan.value_or(false)) {
            LOG_WARN(QStringLiteral("[API] --api-admin-lan has no effect: the listener accepts connections from this device only."));
        }
        const auto result = RemoteApi::Server::instance()->applyAndWait(config);
        const auto bindAddress = result.address.isNull() ? headlessApiBindAddress(config) : result.address;
        const auto listen = headlessApiListenText(bindAddress, config.port);
        switch (result.state) {
            case RemoteApi::ApplyResult::State::Off:
                LOG_INFO(QStringLiteral("[API] Off (source %1)").arg(headlessApiSourceName(effective.source)));
                return {};
            case RemoteApi::ApplyResult::State::Listening:
                LOG_INFO(QStringLiteral("[API] Listening on %1 (%2)").arg(listen, headlessApiDescribe(effective)));
                return {};
            case RemoteApi::ApplyResult::State::KeyInvalid:
                // A key file's problem and its remedy were logged when it was read.
                LOG_WARN(QStringLiteral("[API] Not listening on %1: the key is missing or invalid (%2)%3")
                             .arg(listen, headlessApiDescribe(effective),
                                  effective.keyFile.isEmpty() ? QStringLiteral("; pass --api-key-file FILE (or THRONE_API_KEY_FILE)")
                                                              : QString()));
                return {};
            case RemoteApi::ApplyResult::State::BindFailed:
                break;
        }
        if (!effective.explicitListen) {
            // At boot the holder is the instance that won the port; a retry could take it in that one's restart gap, for good.
            if (result.error == QAbstractSocket::AddressInUseError && headlessApiThroneAnswers(bindAddress, config.port)) {
                LOG_WARN(QStringLiteral("[API] Not listening on %1 (%2): another Throne instance's API holds it; give this one "
                                        "--api-listen PORT (or THRONE_API_LISTEN)")
                             .arg(listen, headlessApiDescribe(effective)));
                return {};
            }
            headlessApiState().quietRetry = true;
            LOG_WARN(QStringLiteral("[API] Not listening on %1 (%2): %3; retrying quietly every %4 s. --api-listen PORT (or "
                                    "THRONE_API_LISTEN) picks another port")
                         .arg(listen, headlessApiDescribe(effective), result.text)
                         .arg(kHeadlessApiQuietRetryMs / 1000));
            return {};
        }
        if (result.error == QAbstractSocket::SocketAddressNotAvailableError) {
            headlessApiState().retrying = true;
            LOG_WARN(QStringLiteral("[API] %1 is not available yet; retrying for %2 s").arg(listen).arg(kHeadlessApiRetryAttempts));
            return {ExitCode::Ok, true};
        }
        LOG_ERROR(QStringLiteral("[API] Cannot listen on %1 (%2): %3").arg(listen, headlessApiDescribe(effective), result.text));
        return {ExitCode::Config, false};
    }

    void StartBindRetry(QObject *context, std::function<void()> bound, std::function<void(int exitCode)> giveUp) {
        auto *timer = new QTimer(context);
        timer->setInterval(kHeadlessApiRetryIntervalMs);
        auto attempts = std::make_shared<int>(0);
        QObject::connect(timer, &QTimer::timeout, context, [timer, attempts, bound = std::move(bound), giveUp = std::move(giveUp)] {
            const auto stop = [timer] {
                headlessApiState().retrying = false;
                timer->stop();
                timer->deleteLater();
            };
            if (Configs::dataManager->settingsRepo->prepare_exit) {
                stop();
                return;
            }
            const bool last = ++*attempts >= kHeadlessApiRetryAttempts;
            const auto effective = RemoteApi::EffectiveConfig();
            // Polling the interfaces keeps the worker from logging a failed bind every second.
            if (!last && !headlessApiAddressPresent(effective.config.address)) return;
            const auto result = RemoteApi::Server::instance()->applyAndWait(effective.config);
            const auto listen = headlessApiListenText(headlessApiBindAddress(effective.config), effective.config.port);
            if (result.state == RemoteApi::ApplyResult::State::Listening) {
                stop();
                LOG_INFO(QStringLiteral("[API] Listening on %1 after %2 s (%3)").arg(listen).arg(*attempts).arg(headlessApiDescribe(effective)));
                bound();
                return;
            }
            const bool notAvailable = result.state == RemoteApi::ApplyResult::State::BindFailed
                                      && result.error == QAbstractSocket::SocketAddressNotAvailableError;
            if (notAvailable && !last) return;
            stop();
            if (result.state == RemoteApi::ApplyResult::State::Off) return;
            LOG_ERROR(QStringLiteral("[API] Cannot listen on %1: %2").arg(listen, result.text));
            const bool refused = result.state == RemoteApi::ApplyResult::State::BindFailed && !notAvailable;
            giveUp(refused ? ExitCode::Config : ExitCode::Fatal);
        });
        timer->start();
    }

    void StartQuietRetry(QObject *context, std::function<void()> bound) {
        if (!headlessApiState().quietRetry) return;
        auto *timer = new QTimer(context);
        timer->setInterval(kHeadlessApiQuietRetryMs);
        auto freeTicks = std::make_shared<int>(0);
        QObject::connect(timer, &QTimer::timeout, context, [timer, freeTicks, bound = std::move(bound)] {
            const auto stop = [timer] {
                headlessApiState().quietRetry = false;
                timer->stop();
                timer->deleteLater();
            };
            if (Configs::dataManager->settingsRepo->prepare_exit) {
                stop();
                return;
            }
            auto *server = RemoteApi::Server::instance();
            const auto last = server->lastResult();
            // EngineHost::init re-applies once on its own; that one may have bound already.
            if (last.state == RemoteApi::ApplyResult::State::Listening) {
                stop();
                bound();
                return;
            }
            const auto effective = RemoteApi::EffectiveConfig();
            if (last.state != RemoteApi::ApplyResult::State::BindFailed || effective.explicitListen || !effective.config.enabled) {
                stop();
                return;
            }
            const auto address = headlessApiBindAddress(effective.config);
            const auto port = effective.config.port;
            const auto listen = headlessApiListenText(address, port);
            if (headlessApiThroneAnswers(address, port)) {
                LOG_WARN(QStringLiteral("[API] Another Throne instance's API holds %1; give this one --api-listen PORT (or "
                                        "THRONE_API_LISTEN)").arg(listen));
                stop();
                return;
            }
            if (!headlessApiPortFree(address, port)) {
                *freeTicks = 0;
                return;
            }
            // A port free on one tick only may be another instance's restart gap.
            if (++*freeTicks < 2) return;
            const auto result = server->applyAndWait(effective.config);
            if (result.state != RemoteApi::ApplyResult::State::Listening) {
                *freeTicks = 0;
                return;
            }
            stop();
            LOG_INFO(QStringLiteral("[API] Listening on %1 now that the port is free (%2)").arg(listen, headlessApiDescribe(effective)));
            bound();
        });
        timer->start();
    }

    void WatchExplicitAddress(QObject *context) {
        const auto effective = RemoteApi::EffectiveConfig();
        const auto address = effective.config.address;
        if (!effective.explicitListen || !effective.config.enabled || !headlessApiWatchable(address)) return;
        auto *timer = new QTimer(context);
        timer->setInterval(kHeadlessApiWatchIntervalMs);
        QObject::connect(timer, &QTimer::timeout, context, [timer, address] {
            auto &state = headlessApiState();
            if (Configs::dataManager->settingsRepo->prepare_exit) {
                timer->stop();
                timer->deleteLater();
                return;
            }
            // Before the bind retry succeeded there is nothing to watch yet.
            if (RemoteApi::Server::instance()->lastResult().state != RemoteApi::ApplyResult::State::Listening) return;
            const bool missing = !headlessApiAddressPresent(address);
            if (missing == state.addressMissing) return;
            state.addressMissing = missing;
            if (missing) {
                LOG_WARN(QStringLiteral("[API] %1 is no longer on any interface; the API is unreachable until it returns. To survive "
                                        "renumbering use --api-listen *:PORT with --api-allow <subnet>").arg(address.toString()));
            } else {
                LOG_INFO(QStringLiteral("[API] %1 is back on an interface; the API is reachable again").arg(address.toString()));
            }
        });
        timer->start();
    }

    QJsonObject StatusBlock() {
        const auto &state = headlessApiState();
        const auto effective = RemoteApi::EffectiveConfig();
        const auto result = RemoteApi::Server::instance()->lastResult();
        QJsonObject block{{"source", headlessApiSourceName(effective.source)}};
        QString name;
        switch (result.state) {
            case RemoteApi::ApplyResult::State::Off: name = QStringLiteral("off"); break;
            case RemoteApi::ApplyResult::State::Listening: name = QStringLiteral("listening"); break;
            case RemoteApi::ApplyResult::State::KeyInvalid: name = QStringLiteral("key_invalid"); break;
            case RemoteApi::ApplyResult::State::BindFailed: name = QStringLiteral("failed"); break;
        }
        if (state.retrying) name = QStringLiteral("retrying");
        const bool addressMissing = state.addressMissing && result.state == RemoteApi::ApplyResult::State::Listening;
        if (addressMissing) name = QStringLiteral("address_missing");
        block.insert(QStringLiteral("state"), name);
        if (result.state == RemoteApi::ApplyResult::State::Listening || result.state == RemoteApi::ApplyResult::State::BindFailed) {
            block.insert(QStringLiteral("listen"), headlessApiListenText(result.address, result.port));
        } else if (effective.config.enabled) {
            block.insert(QStringLiteral("listen"), headlessApiListenText(headlessApiBindAddress(effective.config), effective.config.port));
        }
        if (!effective.keyFile.isEmpty()) block.insert(QStringLiteral("key_file"), effective.keyFile);
        if (addressMissing) {
            block.insert(QStringLiteral("error"), QStringLiteral("%1 is no longer on any interface").arg(result.address.toString()));
        } else if (result.state == RemoteApi::ApplyResult::State::KeyInvalid && !state.keyError.isEmpty()) {
            block.insert(QStringLiteral("error"), state.keyError);
        } else if (result.state == RemoteApi::ApplyResult::State::KeyInvalid || result.state == RemoteApi::ApplyResult::State::BindFailed) {
            block.insert(QStringLiteral("error"), result.text);
        }
        return block;
    }
} // namespace Engine::HeadlessApi
