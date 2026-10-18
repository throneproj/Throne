#pragma once

#include <QCommandLineParser>
#include <QDir>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include "include/engine/app/Bootstrap.h"
#include "include/global/Logger.hpp"

// Command line and environment of "Throne --headless" and its client modes.
namespace Engine::HeadlessCli {
    enum class RestartMode { Auto, Exit, Exec };

    enum class ClientMode { None, Health, PrintConnection, Check };

    // Long names; one or two dashes both parse. HeadlessApi::ParseOverlay reads the api-* ones from the same parser.
    inline constexpr char kDataDir[] = "data-dir"; // alias "appdata"
    inline constexpr char kRestartMode[] = "restart-mode";
    inline constexpr char kNoResume[] = "no-resume";
    inline constexpr char kNoKillSwitch[] = "no-kill-switch";
    inline constexpr char kLogFormat[] = "log-format";
    inline constexpr char kApiListen[] = "api-listen";
    inline constexpr char kApiKeyFile[] = "api-key-file";
    inline constexpr char kApiAdminLan[] = "api-admin-lan";
    inline constexpr char kApiAllow[] = "api-allow";
    inline constexpr char kHealth[] = "health";
    inline constexpr char kPrintConnection[] = "print-connection";
    inline constexpr char kCheck[] = "check";
    // Windows only: run under the SCM (elsewhere a usage error).
    inline constexpr char kService[] = "service";
    inline constexpr char kDebug[] = "debug";

    struct LaunchOptions {
        // Absolute and clean against the launch CWD; empty: the default chain.
        QString dataDir;
        // "--data-dir" | "THRONE_DATA_DIR" | "STATE_DIRECTORY" | "root default" | "ProgramData default" | "user config";
        // filled by ResolveDataDir.
        QString dataDirSource;
        // --restart-mode / THRONE_RESTART_MODE.
        RestartMode restartMode = RestartMode::Auto;
        // !--no-resume and THRONE_RESUME not 0/false/no/off.
        bool resume = true;
        // --no-kill-switch, or THRONE_KILL_SWITCH 0/false/no/off: RuntimeOptions::killSwitchOverride.
        bool noKillSwitch = false;
        // --service (Windows): started by the SCM; the data dir defaults to %ProgramData%\Throne and restarts always exit.
        bool service = false;
        // --log-format / THRONE_LOG_FORMAT.
        Logging::StdFormat logFormat = Logging::StdFormat::Auto;
        ClientMode client = ClientMode::None;
        bool debug = false;
        // "--headless" | "THRONE_HEADLESS" | "--service" | "client mode"; logged at boot.
        QString trigger;
        // For Bootstrap::OpenDataDir: {} or {"-debug"}.
        QStringList openDataDirArgs;
    };

    enum class ParseOutcome { Run, Help, Version, Usage };

    // Sets ParseAsLongOptions before adding anything; --headless is hidden; --service only on Windows; no --console.
    void AddOptions(QCommandLineParser &parser);

    // parse() only, never process(). Usage: an unknown option, a missing or bad value (flag or env), any positional argument,
    // or more than one client mode. macOS -psn_* arguments are dropped first.
    ParseOutcome Parse(QCommandLineParser &parser, const QStringList &arguments, const QProcessEnvironment &env,
                       const QDir &launchCwd, const Bootstrap::Launch &launch, LaunchOptions *out, QString *message);

    // helpText() plus the exit-code table, plain English.
    QString HelpText(const QCommandLineParser &parser);

    // Chain: option > THRONE_DATA_DIR > the first ':' entry of STATE_DIRECTORY > as root: /var/lib/throne (Linux),
    // /Library/Application Support/Throne (macOS) > AppConfigLocation (HOME empty or "/": an error). Never the app dir.
    // Windows: option > THRONE_DATA_DIR > --service or LocalSystem: %ProgramData%\Throne (SecureDataDir; refused -> 78) >
    // AppConfigLocation; no STATE_DIRECTORY. An explicit dir that Users can write is logged as a warning.
    // create: mkpath wd and wd/config, 0700 on the dirs it created, a write probe; false (client modes) touches nothing.
    // Fills DataDir{wd, useAppdata = true, appdataDir = wd, configDir} and options->dataDirSource. 0 or ExitCode::Config.
    int ResolveDataDir(LaunchOptions *options, const QProcessEnvironment &env, bool create, Bootstrap::DataDir *out,
                       QString *error);

    // bootParentPid is captured at RunHeadless entry; *why goes into the boot line.
    RestartMode EffectiveRestartMode(RestartMode requested, const QProcessEnvironment &env, qint64 bootParentPid, QString *why);

    QString RestartModeName(RestartMode mode);
} // namespace Engine::HeadlessCli
