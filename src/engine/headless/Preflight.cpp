#include "include/engine/headless/Preflight.h"

#include <cstdio>
#include <exception>
#include <initializer_list>
#include <optional>
#include <string>

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QNetworkInterface>
#include <QProcess>
#include <QSslSocket>
#include <QStringList>
#include <QSysInfo>
#include <QTcpServer>
#include <QUdpSocket>
#include <QUrl>

#include <3rdparty/SQLiteCpp/include/SQLiteCpp.h>
#include <3rdparty/SQLiteCpp/include/sqlite3.h>

#include "NkrVersion.h"
#include "include/api/remote/Protocol.hpp"
#include "include/engine/app/DataDirLock.h"
#include "include/engine/app/ExitCodes.h"
#include "include/engine/headless/HeadlessInstance.h"
#include "include/global/Configs.hpp"
#include "include/global/Validation.hpp"
#include "include/sys/HostPresence.hpp"
#include "include/sys/HostTun.hpp"
#include "include/sys/Process.hpp"
#include "include/sys/ServiceManager.hpp"

#ifdef Q_OS_UNIX
#include <cerrno>

#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Engine::Preflight {
    namespace {
        constexpr int preflightHelloMs = 2000;
        constexpr int preflightCoreCheckMs = 5000;
        constexpr int preflightDbBusyMs = 2000;

        void preflightPrint(const QString &text) {
#ifdef Q_OS_WIN
            if (_fileno(stdout) < 0) return;
#endif
            std::fputs(text.toLocal8Bit().append('\n').constData(), stdout);
            std::fflush(stdout);
        }

        class PreflightReport {
        public:
            void ok(const QString &text) { printLine("[ OK ] ", text, {}); }

            void info(const QString &text, const QStringList &fixes = {}) { printLine("[INFO] ", text, fixes); }

            void warn(const QString &text, const QStringList &fixes = {}) {
                ++m_warnings;
                printLine("[WARN] ", text, fixes);
            }

            void fail(const QString &text, const QStringList &fixes = {}) {
                ++m_failures;
                printLine("[FAIL] ", text, fixes);
            }

            [[nodiscard]] int failures() const { return m_failures; }

            [[nodiscard]] int warnings() const { return m_warnings; }

        private:
            static void printLine(const char *tag, const QString &text, const QStringList &fixes) {
                preflightPrint(QLatin1String(tag) + text);
                for (const auto &fix : fixes) preflightPrint(QStringLiteral("       fix: ") + fix);
            }

            int m_failures = 0;
            int m_warnings = 0;
        };

        QString preflightNative(const QString &path) {
            return QDir::toNativeSeparators(path);
        }

        // For the fix lines: a path the user pastes into a shell.
        QString preflightQuote(const QString &path) {
            const QString shown = preflightNative(path);
#ifdef Q_OS_WIN
            return shown.contains(QLatin1Char(' ')) ? QLatin1Char('"') + shown + QLatin1Char('"') : shown;
#else
            bool plain = !shown.isEmpty();
            for (const QChar c : shown) {
                if (!c.isLetterOrNumber() && !QStringLiteral("_@%+=:,./-").contains(c)) {
                    plain = false;
                    break;
                }
            }
            if (plain) return shown;
            QString quoted = shown;
            quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
            return QLatin1Char('\'') + quoted + QLatin1Char('\'');
#endif
        }

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
        // Fix lines that grant the core privileges; a packaged install names its own way instead.
        QStringList preflightElevationFixes(const QStringList &fixes) {
#ifdef NKR_ELEVATION_HINT
            Q_UNUSED(fixes)
            return {QString::fromUtf8(NKR_ELEVATION_HINT)};
#else
            return fixes;
#endif
        }
#endif

        bool preflightWritable(const QString &path) {
#ifdef Q_OS_UNIX
            return ::access(QFile::encodeName(path).constData(), W_OK | X_OK) == 0;
#else
#if defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
            // Without it Qt reports only the read-only attribute, never the ACL.
            const QNtfsPermissionCheckGuard ntfsCheck;
#endif
            return QFileInfo(path).isWritable();
#endif
        }

        // The closest existing directory a missing path would be created in; empty when none exists.
        QString preflightExistingAncestor(const QString &path) {
            QString current = QDir::cleanPath(path);
            while (!QFileInfo::exists(current)) {
                const QString parent = QFileInfo(current).path();
                if (parent == current || parent.isEmpty()) return {};
                current = parent;
            }
            return current;
        }

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
        // A missing dir counts when the first start could create it.
        bool preflightCreatableOrWritable(const QString &dir) {
            if (QFileInfo::exists(dir)) return QFileInfo(dir).isDir() && preflightWritable(dir);
            const QString ancestor = preflightExistingAncestor(dir);
            return !ancestor.isEmpty() && QFileInfo(ancestor).isDir() && preflightWritable(ancestor);
        }
#endif

#ifdef Q_OS_UNIX
        QString preflightUserName(uid_t uid) {
            if (const passwd *entry = ::getpwuid(uid); entry != nullptr) return QString::fromLocal8Bit(entry->pw_name);
            return QString::number(uid);
        }

        // HeadlessCli's rule: a user private group, with no one but the owner in it, may write to the data dir.
        bool preflightPrivateGroup(uid_t uid, gid_t gid) {
            QByteArray ownerName;
            if (const passwd *owner = ::getpwuid(uid); owner != nullptr) {
                if (owner->pw_gid != gid) return false;
                ownerName = owner->pw_name;
            } else if (uid != gid || uid == 0) {
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

        QString preflightGroupName(gid_t gid) {
            if (const group *entry = ::getgrgid(gid); entry != nullptr) {
                return QStringLiteral("group %1 (gid %2)").arg(QString::fromLocal8Bit(entry->gr_name), QString::number(gid));
            }
            return QStringLiteral("group gid %1").arg(gid);
        }

        // Who besides its owner can change the dir, as HeadlessCli judges it before a start; *owner gets the owner uid.
        QStringList preflightOpenToOthers(const QString &path, uid_t *owner) {
            const QByteArray encoded = QFile::encodeName(path);
            struct stat entry{};
            struct stat st{};
            if (::lstat(encoded.constData(), &entry) != 0 || ::stat(encoded.constData(), &st) != 0) return {};
            *owner = st.st_uid;
            const uid_t euid = ::geteuid();
            QStringList reasons;
            if (S_ISLNK(entry.st_mode) && entry.st_uid != euid && entry.st_uid != 0) {
                reasons << QStringLiteral("it is a symbolic link owned by uid %1").arg(entry.st_uid);
            }
            if ((st.st_mode & S_IWOTH) != 0) reasons << QStringLiteral("every user can write to it");
            if ((st.st_mode & S_IWGRP) != 0 && !preflightPrivateGroup(st.st_uid, st.st_gid)) {
                reasons << preflightGroupName(st.st_gid) + QStringLiteral(" can write to it");
            }
            return reasons;
        }
#endif

        // The lock record outlives its owner, and the PID may belong to another program by now.
        bool preflightPidAlive(qint64 pid) {
            if (pid <= 0) return false;
#ifdef Q_OS_UNIX
            if (static_cast<qint64>(static_cast<pid_t>(pid)) != pid) return false;
            if (::kill(static_cast<pid_t>(pid), 0) != 0 && errno != EPERM) return false;
            // kill() already proved it alive; a name this OS or user cannot read does not undo that.
            const QString name = Bootstrap::ProcessName(pid);
            return name.isEmpty() || name.contains(QLatin1String("throne"), Qt::CaseInsensitive);
#else
            return Bootstrap::ProcessName(pid).contains(QLatin1String("throne"), Qt::CaseInsensitive);
#endif
        }

        // ---- 1. build

        void preflightBuild(PreflightReport &report) {
            QStringList fields{QStringLiteral("Throne " NKR_VERSION), QStringLiteral("Qt ") + QString::fromLatin1(qVersion()),
                               QSysInfo::prettyProductName() + QLatin1Char(' ') + QSysInfo::currentCpuArchitecture()};
#ifdef Q_OS_UNIX
            fields << QStringLiteral("kernel ") + QSysInfo::kernelVersion();
            fields << QStringLiteral("euid %1, ruid %2").arg(::geteuid()).arg(::getuid());
#elif defined(Q_OS_WIN)
            fields << (Configs::KnownAdmin().value_or(false) ? QStringLiteral("elevated") : QStringLiteral("not elevated"));
            if (Sys::ServiceManager::RunningAsLocalSystem()) fields << QStringLiteral("LocalSystem");
#endif
            report.info(QStringLiteral("build: ") + fields.join(QStringLiteral(", ")));
        }

        // ---- 2. data dir

        void preflightDataDir(PreflightReport &report, const HeadlessCli::LaunchOptions &options, const QProcessEnvironment &env,
                              const Bootstrap::DataDir &dataDir) {
            const QString wd = dataDir.wd.absolutePath();
            const QString shown = QStringLiteral("data dir %1 (%2)").arg(preflightNative(wd), options.dataDirSource);
#ifdef Q_OS_UNIX
            const QString user = preflightUserName(::geteuid());
#endif
            if (!QFileInfo::exists(wd)) {
                const QString ancestor = preflightExistingAncestor(wd);
                if (!ancestor.isEmpty() && QFileInfo(ancestor).isDir() && preflightWritable(ancestor)) {
                    report.info(shown + QStringLiteral(": does not exist yet; the first start creates it"));
                } else {
                    QStringList fixes;
#ifdef Q_OS_UNIX
                    fixes << QStringLiteral("sudo install -d -m 700 -o %1 %2").arg(user, preflightQuote(wd));
#else
                    fixes << QStringLiteral("create it with write access for this user");
#endif
                    fixes << QStringLiteral("or pass another --data-dir");
                    report.fail(shown + QStringLiteral(": does not exist, and this user cannot create it in %1")
                                            .arg(ancestor.isEmpty() ? QStringLiteral("any existing parent") : preflightNative(ancestor)),
                                fixes);
                }
            } else {
                QStringList fails;
                QStringList warns;
                QStringList fixes;
                const auto vet = [&](const QString &path) {
                    const QFileInfo info(path);
                    if (!info.exists()) return;
                    const QString shownPath = preflightNative(path);
                    if (!info.isDir()) {
                        fails << shownPath + QStringLiteral(" is not a directory");
                        fixes << QStringLiteral("move it aside, or pass another --data-dir");
                        return;
                    }
                    if (!preflightWritable(path)) {
                        fails << shownPath + QStringLiteral(" is not writable by this user");
#ifdef Q_OS_UNIX
                        fixes << QStringLiteral("run Throne (and this check) as the owner of the data dir, or: sudo chown -R %1 %2")
                                     .arg(user, preflightQuote(wd));
#else
                        fixes << QStringLiteral("give this user write access to it, or pass another --data-dir");
#endif
                    }
#ifdef Q_OS_UNIX
                    uid_t owner = ::geteuid();
                    if (const QStringList open = preflightOpenToOthers(path, &owner); !open.isEmpty()) {
                        if (::geteuid() == 0) {
                            fails << shownPath + QStringLiteral(": ") + open.join(QStringLiteral("; ")) +
                                         QStringLiteral(" (Throne refuses to run as root with it)");
                        } else {
                            warns << shownPath + QStringLiteral(": ") + open.join(QStringLiteral("; ")) +
                                         QStringLiteral(", so other users can change the settings that name the programs Throne starts");
                        }
                        fixes << QStringLiteral("chmod go-w %1").arg(preflightQuote(path));
                    }
                    if (owner != ::geteuid()) {
                        warns << QStringLiteral("%1 is owned by uid %2, who controls Throne's settings").arg(shownPath).arg(owner);
                    }
#endif
                };
                vet(wd);
                vet(dataDir.configDir);
                fixes.removeDuplicates();
                if (!fails.isEmpty()) {
                    report.fail(shown + QStringLiteral(": ") + (fails + warns).join(QStringLiteral("; ")), fixes);
                } else if (!warns.isEmpty()) {
                    report.warn(shown + QStringLiteral(": ") + warns.join(QStringLiteral("; ")), fixes);
                } else {
                    report.ok(shown + (QFileInfo::exists(dataDir.configDir) ? QStringLiteral(": writable")
                                                                             : QStringLiteral(": writable; config/ is created on the first start")));
                }
            }
#ifdef Q_OS_UNIX
            if (const QString home = env.value(QStringLiteral("HOME")); home.isEmpty() || QDir::cleanPath(home) == QLatin1String("/")) {
                report.info(QStringLiteral("HOME is not set (or is /): keep naming the data dir explicitly (--data-dir, THRONE_DATA_DIR or "
                                           "STATE_DIRECTORY)"));
            }
#else
            Q_UNUSED(env)
#endif
        }

        // ---- 3. instance

        struct PreflightInstance {
            // The live owner of this data dir (it answered HELLO, or the OS reports it running); 0 when none.
            qint64 pid = 0;
            QString who;
            // The headless owner answered HEALTH with its full status; the fields below are only meaningful then.
            bool health = false;
            QString connection;
            QString apiState;
            QString apiListen;
            QString apiError;
            // A profile is connecting, running or stopping: only then does the core hold the mixed, dns-in, Clash and sing-box API ports.
            bool coreBound = false;
            bool tunOn = false;
        };

        // Liveness says nothing about which ports the owner holds; its HEALTH does. A still-starting owner has no api block yet.
        void preflightAskHealth(const Bootstrap::DataDir &dataDir, PreflightInstance *instance) {
            const auto reply = HeadlessInstance::AskInstance(dataDir.wd, QByteArrayLiteral("HEALTH"), preflightHelloMs);
            if (!reply.answered) return;
            const QJsonObject health = QJsonDocument::fromJson(reply.line).object();
            const QJsonValue api = health.value(QStringLiteral("api"));
            if (!api.isObject()) return;
            if (const qint64 pid = health.value(QStringLiteral("pid")).toInteger(); pid > 0 && pid != instance->pid) return;
            const QJsonObject block = api.toObject();
            instance->health = true;
            instance->apiState = block.value(QStringLiteral("state")).toString();
            instance->apiListen = block.value(QStringLiteral("listen")).toString();
            instance->apiError = block.value(QStringLiteral("error")).toString();
            instance->connection = health.value(QStringLiteral("state")).toString();
            instance->coreBound = instance->connection == QLatin1String("connecting") || instance->connection == QLatin1String("running") ||
                                  instance->connection == QLatin1String("stopping");
            // tun is the requested mode; tun_running covers a Tun profile kept running after tun off, or a custom config's tun inbound.
            instance->tunOn = instance->coreBound &&
                              (health.value(QStringLiteral("tun")).toBool() || health.value(QStringLiteral("tun_running")).toBool());
        }

        PreflightInstance preflightInstance(PreflightReport &report, const Bootstrap::DataDir &dataDir) {
            PreflightInstance instance;
            if (!dataDir.wd.exists()) {
                report.ok(QStringLiteral("instance: none (no data dir yet)"));
                return instance;
            }
            // ReadOwner only reads; flock on the lock would make a daemon starting right now exit 3.
            const QJsonObject owner =
                QFileInfo::exists(DataDirLock::LockPath(dataDir.configDir)) ? DataDirLock::ReadOwner(dataDir.configDir) : QJsonObject();
            const qint64 ownerPid = owner.value(QStringLiteral("pid")).toInteger();
            const QString ownerMode = owner.value(QStringLiteral("mode")).toString();
            const QString described = DataDirLock::DescribeOwner(owner);
            const qint64 hello = HeadlessInstance::Hello(dataDir.wd, QStringLiteral("--check"), preflightHelloMs);
            if (hello > 0) {
                instance.pid = hello;
                instance.who = QStringLiteral("the headless Throne of this data dir");
                preflightAskHealth(dataDir, &instance);
                QString status;
                if (instance.health) {
                    status = QStringLiteral("; connection %1, API %2").arg(instance.connection, instance.apiState);
                    if (!instance.apiListen.isEmpty()) status += QStringLiteral(" (%1)").arg(instance.apiListen);
                }
                report.info(QStringLiteral("instance: running: %1")
                                .arg(ownerPid == hello ? described : QStringLiteral("headless (PID %1)").arg(hello)) +
                            status);
                return instance;
            }
            const QString answered = hello == 0 ? QStringLiteral("; something that is not a headless Throne answered on its instance socket")
                                                : QString();
            if (ownerPid <= 0) {
                report.ok(QStringLiteral("instance: none") + answered);
                return instance;
            }
            if (!preflightPidAlive(ownerPid)) {
                report.ok(QStringLiteral("instance: none (the last owner, PID %1, has exited)").arg(ownerPid) + answered);
                return instance;
            }
            instance.pid = ownerPid;
            const bool gui = ownerMode == QLatin1String("gui");
            instance.who = gui ? QStringLiteral("the Throne window that uses this data dir") : QStringLiteral("the owner of this data dir");
            if (gui) {
                report.warn(QStringLiteral("instance: the Throne window holds this data dir: %1; Throne --headless exits 3 while it runs")
                                .arg(described),
                            {QStringLiteral("quit the Throne window, or pass another --data-dir")});
            } else if (ownerMode == QLatin1String("headless")) {
                report.info(QStringLiteral("instance: %1 holds the data dir but did not answer (still starting, or busy)").arg(described) +
                            answered);
            } else {
                report.info(QStringLiteral("instance: %1 holds the data dir").arg(described) + answered);
            }
            return instance;
        }

        // ---- 4. settings

        struct PreflightSettings {
            bool remoteApiEnable = false;
            bool remoteApiLan = false;
            int remoteApiPort = RemoteApi::kDefaultPort;
            QString remoteApiKey;
            QString inboundAddress = QStringLiteral("127.0.0.1");
            int inboundPort = 2080;
            bool randomInboundPort = false;
            bool disableMixedInbound = false;
            int dnsInPort = 5533;
            int clashApiPort = -9090;
            QString clashListen = QStringLiteral("127.0.0.1");
            int coreApiPort = -9091;
            bool killSwitch = false;
            QString tunCidr = QStringLiteral("172.19.0.1/24");
            bool rememberTun = false;
        };

        // Database keys, as SettingsRepo stores them (remember_tun is tun_mode_enabled).
        constexpr const char *preflightSettingKeys[] = {
            "remote_api_enable", "remote_api_lan", "remote_api_port", "remote_api_key", "inbound_address", "inbound_socks_port",
            "random_inbound_port", "disable_mixed_inbound", "core_dns_in_port", "core_box_clash_api", "core_box_clash_listen_addr",
            "core_box_api_port", "kill_switch", "vpn_tun_ipv4_cidr", "tun_mode_enabled",
        };

        std::string preflightSettingsSql() {
            std::string sql = "SELECT key, value FROM settings WHERE key IN (";
            bool first = true;
            for (const char *key : preflightSettingKeys) {
                if (!first) sql += ", ";
                sql += "'";
                sql += key;
                sql += "'";
                first = false;
            }
            return sql + ")";
        }

        // As SettingsRepo::loadAllSettings parses them.
        void preflightApplySetting(PreflightSettings &settings, const QString &key, const QString &value) {
            const bool flag = value == QLatin1String("true") || value == QLatin1String("1");
            bool ok = false;
            int number = value.toInt(&ok);
            if (!ok) number = 0;
            if (key == QLatin1String("remote_api_enable")) settings.remoteApiEnable = flag;
            else if (key == QLatin1String("remote_api_lan")) settings.remoteApiLan = flag;
            else if (key == QLatin1String("remote_api_port")) settings.remoteApiPort = number;
            else if (key == QLatin1String("remote_api_key")) settings.remoteApiKey = value;
            else if (key == QLatin1String("inbound_address")) settings.inboundAddress = value;
            else if (key == QLatin1String("inbound_socks_port")) settings.inboundPort = number;
            else if (key == QLatin1String("random_inbound_port")) settings.randomInboundPort = flag;
            else if (key == QLatin1String("disable_mixed_inbound")) settings.disableMixedInbound = flag;
            else if (key == QLatin1String("core_dns_in_port")) settings.dnsInPort = number;
            else if (key == QLatin1String("core_box_clash_api")) settings.clashApiPort = number;
            else if (key == QLatin1String("core_box_clash_listen_addr")) settings.clashListen = value;
            else if (key == QLatin1String("core_box_api_port")) settings.coreApiPort = number;
            else if (key == QLatin1String("kill_switch")) settings.killSwitch = flag;
            else if (key == QLatin1String("vpn_tun_ipv4_cidr")) settings.tunCidr = value;
            else if (key == QLatin1String("tun_mode_enabled")) settings.rememberTun = flag;
        }

        struct PreflightDbRead {
            bool read = false;
            bool noTable = false;
            // Primary SQLite result code; -1 when the error is not SQLite's.
            int code = -1;
            QString error;
        };

        PreflightDbRead preflightQuerySettings(const std::string &name, int flags, PreflightSettings *settings) {
            PreflightDbRead result;
            try {
                SQLite::Database db(name, flags, preflightDbBusyMs);
                if (!db.tableExists("settings")) {
                    result.noTable = true;
                    return result;
                }
                PreflightSettings read;
                SQLite::Statement query(db, preflightSettingsSql());
                while (query.executeStep()) {
                    preflightApplySetting(read, QString::fromUtf8(query.getColumn(0).getText()), QString::fromUtf8(query.getColumn(1).getText()));
                }
                *settings = read;
                result.read = true;
            } catch (const SQLite::Exception &e) {
                result.code = e.getErrorCode() & 0xff;
                result.error = QString::fromUtf8(e.what());
            } catch (const std::exception &e) {
                result.error = QString::fromUtf8(e.what());
            }
            return result;
        }

        // A read-only connection still creates a missing -wal/-shm beside a WAL database (root-owned, when root runs this), and a
        // zero-length database makes SQLite delete an existing -wal; immutable reads the main file alone and never writes.
        PreflightDbRead preflightReadSettings(const QString &dbFile, PreflightSettings *settings) {
            const QFileInfo info(dbFile);
            const bool wal = info.size() > 0 && QFileInfo::exists(dbFile + QStringLiteral("-wal")) &&
                             QFileInfo::exists(dbFile + QStringLiteral("-shm"));
            PreflightDbRead walRead;
            if (wal) {
                walRead = preflightQuerySettings(dbFile.toStdString(), SQLite::OPEN_READONLY, settings);
                if (walRead.read || walRead.noTable) return walRead;
            }
            const QUrl url = QUrl::fromLocalFile(QFileInfo(dbFile).absoluteFilePath());
            if (!url.host().isEmpty()) {
                // A UNC path has an authority, which SQLite URIs refuse.
                if (wal) return walRead;
                PreflightDbRead result;
                result.error = QStringLiteral("a network path is not read without the write-ahead log");
                return result;
            }
            const std::string uri = url.toEncoded().toStdString() + "?immutable=1";
            return preflightQuerySettings(uri, SQLite::OPEN_READONLY | SQLite::OPEN_URI, settings);
        }

        PreflightSettings preflightSettings(PreflightReport &report, const Bootstrap::DataDir &dataDir, const PreflightInstance &instance) {
            PreflightSettings settings;
            const QString dbFile = QDir(dataDir.configDir).absoluteFilePath(QStringLiteral("throne.db"));
            const QString shown = preflightNative(dbFile);
            const QString advice = QStringLiteral("make %1 and its -wal and -shm files readable and writable by the user Throne runs as").arg(shown);
            const QFileInfo info(dbFile);
            if (!info.exists()) {
                report.info(QStringLiteral("settings: no database yet (%1); the checks below use the defaults").arg(shown));
            } else if (!info.isFile()) {
                report.fail(QStringLiteral("settings: %1 is not a regular file; Throne --headless cannot open it").arg(shown),
                            {QStringLiteral("move it aside")});
            } else {
                const auto read = preflightReadSettings(dbFile, &settings);
                const bool denied = Configs::DbFilesDenied(dbFile);
                if (read.read && !denied) {
                    report.ok(QStringLiteral("settings: read from %1").arg(shown));
                } else if (read.read || read.noTable) {
                    const QString what = read.read ? QStringLiteral("read from %1").arg(shown)
                                                   : QStringLiteral("none saved yet in %1; the checks below use the defaults").arg(shown);
                    if (denied) {
                        report.fail(QStringLiteral("settings: %1, but this user cannot write it (or its -wal and -shm files); Throne "
                                                   "--headless exits 78")
                                        .arg(what),
                                    {advice});
                    } else {
                        report.info(QStringLiteral("settings: ") + what);
                    }
                } else if (denied) {
                    report.fail(QStringLiteral("settings: this user cannot read and write %1 (or its -wal and -shm files); Throne --headless "
                                               "exits 78; the checks below use the defaults")
                                    .arg(shown),
                                {advice});
                } else if ((read.code == SQLITE_CORRUPT || read.code == SQLITE_NOTADB) && instance.pid <= 0) {
                    report.fail(QStringLiteral("settings: %1 is unusable (%2); Throne --headless exits 78; the checks below use the defaults")
                                    .arg(shown, read.error),
                                {QStringLiteral("move %1 and its -wal and -shm files aside to start with an empty database, or restore them "
                                                "from a backup")
                                     .arg(shown)});
                } else {
                    report.warn(QStringLiteral("settings: cannot read %1 (%2); the checks below use the defaults").arg(shown, read.error));
                }
            }
            const QString statsFile = QDir(dataDir.configDir).absoluteFilePath(QStringLiteral("throne_stats.db"));
            if (Configs::DbFilesDenied(statsFile)) {
                report.warn(QStringLiteral("settings: this user cannot read and write %1 (traffic statistics)").arg(preflightNative(statsFile)),
                            {QStringLiteral("make %1 and its -wal and -shm files readable and writable by the user Throne runs as")
                                 .arg(preflightNative(statsFile))});
            }
            return settings;
        }

        // ---- 5. TLS

        void preflightTls(PreflightReport &report) {
            Bootstrap::AddBundledPluginPath();
            if (QSslSocket::supportsSsl()) {
                report.ok(QStringLiteral("TLS: %1 (%2 backend)").arg(QSslSocket::sslLibraryVersionString(), QSslSocket::activeBackend()));
                return;
            }
            const QStringList backends = QSslSocket::availableBackends();
            QStringList fixes;
#ifdef Q_OS_LINUX
            fixes << QStringLiteral("install OpenSSL 3 (Debian and Ubuntu: libssl3; Fedora: openssl-libs; Alpine: libssl3), and keep the "
                                    "release's usr/plugins/tls directory next to Throne");
#else
            fixes << QStringLiteral("reinstall Throne: its TLS plugin is missing");
#endif
            report.fail(QStringLiteral("TLS: no usable TLS backend (%1); subscription updates and the other downloads Throne makes fail")
                            .arg(backends.isEmpty() ? QStringLiteral("none found")
                                                    : QStringLiteral("found: ") + backends.join(QStringLiteral(", "))),
                        fixes);
        }

        // ---- 6. core

        struct PreflightCore {
            QString path;
            // ThroneCore --check answered with its JSON line.
            bool reported = false;
            QJsonObject report;
        };

        std::optional<bool> preflightFlag(const QJsonObject &json, const char *key) {
            const QJsonValue value = json.value(QLatin1String(key));
            if (value.isBool()) return value.toBool();
            return std::nullopt;
        }

        int preflightInt(const QJsonObject &json, const char *key) {
            const QJsonValue value = json.value(QLatin1String(key));
            return value.isDouble() ? value.toInt(-1) : -1;
        }

        QString preflightLastLine(const QByteArray &bytes) {
            const QStringList lines = QString::fromUtf8(bytes).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
                QString line = it->trimmed();
                if (line.isEmpty()) continue;
                for (auto &ch : line) {
                    if (ch.unicode() < 0x20) ch = QLatin1Char(' ');
                }
                return line.left(240);
            }
            return {};
        }

        PreflightCore preflightCore(PreflightReport &report, const QProcessEnvironment &env) {
            PreflightCore core;
            core.path = Configs::FindCoreRealPath();
            const QString shown = preflightNative(core.path);
            const QString skipped = QStringLiteral("; the Tun and kill-switch checks are skipped");
            const QStringList buildFix{QStringLiteral("check that it was built for this system (CPU architecture, C library, OS version)")};
            const QFileInfo info(core.path);
            if (!info.exists()) {
                report.fail(QStringLiteral("core: %1 is missing").arg(shown),
                            {QStringLiteral("reinstall Throne: ThroneCore must sit next to the Throne binary")});
                return core;
            }
            if (!info.isFile() || !info.isExecutable()) {
#ifdef Q_OS_UNIX
                report.fail(QStringLiteral("core: %1 is not an executable file").arg(shown),
                            {QStringLiteral("chmod +x %1").arg(preflightQuote(core.path))});
#else
                report.fail(QStringLiteral("core: %1 is not an executable file").arg(shown),
                            {QStringLiteral("reinstall Throne")});
#endif
                return core;
            }

            QProcess process;
            QProcessEnvironment childEnv = Sys::ServiceManager::ChildEnvironment(env);
            // An older core without --check would wait for this socket instead of failing at once.
            childEnv.remove(QStringLiteral("THRONE_CORE_SOCKET"));
            childEnv.remove(QStringLiteral("THRONE_GUARD"));
            process.setProcessEnvironment(childEnv);
            process.setStandardInputFile(QProcess::nullDevice());
            Configs_sys::SetIsolatedChildren(true);
            Configs_sys::IsolateChild(&process);
            QElapsedTimer clock;
            clock.start();
            process.start(core.path, {QStringLiteral("--check")});
            if (!process.waitForStarted(preflightCoreCheckMs)) {
                report.fail(QStringLiteral("core: %1 cannot be started: %2").arg(shown, process.errorString()), buildFix);
                return core;
            }
            const int left = static_cast<int>(qMax<qint64>(100, preflightCoreCheckMs - clock.elapsed()));
            if (!process.waitForFinished(left) && process.state() != QProcess::NotRunning) {
                Configs_sys::KillChildGroup(&process);
                process.kill();
                process.waitForFinished(1000);
                report.warn(QStringLiteral("core: %1 --check did not answer within %2 s").arg(shown).arg(preflightCoreCheckMs / 1000) + skipped);
                return core;
            }
            const QByteArray out = process.readAllStandardOutput();
            const QString error = preflightLastLine(process.readAllStandardError());
            const QString errorSuffix = error.isEmpty() ? QString() : QStringLiteral(": ") + error;
            if (process.exitStatus() == QProcess::CrashExit) {
                report.fail(QStringLiteral("core: %1 crashed at start").arg(shown) + errorSuffix, buildFix);
                return core;
            }
            for (const QByteArray &line : out.split('\n')) {
                const QJsonDocument document = QJsonDocument::fromJson(line.trimmed());
                if (!document.isObject()) continue;
                core.report = document.object();
                core.reported = true;
                break;
            }
            if (!core.reported) {
                const int code = process.exitCode();
                if (error.contains(QLatin1String("parent check"))) {
                    report.fail(QStringLiteral("core: %1 refuses to run under this Throne").arg(shown) + errorSuffix,
                                {QStringLiteral("Throne and ThroneCore must come from the same release and sit in the same directory")});
                } else if (code == 0 || error.contains(QLatin1String("THRONE_CORE_SOCKET"))) {
                    report.warn(QStringLiteral("core: %1 does not support --check (an older ThroneCore?)").arg(shown) + skipped,
                                {QStringLiteral("install Throne and ThroneCore from the same release")});
                } else {
                    report.fail(QStringLiteral("core: %1 exited with code %2").arg(shown).arg(code) + errorSuffix, buildFix);
                }
                return core;
            }

            QStringList details;
            const int euid = preflightInt(core.report, "euid");
            const int ruid = preflightInt(core.report, "ruid");
            if (euid >= 0) details << QStringLiteral("euid %1").arg(euid);
            if (ruid >= 0) details << QStringLiteral("ruid %1").arg(ruid);
            if (preflightFlag(core.report, "setuid") == true) details << QStringLiteral("setuid");
            if (const QString kernel = core.report.value(QStringLiteral("kernel")).toString(); !kernel.isEmpty()) {
                details << QStringLiteral("kernel ") + kernel;
            }
            report.ok(QStringLiteral("core: %1").arg(shown) +
                      (details.isEmpty() ? QString() : QStringLiteral(" (") + details.join(QStringLiteral(", ")) + QLatin1Char(')')));
            return core;
        }

        // ---- 7. Tun

        void preflightTun(PreflightReport &report, const PreflightCore &core, const PreflightSettings &settings) {
            const QString remembered = settings.rememberTun
                                           ? QStringLiteral("; the remembered Tun mode will not come back at start (degraded: tun_unavailable)")
                                           : QString();
            const QString corePath = preflightQuote(core.path);
#if defined(Q_OS_LINUX)
            if (!core.reported) {
                report.info(QStringLiteral("Tun: not checked (the core did not report)"));
                return;
            }
            const auto tunDevice = preflightFlag(core.report, "tun_device");
            const auto netAdmin = preflightFlag(core.report, "net_admin");
            const QString detail = core.report.value(QStringLiteral("detail")).toString();
            QStringList problems;
            QStringList fixes;
            if (tunDevice == false) {
                problems << (detail.startsWith(QLatin1String("cannot open ")) ? QStringLiteral("the core ") + detail
                                                                                : QStringLiteral("the core cannot open /dev/net/tun"));
                fixes << QStringLiteral("sudo modprobe tun; in a container: --device /dev/net/tun");
            }
            if (netAdmin == false) {
                problems << QStringLiteral("the core lacks CAP_NET_ADMIN");
                fixes << preflightElevationFixes(
                    {QStringLiteral("sudo setcap cap_net_admin,cap_net_raw,cap_net_bind_service,cap_sys_ptrace,cap_dac_read_search+ep %1")
                         .arg(corePath),
                     QStringLiteral("or make the core setuid root: sudo chown root:root %1 && sudo chmod u+s %1").arg(corePath)});
                fixes << QStringLiteral("in a container: --cap-add NET_ADMIN");
            }
            if (!problems.isEmpty()) {
                report.warn(QStringLiteral("Tun: unavailable: ") + problems.join(QStringLiteral("; ")) + remembered, fixes);
            } else if (tunDevice == true && netAdmin == true) {
                report.ok(QStringLiteral("Tun: available (the core opens /dev/net/tun and has CAP_NET_ADMIN)"));
            } else {
                report.info(QStringLiteral("Tun: unknown (%1)").arg(detail.isEmpty() ? QStringLiteral("the core could not tell") : detail));
            }
#elif defined(Q_OS_MACOS)
            if (!core.reported) {
                report.info(QStringLiteral("Tun: not checked (the core did not report)"));
                return;
            }
            const int euid = preflightInt(core.report, "euid");
            if (euid == 0) {
                report.ok(QStringLiteral("Tun: available (the core runs as root)"));
            } else {
                report.warn(QStringLiteral("Tun: unavailable: the core does not run as root") + remembered,
                            preflightElevationFixes(
                                {QStringLiteral("sudo chown root:wheel %1 && sudo chmod u+s %1").arg(corePath)}));
            }
#elif defined(Q_OS_WIN)
            Q_UNUSED(core)
            Q_UNUSED(corePath)
            if (Configs::KnownAdmin().value_or(false)) {
                report.ok(QStringLiteral("Tun: available (Throne runs elevated)"));
            } else {
                report.warn(QStringLiteral("Tun: unavailable: Throne does not run elevated") + remembered,
                            {QStringLiteral("run Throne from an elevated prompt, as a task with the highest privileges, or as a Windows "
                                            "service (Throne --headless --service)")});
            }
#else
            Q_UNUSED(core)
            Q_UNUSED(corePath)
            Q_UNUSED(remembered)
            report.info(QStringLiteral("Tun: not checked on this OS"));
#endif
        }

        // ---- 8. kill switch

        void preflightKillSwitch(PreflightReport &report, const PreflightCore &core, const PreflightSettings &settings,
                                 const HeadlessCli::LaunchOptions &options) {
            QString setting;
            if (settings.killSwitch && options.noKillSwitch) {
                setting = QStringLiteral("kill_switch is on in the settings, but --no-kill-switch turns it off for this run");
            } else if (settings.killSwitch) {
                setting = QStringLiteral("kill_switch is on in the settings, so profiles stay refused until this is fixed or kill_switch is "
                                         "turned off");
            } else {
                setting = QStringLiteral("kill_switch is off in the settings");
            }
            const QString corePath = preflightQuote(core.path);
#if defined(Q_OS_LINUX)
            if (!core.reported) {
                report.info(QStringLiteral("kill switch: not checked (the core did not report); ") + setting);
                return;
            }
            const auto privilege = preflightFlag(core.report, "kill_switch_privilege");
            const auto netAdmin = preflightFlag(core.report, "net_admin");
            const auto nftables = preflightFlag(core.report, "nftables");
            const QString nftablesError = core.report.value(QStringLiteral("nftables_error")).toString();
            const int euid = preflightInt(core.report, "euid");
            QStringList problems;
            QStringList fixes;
            if (privilege == false) {
                if (euid != 0) {
                    problems << QStringLiteral("the core does not run as root");
                    fixes << preflightElevationFixes(
                        {QStringLiteral("run Throne as root (a root systemd unit), or make the core setuid root: sudo chown root:root %1 && "
                                        "sudo chmod u+s %1")
                             .arg(corePath)});
                    if (netAdmin == true) fixes << QStringLiteral("a unit that grants capabilities instead of root cannot use it");
                } else {
                    problems << QStringLiteral("the core runs as root without CAP_NET_ADMIN");
                    fixes << QStringLiteral("in a container: --cap-add NET_ADMIN");
                }
            }
            if (nftables == false) {
                problems << QStringLiteral("nftables does not work") +
                                (nftablesError.isEmpty() ? QString() : QStringLiteral(" (") + nftablesError + QLatin1Char(')'));
                fixes << QStringLiteral("sudo modprobe nf_tables (the kernel needs nftables support)");
            }
            if (!problems.isEmpty()) {
                report.warn(QStringLiteral("kill switch: unavailable: ") + problems.join(QStringLiteral("; ")) + QStringLiteral("; ") + setting,
                            fixes);
            } else if (privilege == true) {
                const QString nft = nftables == true      ? QStringLiteral(", nftables works")
                                    : nftablesError.isEmpty() ? QString()
                                                              : QStringLiteral(", nftables not checked: ") + nftablesError;
                report.ok(QStringLiteral("kill switch: available (the core runs as root with CAP_NET_ADMIN%1); ").arg(nft) + setting);
            } else {
                const QString detail = core.report.value(QStringLiteral("detail")).toString();
                report.info(QStringLiteral("kill switch: unknown (%1); ").arg(detail.isEmpty() ? QStringLiteral("the core could not tell") : detail) +
                            setting);
            }
#elif defined(Q_OS_MACOS)
            if (!core.reported) {
                report.info(QStringLiteral("kill switch: not checked (the core did not report); ") + setting);
                return;
            }
            const auto privilege = preflightFlag(core.report, "kill_switch_privilege");
            if (privilege == true) {
                report.ok(QStringLiteral("kill switch: available (the core runs as root); ") + setting);
            } else if (privilege == false) {
                report.warn(QStringLiteral("kill switch: unavailable: the core does not run as root; ") + setting,
                            preflightElevationFixes(
                                {QStringLiteral("sudo chown root:wheel %1 && sudo chmod u+s %1").arg(corePath)}));
            } else {
                report.info(QStringLiteral("kill switch: unknown (the core could not tell); ") + setting);
            }
#elif defined(Q_OS_WIN)
            Q_UNUSED(core)
            Q_UNUSED(corePath)
            if (Configs::KnownAdmin().value_or(false)) {
                report.ok(QStringLiteral("kill switch: available (Throne runs elevated); ") + setting);
            } else {
                report.warn(QStringLiteral("kill switch: unavailable: Throne does not run elevated; ") + setting,
                            {QStringLiteral("run Throne elevated, or as a Windows service (Throne --headless --service)")});
            }
#else
            Q_UNUSED(core)
            Q_UNUSED(corePath)
            report.info(QStringLiteral("kill switch: not supported on this OS; ") + setting);
#endif
        }

        // ---- 9. API key and ports

        struct PreflightApi {
            enum class Key { Settings, ExplicitFile, DefaultFile };
            bool enabled = false;
            QHostAddress bind;
            quint16 port = RemoteApi::kDefaultPort;
            QString source;
            Key key = Key::Settings;
            QString keyFile;
        };

        QString preflightApiSource(RemoteApi::ConfigSource source) {
            switch (source) {
                case RemoteApi::ConfigSource::Settings: return QStringLiteral("from the settings");
                case RemoteApi::ConfigSource::Flags: return QStringLiteral("from --api-listen");
                case RemoteApi::ConfigSource::Environment: return QStringLiteral("from THRONE_API_LISTEN");
                case RemoteApi::ConfigSource::Default: break;
            }
            return QStringLiteral("the headless default");
        }

        // RemoteApi::EffectiveConfig over the peeked settings; the overlay is never installed.
        PreflightApi preflightApiConfig(const PreflightSettings &settings, const RemoteApi::Overlay &overlay, const QString &configDir) {
            PreflightApi api;
            api.enabled = settings.remoteApiEnable;
            bool lan = settings.remoteApiLan;
            if (settings.remoteApiPort >= 1 && settings.remoteApiPort <= 65535) api.port = static_cast<quint16>(settings.remoteApiPort);
            api.source = preflightApiSource(RemoteApi::ConfigSource::Settings);
            QHostAddress address;
            if (overlay.headless && !api.enabled) {
                api.enabled = true;
                lan = false;
                api.port = RemoteApi::kDefaultPort;
                api.source = preflightApiSource(RemoteApi::ConfigSource::Default);
                api.key = PreflightApi::Key::DefaultFile;
                api.keyFile = QDir(configDir).absoluteFilePath(QStringLiteral("api.key"));
            }
            if (overlay.enabled.has_value() && !*overlay.enabled) {
                api.enabled = false;
                api.source = preflightApiSource(overlay.listenSource);
                return api;
            }
            if (overlay.port.has_value()) {
                address = overlay.address.isNull() ? QHostAddress(QHostAddress::LocalHost) : overlay.address;
                api.port = *overlay.port;
                lan = !address.isLoopback();
                api.source = preflightApiSource(overlay.listenSource);
            }
            if (!overlay.keyFile.isEmpty()) {
                api.key = PreflightApi::Key::ExplicitFile;
                api.keyFile = overlay.keyFile;
            }
            api.bind = !address.isNull() ? address : lan ? QHostAddress(QHostAddress::Any) : QHostAddress(QHostAddress::LocalHost);
            return api;
        }

        void preflightApiKey(PreflightReport &report, const PreflightApi &api, const PreflightSettings &settings,
                             const RemoteApi::Overlay &overlay) {
            if (!api.enabled) return;
            const QString shown = preflightNative(api.keyFile);
            switch (api.key) {
                case PreflightApi::Key::Settings:
                    if (RemoteApi::KeyFromText(settings.remoteApiKey).isEmpty()) {
                        report.warn(QStringLiteral("API key: remote_api_key in the settings is missing or invalid, so the Throne API will not "
                                                   "listen (key_invalid)"),
                                    {QStringLiteral("pass --api-key-file FILE (create one with: thronectl keygen -o FILE)")});
                    } else {
                        report.ok(QStringLiteral("API key: from the settings (remote_api_key)"));
                    }
                    return;
                case PreflightApi::Key::ExplicitFile: {
                    const QString source = overlay.keyFileSource == RemoteApi::ConfigSource::Flags ? QStringLiteral("--api-key-file")
                                                                                                   : QStringLiteral("THRONE_API_KEY_FILE");
                    const auto file = RemoteApi::LoadKeyFile(api.keyFile, false);
                    if (file.error.isEmpty()) {
                        report.ok(QStringLiteral("API key: %1 (%2)").arg(shown, source));
                    } else {
                        report.fail(QStringLiteral("API key: %1 (%2) is not usable: %3; Throne --headless exits 78").arg(shown, source, file.error),
                                    {QStringLiteral("create one with: thronectl keygen -o %1").arg(preflightQuote(api.keyFile))});
                    }
                    return;
                }
                case PreflightApi::Key::DefaultFile: {
                    const QFileInfo info(api.keyFile);
                    if (!info.exists() && !info.isSymLink()) {
                        report.info(QStringLiteral("API key: %1 is created on the first start").arg(shown));
                        return;
                    }
                    if (info.isFile() && info.size() == 0) {
                        report.info(QStringLiteral("API key: %1 is empty; the next start writes a new key into it").arg(shown));
                        return;
                    }
                    const auto file = RemoteApi::LoadKeyFile(api.keyFile, false);
                    if (file.error.isEmpty()) {
                        report.ok(QStringLiteral("API key: %1").arg(shown));
                    } else {
                        report.warn(QStringLiteral("API key: %1 is not usable (%2), so the Throne API may not listen").arg(shown, file.error),
                                    {QStringLiteral("delete it to have a new key created at the next start, or write one with: thronectl "
                                                    "keygen -o %1")
                                         .arg(preflightQuote(api.keyFile))});
                    }
                    return;
                }
            }
        }

        struct PreflightListener {
            QString label;
            QHostAddress address;
            int port = 0;
            bool udp = false;
            // The core binds it, with its own privileges (root, or CAP_NET_BIND_SERVICE through setcap).
            bool byCore = false;
            QStringList fixes;
            // Its address is gone at times (an explicit API address); Throne waits for it.
            bool mayWaitForAddress = false;
            // The Throne API itself, which a running instance holds only while its HEALTH says it listens there.
            bool api = false;
            // How to move it elsewhere, as a fix clause ("pick another: ...").
            QString change;
        };

        enum class PreflightBind { Free, InUse, Denied, NoAddress, Other };

        PreflightBind preflightProbe(const PreflightListener &listener, QString *error) {
            QAbstractSocket::SocketError code = QAbstractSocket::UnknownSocketError;
            if (listener.udp) {
                QUdpSocket socket;
                if (socket.bind(listener.address, static_cast<quint16>(listener.port), QAbstractSocket::DontShareAddress)) {
                    socket.close();
                    return PreflightBind::Free;
                }
                code = socket.error();
                *error = socket.errorString();
            } else {
                QTcpServer server;
                if (server.listen(listener.address, static_cast<quint16>(listener.port))) {
                    server.close();
                    return PreflightBind::Free;
                }
                code = server.serverError();
                *error = server.errorString();
            }
            switch (code) {
                case QAbstractSocket::AddressInUseError: return PreflightBind::InUse;
                case QAbstractSocket::SocketAccessError: return PreflightBind::Denied;
                case QAbstractSocket::SocketAddressNotAvailableError: return PreflightBind::NoAddress;
                default: break;
            }
            return PreflightBind::Other;
        }

        bool preflightWildcard(const QHostAddress &address) {
            return address == QHostAddress::Any || address == QHostAddress::AnyIPv4 || address == QHostAddress::AnyIPv6;
        }

        QString preflightEndpoint(const QHostAddress &address, int port) {
            if (address == QHostAddress::Any || address == QHostAddress::AnyIPv6) return QStringLiteral("*:%1").arg(port);
            if (address.protocol() == QAbstractSocket::IPv6Protocol) return QStringLiteral("[%1]:%2").arg(address.toString()).arg(port);
            return QStringLiteral("%1:%2").arg(address.toString()).arg(port);
        }

        // An IP literal as sing-box takes it; brackets around IPv6 are tolerated.
        std::optional<QHostAddress> preflightListenAddress(const QString &text, const QString &fallback) {
            QString value = text.trimmed();
            if (value.isEmpty()) value = fallback;
            if (value.startsWith(QLatin1Char('[')) && value.endsWith(QLatin1Char(']'))) value = value.mid(1, value.size() - 2);
            QHostAddress address;
            if (!address.setAddress(value)) return std::nullopt;
            return address;
        }

        // Go's net.Listen on a host name listens on its first IPv4 address, else on its first address.
        std::optional<QHostAddress> preflightResolveListenHost(const QString &name) {
            const QList<QHostAddress> addresses = QHostInfo::fromName(name).addresses();
            for (const auto &candidate : addresses) {
                if (candidate.protocol() == QAbstractSocket::IPv4Protocol) return candidate;
            }
            if (addresses.isEmpty()) return std::nullopt;
            return addresses.first();
        }

        // HEALTH's api.listen ("*:P", "[V6]:P" or "V4:P") overlaps the probed endpoint: the same port, and the same address or a
        // wildcard on either side.
        bool preflightApiListenCovers(const QString &listen, const QHostAddress &address, int port) {
            RemoteApi::Overlay parsed;
            QString error;
            if (!RemoteApi::ParseListen(listen, &parsed, &error) || !parsed.port.has_value() || int(*parsed.port) != port) return false;
            return preflightWildcard(parsed.address) || preflightWildcard(address) ||
                   parsed.address.isEqual(address, QHostAddress::TolerantConversion);
        }

        bool preflightHeldByInstance(const PreflightInstance &instance, const PreflightListener &listener) {
            if (!instance.health) return false;
            if (listener.api) {
                return (instance.apiState == QLatin1String("listening") || instance.apiState == QLatin1String("address_missing")) &&
                       preflightApiListenCovers(instance.apiListen, listener.address, listener.port);
            }
            return listener.byCore && instance.coreBound;
        }

        // Why a running headless owner is not the holder, for the FAIL line.
        QString preflightNotHeldNote(const PreflightInstance &instance, const PreflightListener &listener) {
            const QString owner = QStringLiteral("; the headless Throne of this data dir (PID %1)").arg(instance.pid);
            if (!listener.api) return owner + QStringLiteral(" runs no profile now");
            QString note;
            if (instance.apiState == QLatin1String("listening") || instance.apiState == QLatin1String("address_missing")) {
                note = owner + QStringLiteral(" has its API on %1").arg(instance.apiListen);
            } else if (instance.apiState == QLatin1String("off")) {
                note = owner + QStringLiteral(" has its API off");
            } else {
                note = owner + QStringLiteral(" has its API not listening (%1)").arg(instance.apiState);
            }
            if (!instance.apiError.isEmpty()) note += QStringLiteral(": ") + instance.apiError;
            return note;
        }

        void preflightPorts(PreflightReport &report, const PreflightSettings &settings, const RemoteApi::Overlay &overlay,
                            const Bootstrap::DataDir &dataDir, const PreflightInstance &instance) {
            const PreflightApi api = preflightApiConfig(settings, overlay, dataDir.configDir);
            preflightApiKey(report, api, settings, overlay);

            const auto heldFixes = [](const QString &change) {
                return QStringList{QStringLiteral("stop the program that holds it, or ") + change};
            };
            const auto coreListener = [&heldFixes](const QString &label, const QHostAddress &address, int port, bool udp,
                                                   const QString &change) {
                PreflightListener listener;
                listener.label = label;
                listener.address = address;
                listener.port = port;
                listener.udp = udp;
                listener.byCore = true;
                listener.change = change;
                listener.fixes = heldFixes(change);
                return listener;
            };

            QList<PreflightListener> listeners;
            if (api.enabled) {
                PreflightListener listener;
                listener.label = QStringLiteral("the Throne API, ") + api.source;
                listener.address = api.bind;
                listener.port = api.port;
                listener.change = QStringLiteral("pick another: --api-listen 127.0.0.1:PORT");
                listener.fixes = heldFixes(listener.change);
                listener.mayWaitForAddress = overlay.port.has_value() && !preflightWildcard(api.bind) && !api.bind.isLoopback();
                listener.api = true;
                listeners << listener;
            } else {
                report.info(QStringLiteral("port: the Throne API is off (%1)").arg(api.source));
            }

            if (settings.disableMixedInbound) {
                report.info(QStringLiteral("port: the mixed proxy is off in the settings"));
            } else if (settings.randomInboundPort) {
                report.info(QStringLiteral("port: the mixed proxy takes a random free port at each start"));
            } else if (const auto address = preflightListenAddress(settings.inboundAddress, QStringLiteral("127.0.0.1")); !address) {
                report.fail(QStringLiteral("port: the mixed proxy listen address \"%1\" is not an IP address, so profiles fail to start")
                                .arg(settings.inboundAddress),
                            {QStringLiteral("thronectl settings set inbound_address=127.0.0.1")});
            } else {
                listeners << coreListener(QStringLiteral("the mixed proxy, inbound_port"), *address, settings.inboundPort, false,
                                          QStringLiteral("pick another: thronectl settings set inbound_port=PORT"));
            }

            const QString dnsLabel = QStringLiteral("the core's DNS server, dns_in_port");
            const QString dnsChange = QStringLiteral("pick another: thronectl settings set dns_in_port=PORT");
            listeners << coreListener(dnsLabel, QHostAddress(QHostAddress::LocalHost), settings.dnsInPort, false, dnsChange);
            listeners << coreListener(dnsLabel, QHostAddress(QHostAddress::LocalHost), settings.dnsInPort, true, dnsChange);
            if (settings.clashApiPort > 0) {
                const QString clashLabel = QStringLiteral("the Clash API, clash_api_port");
                const QString clashChange =
                    QStringLiteral("pick another: thronectl settings set clash_api_port=PORT (0 turns the Clash API off)");
                const QStringList clashAddressFix{QStringLiteral("thronectl settings set clash_listen_addr=127.0.0.1, or turn the Clash API "
                                                                 "off: thronectl settings set clash_api_port=0")};
                const QString clashName = settings.clashListen.trimmed();
                if (const auto address = preflightListenAddress(settings.clashListen, QStringLiteral("127.0.0.1")); address) {
                    listeners << coreListener(clashLabel, *address, settings.clashApiPort, false, clashChange);
                } else if (!Validation::Host(clashName, Validation::Empty::Allowed).isEmpty()) {
                    report.fail(QStringLiteral("port: the Clash API listen address \"%1\" is neither an IP address nor a host name, so profiles "
                                               "fail to start")
                                    .arg(settings.clashListen),
                                clashAddressFix);
                } else if (const auto resolved = preflightResolveListenHost(clashName); resolved) {
                    listeners << coreListener(clashLabel + QStringLiteral(", ") + clashName, *resolved, settings.clashApiPort, false,
                                              clashChange);
                } else {
                    report.fail(QStringLiteral("port: the Clash API listen address \"%1\" does not resolve, so profiles fail to start").arg(clashName),
                                clashAddressFix);
                }
            }
            if (settings.coreApiPort > 0) {
                listeners << coreListener(QStringLiteral("the sing-box API, singbox_api_port"), QHostAddress(QHostAddress::LocalHost),
                                          settings.coreApiPort, false,
                                          QStringLiteral("pick another: thronectl settings set singbox_api_port=PORT (0 turns the sing-box API off)"));
            }

            for (qsizetype i = 0; i < listeners.size(); ++i) {
                const auto &listener = listeners.at(i);
                const QString where = preflightEndpoint(listener.address, listener.port) + (listener.udp ? QStringLiteral("/udp") : QString());
                if (listener.port < 1 || listener.port > 65535) {
                    report.fail(QStringLiteral("port %1 (%2): not a usable port number").arg(listener.port).arg(listener.label),
                                {listener.change});
                    continue;
                }
                QString clash;
                for (qsizetype j = 0; j < i && clash.isEmpty(); ++j) {
                    const auto &other = listeners.at(j);
                    if (other.port == listener.port && other.udp == listener.udp && other.label != listener.label &&
                        (preflightWildcard(other.address) || preflightWildcard(listener.address) ||
                         other.address.isEqual(listener.address, QHostAddress::TolerantConversion))) {
                        clash = other.label;
                    }
                }
                if (!clash.isEmpty()) {
                    report.fail(QStringLiteral("port %1 (%2): also set for %3").arg(where, listener.label, clash),
                                {QStringLiteral("give each its own port")});
                    continue;
                }
                QString error;
                switch (preflightProbe(listener, &error)) {
                    case PreflightBind::Free:
                        report.ok(QStringLiteral("port %1 (%2): free").arg(where, listener.label));
                        break;
                    case PreflightBind::InUse: {
                        const QString inUse = QStringLiteral("port %1 (%2): in use").arg(where, listener.label);
                        if (preflightHeldByInstance(instance, listener)) {
                            report.ok(inUse + QStringLiteral(" by PID %1 (%2)").arg(QString::number(instance.pid), instance.who));
                        } else if (instance.health) {
                            report.fail(inUse + QStringLiteral(" by another program") + preflightNotHeldNote(instance, listener), listener.fixes);
                        } else if (instance.pid > 0) {
                            report.warn(inUse + QStringLiteral(", perhaps by PID %1 (%2)").arg(QString::number(instance.pid), instance.who),
                                        listener.fixes);
                        } else {
                            report.fail(inUse + QStringLiteral(" by another program"), listener.fixes);
                        }
                        break;
                    }
                    case PreflightBind::Denied:
                        if (listener.byCore) {
                            report.warn(QStringLiteral("port %1 (%2): this user may not listen on it; the core can only as root or with "
                                                       "CAP_NET_BIND_SERVICE")
                                            .arg(where, listener.label),
                                        listener.fixes);
                        } else {
                            report.fail(QStringLiteral("port %1 (%2): this user may not listen on it (ports below 1024 need root or "
                                                       "CAP_NET_BIND_SERVICE)")
                                            .arg(where, listener.label),
                                        listener.fixes);
                        }
                        break;
                    case PreflightBind::NoAddress:
                        if (listener.mayWaitForAddress) {
                            report.warn(QStringLiteral("port %1 (%2): the address is not on this computer now; Throne --headless waits up "
                                                       "to 30 s for it at start")
                                            .arg(where, listener.label));
                        } else {
                            report.fail(QStringLiteral("port %1 (%2): the address is not on this computer").arg(where, listener.label),
                                        listener.fixes);
                        }
                        break;
                    case PreflightBind::Other:
                        report.warn(QStringLiteral("port %1 (%2): cannot be probed: %3").arg(where, listener.label, error));
                        break;
                }
            }
        }

        // ---- 10. host

        // HostTun::Conflict's address rule: the name of the Up, non-loopback interface carrying the Tun host address when it is a LAN,
        // bridge or container network (Ethernet or Wi-Fi type); empty when none carries it or a tun-like one does.
        QString preflightTunLanOwner(const QString &tunCidr) {
            const QHostAddress address(tunCidr.section(QLatin1Char('/'), 0, 0).trimmed());
            if (address.protocol() != QAbstractSocket::IPv4Protocol) return {};
            for (const auto &netif : QNetworkInterface::allInterfaces()) {
                const auto flags = netif.flags();
                if (!flags.testFlag(QNetworkInterface::IsUp) || flags.testFlag(QNetworkInterface::IsLoopBack)) continue;
                bool carries = false;
                for (const auto &entry : netif.addressEntries()) {
                    if (entry.ip().isEqual(address, QHostAddress::ConvertV4MappedToIPv4)) {
                        carries = true;
                        break;
                    }
                }
                if (!carries) continue;
                // A Throne Tun adapter is ours whatever type the OS gives it (wintun may not report a tun-like one).
                const bool throneTun = netif.name().startsWith(QLatin1String(Sys::HostTun::kTunName), Qt::CaseInsensitive) ||
                                       netif.humanReadableName().startsWith(QLatin1String(Sys::HostTun::kTunName), Qt::CaseInsensitive);
                // Wifi is Ieee80211.
                const bool lanLike = netif.type() == QNetworkInterface::Ethernet || netif.type() == QNetworkInterface::Wifi;
                return !throneTun && lanLike ? netif.humanReadableName() : QString();
            }
            return {};
        }

        void preflightHost(PreflightReport &report, const PreflightSettings &settings, const Bootstrap::DataDir &dataDir,
                           const PreflightInstance &instance) {
#ifdef Q_OS_WIN
            constexpr Qt::CaseSensitivity pathCase = Qt::CaseInsensitive;
#else
            constexpr Qt::CaseSensitivity pathCase = Qt::CaseSensitive;
#endif
            if (const auto other = Sys::HostPresence::OtherHeadless(); other.has_value()) {
                if (QDir::cleanPath(other->dataDir).compare(QDir::cleanPath(dataDir.wd.absolutePath()), pathCase) == 0) {
                    report.info(QStringLiteral("host: the headless Throne of this data dir runs (PID %1)").arg(other->pid));
                } else {
                    report.info(QStringLiteral("host: another headless Throne runs on this computer (PID %1, data dir %2, version %3); only "
                                               "one Throne can run Tun at a time")
                                    .arg(QString::number(other->pid), preflightNative(other->dataDir), other->version));
                }
            } else {
                report.info(QStringLiteral("host: no other headless Throne on this computer"));
            }
            const QString conflict = Sys::HostTun::Conflict(settings.tunCidr, 0);
            if (conflict.isEmpty()) {
                report.ok(QStringLiteral("Tun owner: nothing else uses %1 or %2").arg(QLatin1String(Sys::HostTun::kTunName), settings.tunCidr));
                return;
            }
            const QString lanOwner = preflightTunLanOwner(settings.tunCidr);
            if (lanOwner.isEmpty() && instance.health && instance.tunOn) {
                report.info(QStringLiteral("Tun owner: %1 (%2, PID %3, runs Tun)").arg(conflict, instance.who, QString::number(instance.pid)));
            } else if (lanOwner.isEmpty() && !instance.health && instance.pid > 0) {
                report.info(QStringLiteral("Tun owner: %1 (perhaps %2, PID %3)").arg(conflict, instance.who, QString::number(instance.pid)));
            } else if (!lanOwner.isEmpty()) {
                report.warn(QStringLiteral("Tun owner: %1").arg(conflict),
                            {QStringLiteral("%1 is a LAN, bridge or container network (such as Docker's), not a VPN: thronectl settings set "
                                            "tun_ipv4_cidr=CIDR with a range no network interface here uses")
                                 .arg(lanOwner)});
            } else {
                report.warn(QStringLiteral("Tun owner: %1").arg(conflict),
                            {QStringLiteral("stop the other Throne or VPN before turning Tun on; when it only holds the Tun address, "
                                            "thronectl settings set tun_ipv4_cidr=CIDR also works")});
            }
        }

        // ---- 11. supervisor

        void preflightSupervisor(PreflightReport &report, const HeadlessCli::LaunchOptions &options, const QProcessEnvironment &env) {
#ifdef Q_OS_UNIX
            const qint64 parentPid = ::getppid();
#else
            const qint64 parentPid = 0;
#endif
            QStringList parts;
            if (!env.value(QStringLiteral("NOTIFY_SOCKET")).isEmpty()) {
#ifdef Q_OS_LINUX
                parts << QStringLiteral("systemd notify socket (READY, STATUS and STOPPING are sent: Type=notify works)");
#else
                parts << QStringLiteral("NOTIFY_SOCKET is set, but only Linux uses it");
#endif
            }
            bool ok = false;
            const qint64 watchdogUsec = env.value(QStringLiteral("WATCHDOG_USEC")).toLongLong(&ok);
            if (ok && watchdogUsec > 0) {
                QString text = QStringLiteral("watchdog every %1 s").arg(QString::number(static_cast<double>(watchdogUsec) / 1e6, 'g', 6));
                if (const QString pid = env.value(QStringLiteral("WATCHDOG_PID")); !pid.isEmpty()) text += QStringLiteral(" for PID ") + pid;
                parts << text;
            }
            if (!env.value(QStringLiteral("INVOCATION_ID")).isEmpty()) {
                parts << QStringLiteral("INVOCATION_ID is set (a systemd unit, or a process one started)");
            }
            QString why;
            auto mode = HeadlessCli::EffectiveRestartMode(options.restartMode, env, parentPid, &why);
#ifdef Q_OS_WIN
            if (options.service) {
                parts << QStringLiteral("the Windows SCM (--service: start progress, RUNNING and the exit code are reported)");
                mode = HeadlessCli::RestartMode::Exit;
                why = QStringLiteral("--service: the service's recovery actions start Throne again (sc failure, sc failureflag 1)");
            } else if (parts.isEmpty()) {
                parts << QStringLiteral("none (Throne --headless --service runs under the Windows SCM, where a restart request exits with 75 "
                                        "for the service's recovery actions)");
            }
            const QString execText = QStringLiteral("starts Throne again as a new process, which waits for this one to exit");
#else
            if (parts.isEmpty()) parts << QStringLiteral("no service manager detected");
            const QString execText = QStringLiteral("re-executes Throne in place");
#endif
            const QString restart = mode == HeadlessCli::RestartMode::Exec
                                        ? execText
                                        : QStringLiteral("exits with 75 for the service manager to start Throne again");
            report.info(QStringLiteral("supervisor: %1; a restart request %2 (restart mode %3: %4)")
                            .arg(parts.join(QStringLiteral(", ")), restart, HeadlessCli::RestartModeName(mode), why));
        }

        // ---- 12. hwid

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
        // DeviceDetailsHelper's machine-id rule: 32 hex digits.
        bool preflightMachineIdFile(const QString &path) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) return false;
            const QByteArray id = file.read(256).trimmed();
            if (id.size() != 32) return false;
            for (const char c : id) {
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
            }
            return true;
        }
