#pragma once

#include <functional>

#include <QCommandLineParser>
#include <QDir>
#include <QJsonObject>
#include <QProcessEnvironment>

#include "include/api/remote/Overlay.hpp"

class QObject;

// The Throne API of a headless process: the overlay from flags/env, its keys, the bind and its status.
namespace Engine::HeadlessApi {
    // --api-listen/--api-key-file/--api-admin-lan/--api-allow over THRONE_API_LISTEN/_KEY_FILE/_ADMIN_LAN/_ALLOW; flags win.
    // Bools 1|true|yes|on / 0|false|no|off; paths absolute against launchCwd. 0, or ExitCode::Usage with *error.
    int ParseOverlay(const QCommandLineParser &parser, const QProcessEnvironment &env, const QDir &launchCwd,
                     RemoteApi::Overlay *out, QString *error);

    // Before the lock, read-only: an explicit key file that is missing, not a regular file or invalid gives ExitCode::Config.
    // The key read stays in overlay->key, so a pipe is read once.
    int CheckExplicitKeyFile(RemoteApi::Overlay *overlay, QString *error);

    // After OpenDataDir: keeps an explicit key already read, or loads/creates <config>/api.key when the default listener
    // applies (a failure is logged and leaves the API off, never an exit). Then RemoteApi::SetOverlay().
    void ResolveKeys(RemoteApi::Overlay overlay, const QString &configDir);

    struct BindResult {
        int exitCode = 0;
        bool retry = false;
    };

    // After EngineHost::create(), before init(): applyAndWait(EffectiveConfig().config) and one log line, never the key.
    // An explicit listener failing with AddressInUse/Access/other: ExitCode::Config; with AddressNotAvailable: retry.
    // Default/settings listener failures are logged and kept for StatusBlock(); exitCode 0.
    BindResult Bind();

    // After exec() runs: 30 attempts 1 s apart; bound() on success, giveUp(ExitCode::Fatal) after the last. Stops on prepare_exit.
    void StartBindRetry(QObject *context, std::function<void()> bound, std::function<void(int exitCode)> giveUp);

    // After exec() runs, when a default or settings listener failed to bind: every 30 s, without exiting and without logging
    // each attempt, until it listens (bound(), one log line) or prepare_exit. A port another Throne API answers on is logged
    // once with the remedy and not retried. No-op unless Bind() left such a failure.
    void StartQuietRetry(QObject *context, std::function<void()> bound);

    // After exec() runs, for an explicit listener on one address (not a wildcard): every 20 s, observe only; a WARN with the
    // remedy when the address leaves every interface, an INFO when it is back; StatusBlock reports address_missing meanwhile.
    void WatchExplicitAddress(QObject *context);

    // HEALTH "api": {state: off|listening|key_invalid|failed|retrying|address_missing, listen: "127.0.0.1:9095",
    // source: settings|flags|env|default, key_file?, error?}. Never the key.
    QJsonObject StatusBlock();
} // namespace Engine::HeadlessApi
