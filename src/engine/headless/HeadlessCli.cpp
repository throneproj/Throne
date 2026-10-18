#include "include/engine/headless/HeadlessCli.h"

#include <QCommandLineOption>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>

#include "include/engine/app/ExitCodes.h"
#include "include/sys/ServiceManager.hpp"

#ifdef Q_OS_UNIX
#include <cerrno>
#include <cstring>
#include <utility>

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Engine::HeadlessCli {
    namespace {
        // 1 true, 0 false, -1 neither.
        int headlessCliParseBool(const QString &text) {
            const QString value = text.trimmed().toLower();
            if (value == "1" || value == "true" || value == "yes" || value == "on") return 1;
            if (value == "0" || value == "false" || value == "no" || value == "off") return 0;
            return -1;
        }

        bool headlessCliParseRestartMode(const QString &text, RestartMode *out) {
            const QString value = text.trimmed().toLower();
            if (value == "auto") *out = RestartMode::Auto;
            else if (value == "exit") *out = RestartMode::Exit;
            else if (value == "exec") *out = RestartMode::Exec;
            else return false;
            return true;
        }

        bool headlessCliParseLogFormat(const QString &text, Logging::StdFormat *out) {
            const QString value = text.trimmed().toLower();
            if (value == "auto") *out = Logging::StdFormat::Auto;
            else if (value == "journal") *out = Logging::StdFormat::Journal;
            else if (value == "plain") *out = Logging::StdFormat::Plain;
            else return false;
            return true;
        }

#ifndef Q_OS_WIN
        QString headlessCliRootDefault() {
#if defined(Q_OS_LINUX)
            if (::geteuid() == 0) return QStringLiteral("/var/lib/throne");
#elif defined(Q_OS_MACOS)
            if (::geteuid() == 0) return QStringLiteral("/Library/Application Support/Throne");
#endif
            return {};
        }
#endif

#ifdef Q_OS_UNIX
        QString headlessCliErrno(int error) {
            return QString::fromLocal8Bit(std::strerror(error));
        }

        // Every missing component is created 0700 at once, so no other user can slip a config/ or throne.db in first.
        bool headlessCliMakeDirs(const QString &path, QString *error) {
            QStringList missing;
            QString current = path;
            struct stat st{};
            while (::lstat(QFile::encodeName(current).constData(), &st) != 0) {
                if (const int code = errno; code != ENOENT) {
                    *error = QString("cannot inspect %1: %2").arg(current, headlessCliErrno(code));
                    return false;
                }
                missing.prepend(current);
                const QString parent = QFileInfo(current).path();
                if (parent == current) break;
                current = parent;
            }
            for (const QString &dir : std::as_const(missing)) {
                if (::mkdir(QFile::encodeName(dir).constData(), S_IRWXU) == 0) continue;
                if (const int code = errno; code != EEXIST) {
                    *error = QString("could not create %1: %2").arg(dir, headlessCliErrno(code));
                    return false;
                }
            }
            return true;
        }

        // A user private group (umask 002 on Fedora, Ubuntu): no one but the owner is in it, as a listed member or by primary group.
        bool headlessCliPrivateGroup(uid_t uid, gid_t gid) {
            QByteArray ownerName;
            if (const passwd *owner = ::getpwuid(uid); owner != nullptr) {
                if (owner->pw_gid != gid) return false;
                ownerName = owner->pw_name;
            } else if (uid != gid || uid == 0) {
                // An owner this system does not know (a bind mount into a container) passes only by the UPG convention, gid == uid.
                return false;
            }
            if (const group *entry = ::getgrgid(gid); entry != nullptr) {
                for (char **member = entry->gr_mem; member != nullptr && *member != nullptr; ++member) {
                    if (ownerName.isEmpty() || ownerName != *member) return false;
                }
            }
            bool shared = false;
            ::setpwent();
            while (const passwd *user = ::getpwent()) {
                if (user->pw_gid == gid && user->pw_uid != uid) {
                    shared = true;
                    break;
                }
            }
            ::endpwent();
            return !shared;
        }

        QString headlessCliGroupName(gid_t gid) {
            if (const group *entry = ::getgrgid(gid); entry != nullptr) {
                return QString("group %1 (gid %2)").arg(QString::fromLocal8Bit(entry->gr_name), QString::number(gid));
            }
            return QString("group gid %1").arg(gid);
        }

        // The dir holds the settings that name the programs Throne starts: root refuses one other users can write. Another owner is
        // only warned about, since a bind mount or a sudo run on one's own dir is owned by the invoking user.
        bool headlessCliVetDir(const QString &path, QString *error) {
            const QByteArray encoded = QFile::encodeName(path);
            struct stat entry{};
            struct stat st{};
            if (::lstat(encoded.constData(), &entry) != 0 || ::stat(encoded.constData(), &st) != 0) {
                const int code = errno;
                *error = QString("cannot inspect %1: %2").arg(path, headlessCliErrno(code));
                return false;
            }
            if (!S_ISDIR(st.st_mode)) {
                *error = QString("%1 exists and is not a directory").arg(path);
                return false;
            }
            const uid_t euid = ::geteuid();
            QStringList writable;
            if (S_ISLNK(entry.st_mode) && entry.st_uid != euid && entry.st_uid != 0) {
                writable << QString("it is a symbolic link owned by uid %1").arg(entry.st_uid);
            }
            if ((st.st_mode & S_IWOTH) != 0) writable << QStringLiteral("every user can write to it");
            if ((st.st_mode & S_IWGRP) != 0 && !headlessCliPrivateGroup(st.st_uid, st.st_gid)) {
                writable << headlessCliGroupName(st.st_gid) + QStringLiteral(" can write to it");
            }
            if (euid == 0 && !writable.isEmpty()) {
                *error = QString("refusing to run as root with %1: %2. It holds the settings that name the programs Throne starts, "
                                 "so only its owner may write to it (chmod go-w it, or pass another --data-dir)")
                             .arg(path, writable.join("; "));
                return false;
            }
            if (!writable.isEmpty()) {
                LOG_WARN(QString("%1: %2, so other users can change Throne's settings, which name the programs it starts")
                             .arg(path, writable.join("; ")));
            }
            if (st.st_uid != euid) {
                LOG_WARN(QString("%1 is owned by uid %2, who controls Throne's settings, which name the programs it starts%3")
                             .arg(path, QString::number(st.st_uid), euid == 0 ? QStringLiteral(" as root") : QString()));
            }
            return true;
        }
#endif

        // Creates a missing dir, vets its owner and mode on Unix, and checks that it takes a new file.
        bool headlessCliPrepareDir(const QString &path, QString *error) {
#ifdef Q_OS_UNIX
            if (!headlessCliMakeDirs(path, error) || !headlessCliVetDir(path, error)) return false;
#else
            const QFileInfo info(path);
            if (info.exists() && !info.isDir()) {
                *error = QString("%1 exists and is not a directory").arg(QDir::toNativeSeparators(path));
                return false;
            }
            if (!info.exists() && !QDir().mkpath(path)) {
                *error = QString("could not create %1").arg(QDir::toNativeSeparators(path));
                return false;
            }
#endif
            // A fresh random name, created exclusively: a planted link or file is never followed or truncated.
            QTemporaryFile probe(QDir(path).absoluteFilePath(".throne-write-test-XXXXXX"));
            if (!probe.open()) {
                *error = QString("%1 is not writable: %2").arg(QDir::toNativeSeparators(path), probe.errorString());
                return false;
            }
            probe.remove();
            return true;
        }

#ifdef Q_OS_WIN
        // Never mkpath the dir itself: it would inherit ProgramData's user-writable ACEs until SecureDataDir locks it down.
        bool headlessCliSecureSystemDir(const QString &path, QString *error) {
            const QString native = QDir::toNativeSeparators(path);
            const QFileInfo info(path);
            if (info.exists() && !info.isDir()) {
                *error = QString("%1 exists and is not a directory").arg(native);
                return false;
            }
            const bool created = !info.exists();
            if (QString why; !Sys::ServiceManager::SecureDataDir(path, created, &why)) {
                *error = QString("%1: %2").arg(native, why);
                return false;
            }
            if (created && !QDir(path).isEmpty(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) {
                *error = QString("%1 gained files from another process while it was being created; remove it").arg(native);
                return false;
            }
            return true;
        }
#endif
    } // namespace

    void AddOptions(QCommandLineParser &parser) {
        // Before any option: the default compacted mode would split -appdata or -headless into single letters.
        parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
        parser.setApplicationDescription(
            "Throne without a window, driven through the Throne API (thronectl). One or two dashes both work for every option.");
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption headless(QStringLiteral("headless"), QStringLiteral("Run without a window."));
        headless.setFlags(QCommandLineOption::HiddenFromHelp);
        parser.addOption(headless);

#ifdef Q_OS_WIN
        parser.addOption({{kDataDir, "appdata"},
                          "Data directory (THRONE_DATA_DIR). Default: %ProgramData%\\Throne with --service or as LocalSystem, "
                          "else the per-user configuration directory (%LOCALAPPDATA%\\Throne). A portable desktop install keeps "
                          "its data next to the program: pass --data-dir <program directory> to share it.",
                          "DIR"});
#else
        parser.addOption({{kDataDir, "appdata"},
                          "Data directory (THRONE_DATA_DIR). Default: $STATE_DIRECTORY, then /var/lib/throne as root on Linux "
                          "(/Library/Application Support/Throne on macOS), else the per-user configuration directory (such as "
                          "~/.config/Throne). A portable desktop install keeps its data next to the program: pass --data-dir "
                          "<program directory> to share it.",
                          "DIR"});
#endif
        parser.addOption({kRestartMode,
                          "What a restart request (exit code 75) does: exit leaves it to the service manager, exec re-runs Throne "
                          "in place (on Windows: starts a new Throne process), auto picks exit under systemd, launchd, a container "
                          "init or as a Windows service and exec otherwise (THRONE_RESTART_MODE). Default: auto.",
                          "MODE"});
        parser.addOption({kNoResume, "Do not reconnect the last profile at start (THRONE_RESUME=0)."});
        parser.addOption({kNoKillSwitch,
                          "Keep the kill switch off for this run even when the settings turn it on; profiles stay refused "
                          "until kill_switch is turned off (THRONE_KILL_SWITCH=0)."});
        parser.addOption({kLogFormat,
                          "Log format on stderr: auto, journal (systemd priority prefixes) or plain (THRONE_LOG_FORMAT). "
                          "Default: auto.",
                          "FORMAT"});
        parser.addOption({kApiListen,
                          "Throne API listener: PORT, IPv4:PORT, [IPv6]:PORT, *:PORT, or off (THRONE_API_LISTEN). Default: "
                          "127.0.0.1:9095, unless the settings enable the API.",
                          "LISTEN"});
        parser.addOption({kApiKeyFile,
                          "Throne API key file; it must exist (create it with: thronectl keygen -o FILE) "
                          "(THRONE_API_KEY_FILE). Default: <data dir>/config/api.key, created on the first start.",
                          "FILE"});
        parser.addOption({kApiAdminLan,
                          "Accept management requests from the local network, not only from this device "
                          "(THRONE_API_ADMIN_LAN=1)."});
        parser.addOption({kApiAllow,
                          "Addresses or subnets allowed to use a non-local API listener, separated by commas or spaces "
                          "(THRONE_API_ALLOW). An empty value, or one with only separators, exits 64; leave THRONE_API_ALLOW "
                          "unset to allow every address.",
                          "LIST"});
        parser.addOption({kHealth,
                          "Ask the running instance for its health and print one JSON line; exit 0 healthy, 1 not healthy "
                          "or starting, 2 no answer."});
        parser.addOption({kPrintConnection,
                          "Print \"export THRONE_ADDR=...\" and \"export THRONE_KEY_FILE=...\" lines for thronectl: "
                          "eval \"$(Throne --print-connection)\"."});
        parser.addOption({kCheck, "Check whether this host can run Throne headless (data dir, TLS, core, Tun, kill switch, ports) "
                                  "and print one line per item."});
        // Registered everywhere so that Parse can name the platform instead of reporting an unknown option.
        QCommandLineOption service(kService,
                                   "Run as a Windows service, started by the Service Control Manager (install-service.bat); started "
                                   "any other way it exits 64. The data directory defaults to %ProgramData%\\Throne, and a restart "
                                   "request exits 75 for the service's recovery actions.");
#ifndef Q_OS_WIN
        service.setFlags(QCommandLineOption::HiddenFromHelp);
#endif
        parser.addOption(service);
        parser.addOption({kDebug, "Debug logging."});
    }

    ParseOutcome Parse(QCommandLineParser &parser, const QStringList &arguments, const QProcessEnvironment &env,
                       const QDir &launchCwd, const Bootstrap::Launch &launch, LaunchOptions *out, QString *message) {
        QStringList args;
        for (qsizetype i = 0; i < arguments.size(); i++) {
            const QString &arg = arguments.at(i);
            // Finder adds -psn_<n>; -wait_pid= belongs to Bootstrap::RelaunchParent.
            if (i > 0 && (arg.startsWith("-psn_") || arg.startsWith("-wait_pid="))) continue;
            args << arg;
        }
        if (!parser.parse(args)) {
            *message = parser.errorText();
            return ParseOutcome::Usage;
        }
        // addHelpOption() registers --help-all as well; parse() never acts on either by itself.
        if (parser.isSet(QStringLiteral("help")) || parser.isSet(QStringLiteral("help-all"))) return ParseOutcome::Help;
        if (parser.isSet(QStringLiteral("version"))) return ParseOutcome::Version;
        if (const auto positional = parser.positionalArguments(); !positional.isEmpty()) {
            *message = QString("unexpected argument \"%1\": links and files are imported with thronectl").arg(positional.first());
            return ParseOutcome::Usage;
        }

        QStringList clientFlags;
        for (const char *flag : {kHealth, kPrintConnection, kCheck}) {
            if (parser.isSet(flag)) clientFlags << QStringLiteral("--") + QLatin1String(flag);
        }
        if (clientFlags.size() > 1) {
            *message = QString("%1 cannot be combined").arg(clientFlags.join(" and "));
            return ParseOutcome::Usage;
        }
        out->client = parser.isSet(kHealth)            ? ClientMode::Health
                      : parser.isSet(kPrintConnection) ? ClientMode::PrintConnection
                      : parser.isSet(kCheck)           ? ClientMode::Check
                                                       : ClientMode::None;
        // The pre-scan saw raw argv; a client flag swallowed as another option's value would flip the mode.
        if ((launch.mode == Bootstrap::Mode::HeadlessClient) != (out->client != ClientMode::None)) {
            *message = "--health, --print-connection or --check appears as the value of another option";
            return ParseOutcome::Usage;
        }
#ifdef Q_OS_WIN
        // main() already connected to the SCM on the pre-scan's word.
        out->service = launch.service || parser.isSet(kService);
        if (out->service && out->client != ClientMode::None) {
            *message = QString("--service cannot be combined with %1").arg(clientFlags.first());
            return ParseOutcome::Usage;
        }
#else
        if (parser.isSet(kService)) {
            *message = "--service is for Windows only (it runs Throne under the Windows Service Control Manager); here a systemd "
                       "unit, a launchd plist or a container runs Throne headless";
            return ParseOutcome::Usage;
        }
#endif
        out->trigger = out->client != ClientMode::None ? QStringLiteral("client mode")
                       : parser.isSet(QStringLiteral("headless")) ? QStringLiteral("--headless")
                       : out->service                             ? QStringLiteral("--service")
                                                                  : QStringLiteral("THRONE_HEADLESS");

        if (parser.isSet(kDataDir)) {
            const QString value = parser.value(kDataDir);
            if (value.trimmed().isEmpty()) {
                *message = "--data-dir needs a directory";
                return ParseOutcome::Usage;
            }
            out->dataDir = QDir::cleanPath(launchCwd.absoluteFilePath(value));
        }

        if (parser.isSet(kRestartMode)) {
            if (!headlessCliParseRestartMode(parser.value(kRestartMode), &out->restartMode)) {
                *message = QString("--restart-mode must be auto, exit or exec, not \"%1\"").arg(parser.value(kRestartMode));
                return ParseOutcome::Usage;
            }
        } else if (const QString value = env.value("THRONE_RESTART_MODE"); !value.isEmpty()) {
            if (!headlessCliParseRestartMode(value, &out->restartMode)) {
                *message = QString("THRONE_RESTART_MODE must be auto, exit or exec, not \"%1\"").arg(value);
                return ParseOutcome::Usage;
            }
        }

        out->resume = !parser.isSet(kNoResume);
        if (const QString value = env.value("THRONE_RESUME"); !value.isEmpty()) {
            const int resume = headlessCliParseBool(value);
            if (resume < 0) {
                *message = QString("THRONE_RESUME must be 1, true, yes, on, 0, false, no or off, not \"%1\"").arg(value);
                return ParseOutcome::Usage;
            }
            if (resume == 0) out->resume = false;
        }

        out->noKillSwitch = parser.isSet(kNoKillSwitch);
        if (const QString value = env.value("THRONE_KILL_SWITCH"); !value.isEmpty()) {
            const int killSwitch = headlessCliParseBool(value);
            if (killSwitch < 0) {
                *message = QString("THRONE_KILL_SWITCH must be 1, true, yes, on, 0, false, no or off, not \"%1\"").arg(value);
                return ParseOutcome::Usage;
            }
            if (killSwitch == 0) out->noKillSwitch = true;
        }

        if (parser.isSet(kLogFormat)) {
            if (!headlessCliParseLogFormat(parser.value(kLogFormat), &out->logFormat)) {
                *message = QString("--log-format must be auto, journal or plain, not \"%1\"").arg(parser.value(kLogFormat));
                return ParseOutcome::Usage;
            }
        } else if (const QString value = env.value("THRONE_LOG_FORMAT"); !value.isEmpty()) {
            if (!headlessCliParseLogFormat(value, &out->logFormat)) {
                *message = QString("THRONE_LOG_FORMAT must be auto, journal or plain, not \"%1\"").arg(value);
                return ParseOutcome::Usage;
            }
        }

        out->debug = parser.isSet(kDebug);
        out->openDataDirArgs = out->debug ? QStringList{QStringLiteral("-debug")} : QStringList{};
        return ParseOutcome::Run;
    }

    QString HelpText(const QCommandLineParser &parser) {
        QString text = parser.helpText() + QStringLiteral(
            "\n"
            "Environment:\n"
            "  THRONE_HEADLESS=1   run headless without --headless\n"
            "  Every option above names its variable; a flag wins over its variable.\n"
            "\n"
            "Exit codes:\n"
            "  0   stopped (SIGTERM, SIGINT, Ctrl+C, a service stop, thronectl shutdown); --health: healthy; --check:\n"
            "      nothing failed\n"
            "  1   start failed (data dir lock, instance socket, database (transient), core IPC listener, API address\n"
            "      never appeared); --health: not healthy or still starting; --check: an item failed\n"
            "  2   --health: no instance answered; --print-connection: the API is off or not listening, or its key is not\n"
            "      in a file\n"
            "  3   the data directory is in use by another Throne\n"
            "  64  bad option, option value or environment value; --service not started by the Windows service manager\n"
            "  70  the core kept failing and Throne gave up, or it was not usable for 60 s (see Recovery)\n"
            "  71  the progress watchdog found the engine stuck (a profile connecting for 180 s or stopping for 90 s)\n"
            "  75  restart requested (thronectl restart, SIGHUP, backup restore) with --restart-mode exit, or as a\n"
            "      Windows service\n"
            "  78  configuration error (data directory, API key file, API listener, unusable database file)\n"
            "\n"
            "Recovery:\n"
            "  If saved settings keep Throne unreachable (kill switch, Tun), start it once with --no-resume\n"
            "  --no-kill-switch: no profile starts and the kill switch stays off for that run, so thronectl on this host\n"
            "  can change the settings. After three boots in a row without a working core, Throne stays up (degraded\n"
            "  core_gave_up) instead of exiting 70 or 71, so the API stays reachable.\n");
#ifdef Q_OS_WIN
        text += QStringLiteral(
            "\n"
            "Windows:\n"
            "  Throne.exe returns to the prompt at once. For the exit code run it with start \"\" /wait Throne.exe ... in\n"
            "  cmd, or with Start-Process -Wait -PassThru in PowerShell.\n");
#endif
        return text;
    }

    int ResolveDataDir(LaunchOptions *options, const QProcessEnvironment &env, bool create, Bootstrap::DataDir *out,
                       QString *error) {
        QString path;
        [[maybe_unused]] bool systemDefault = false;
        if (!options->dataDir.isEmpty()) {
            path = options->dataDir;
            options->dataDirSource = QStringLiteral("--data-dir");
        } else if (const QString value = env.value("THRONE_DATA_DIR"); !value.isEmpty()) {
            // Still the launch CWD: RunHeadless resolves the dir before OpenDataDir moves it.
            path = QDir::current().absoluteFilePath(value);
            options->dataDirSource = QStringLiteral("THRONE_DATA_DIR");
#ifdef Q_OS_WIN
        } else if (path = Sys::ServiceManager::SystemDataDir(options->service); !path.isEmpty()) {
            options->dataDirSource = QStringLiteral("ProgramData default");
            systemDefault = true;
#else
        } else if (const QString value = env.value("STATE_DIRECTORY").section(':', 0, 0); !value.isEmpty()) {
            path = QDir::current().absoluteFilePath(value);
            options->dataDirSource = QStringLiteral("STATE_DIRECTORY");
        } else if (path = headlessCliRootDefault(); !path.isEmpty()) {
            options->dataDirSource = QStringLiteral("root default");
#endif
        } else {
#ifdef Q_OS_UNIX
            // Without a home, AppConfigLocation becomes /.config/Throne.
            const QString home = QDir::cleanPath(env.value("HOME"));
#ifdef Q_OS_LINUX
            const bool xdgConfig = QDir::isAbsolutePath(env.value("XDG_CONFIG_HOME"));
#else
            const bool xdgConfig = false;
#endif
            if (!xdgConfig && (home.isEmpty() || home == "/")) {
                *error = "HOME is not set (or is /), so there is no per-user data directory; pass --data-dir DIR or set THRONE_DATA_DIR";
                return ExitCode::Config;
            }
#endif
            path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
            options->dataDirSource = QStringLiteral("user config");
        }
        path = QDir::cleanPath(path);
        if (path.isEmpty()) {
            *error = "no data directory could be determined; pass --data-dir DIR";
            return ExitCode::Config;
        }

        const QString configDir = QDir(path).absoluteFilePath("config");
        if (create) {
#ifdef Q_OS_WIN
            if (systemDefault && !headlessCliSecureSystemDir(path, error)) return ExitCode::Config;
#endif
            if (!headlessCliPrepareDir(path, error) || !headlessCliPrepareDir(configDir, error)) return ExitCode::Config;
#ifdef Q_OS_WIN
            if (const QString who = Sys::ServiceManager::UsersCanWriteDataDir(path); !systemDefault && !who.isEmpty()) {
                LOG_WARN(QString("%1: %2 can write to it, so other users can change Throne's settings, which name the programs it starts")
                             .arg(QDir::toNativeSeparators(path), who));
            }
#endif
        } else if (const QFileInfo info(path); info.exists() && !info.isDir()) {
            *error = QString("%1 exists and is not a directory").arg(QDir::toNativeSeparators(path));
            return ExitCode::Config;
        }

        out->wd = QDir(path);
        out->useAppdata = true;
        out->appdataDir = path;
        out->configDir = configDir;
        return ExitCode::Ok;
    }

    RestartMode EffectiveRestartMode(RestartMode requested, const QProcessEnvironment &env, qint64 bootParentPid, QString *why) {
        if (requested != RestartMode::Auto) {
#ifdef Q_OS_WIN
            if (requested == RestartMode::Exec && Sys::ServiceManager::IsService()) {
                *why = "a Windows service always exits; its recovery actions start it again";
                return RestartMode::Exit;
            }
#endif
            *why = "requested";
            return requested;
        }
        if (!env.value("NOTIFY_SOCKET").isEmpty()) {
            *why = "systemd notify socket";
            return RestartMode::Exit;
        }
#ifdef Q_OS_WIN
        Q_UNUSED(bootParentPid)
        if (Sys::ServiceManager::IsService()) {
            *why = "Windows service";
            return RestartMode::Exit;
        }
        *why = "not a Windows service; a new Throne process starts";
        return RestartMode::Exec;
#else
        // Set by the image that exec'd us: the launching shell may be gone and our new parent says nothing about a supervisor.
        if (env.value("THRONE_RESTART_INHERITED") == "exec") {
            *why = "inherited from the previous image";
            return RestartMode::Exec;
        }
        // getppid() is 0 when Throne itself is PID 1 in a container without an init.
        if (bootParentPid <= 1) {
            *why = "parent is PID 1 (system manager, container init) or we are PID 1";
            return RestartMode::Exit;
        }
        const QString comm = Bootstrap::ProcessName(bootParentPid);
        // INVOCATION_ID alone leaks into shells under terminal services; a user unit's parent is the systemd user manager.
        if (!env.value("INVOCATION_ID").isEmpty() && comm == "systemd") {
            *why = "systemd user manager";
            return RestartMode::Exit;
        }
        *why = comm.isEmpty() ? QString("launched by PID %1").arg(bootParentPid) : QString("launched by %1").arg(comm);
        return RestartMode::Exec;
#endif
    }

    QString RestartModeName(RestartMode mode) {
        switch (mode) {
            case RestartMode::Exit: return QStringLiteral("exit");
            case RestartMode::Exec: return QStringLiteral("exec");
            case RestartMode::Auto: break;
        }
        return QStringLiteral("auto");
    }
} // namespace Engine::HeadlessCli