#endif

        void preflightHwid(PreflightReport &report, const Bootstrap::DataDir &dataDir) {
#if defined(Q_OS_WIN)
            Q_UNUSED(dataDir)
            report.info(QSysInfo::machineUniqueId().isEmpty() ? QStringLiteral("hwid: the host name (no Windows MachineGuid)")
                                                              : QStringLiteral("hwid: the Windows MachineGuid"));
#elif defined(Q_OS_MACOS)
            Q_UNUSED(dataDir)
            report.info(QSysInfo::machineUniqueId().isEmpty()
                            ? QStringLiteral("hwid: none (no IOPlatformUUID); subscriptions are fetched without a device ID")
                            : QStringLiteral("hwid: the IOPlatformUUID"));
#else
#ifdef Q_OS_LINUX
            for (const char *path : {"/etc/machine-id", "/var/lib/dbus/machine-id"}) {
                if (preflightMachineIdFile(QString::fromLatin1(path))) {
                    report.info(QStringLiteral("hwid: %1").arg(QString::fromLatin1(path)));
                    return;
                }
            }
#else
            if (!QSysInfo::machineUniqueId().isEmpty()) {
                report.info(QStringLiteral("hwid: the system host ID"));
                return;
            }
#endif
            const QString stored = QDir(dataDir.configDir).absoluteFilePath(QStringLiteral("hwid"));
            if (preflightMachineIdFile(stored)) {
                report.info(QStringLiteral("hwid: %1 (no usable machine-id)").arg(preflightNative(stored)));
            } else if (preflightCreatableOrWritable(dataDir.configDir)) {
                report.info(QStringLiteral("hwid: no usable machine-id; the first start generates one into %1").arg(preflightNative(stored)));
            } else {
                report.warn(QStringLiteral("hwid: no usable machine-id, and %1 cannot be written, so subscriptions are fetched without a "
                                           "device ID")
                                .arg(preflightNative(stored)),
                            {QStringLiteral("sudo systemd-machine-id-setup, or make the data dir writable")});
            }
#endif
        }
    } // namespace

    int Run(const HeadlessCli::LaunchOptions &options, const QProcessEnvironment &env, const RemoteApi::Overlay &overlay,
            const Bootstrap::DataDir &dataDir) {
        PreflightReport report;
        preflightBuild(report);
        preflightDataDir(report, options, env, dataDir);
        const PreflightInstance instance = preflightInstance(report, dataDir);
        const PreflightSettings settings = preflightSettings(report, dataDir, instance);
        preflightTls(report);
        const PreflightCore core = preflightCore(report, env);
        preflightTun(report, core, settings);
        preflightKillSwitch(report, core, settings, options);
        preflightPorts(report, settings, overlay, dataDir, instance);
        preflightHost(report, settings, dataDir, instance);
        preflightSupervisor(report, options, env);
        preflightHwid(report, dataDir);
        preflightPrint(QStringLiteral("result: %1 failed, %2 %3").arg(report.failures()).arg(report.warnings())
                           .arg(report.warnings() == 1 ? QStringLiteral("warning") : QStringLiteral("warnings")));
        return report.failures() > 0 ? ExitCode::Fatal : ExitCode::Ok;
    }
} // namespace Engine::Preflight
