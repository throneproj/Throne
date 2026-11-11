#include "include/sys/CoreElevation.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QStringList>
#include <QtEndian>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"
#include "include/global/Utils.hpp"
#include "include/sys/AutoRun.hpp"
#include "include/sys/windows/MiniDump.h"
#include "include/sys/windows/guihelper.h"
#include "3rdparty/WinCommander.hpp"
#include "NkrVersion.h"

#include <windows.h>
#include <psapi.h>
#include <sddl.h>
#include <shlobj.h>
#include "include/sys/windows/TaskSchedulerCom.h"

namespace Sys::CoreElevation {
    namespace {
        // FOLDERID_ProgramFiles, spelled out so no uuid import library is needed.
        constexpr GUID kCoreElevationProgramFiles = {0x905e63b6, 0xc1bf, 0x494e, {0xb2, 0x9c, 0x65, 0xb7, 0x32, 0xd3, 0xd2, 0x1a}};
        constexpr char kCoreElevationHelperFlag[] = "--elevated-helper";
        constexpr DWORD kCoreElevationGuiWaitMs = 60 * 1000;
        constexpr DWORD kCoreElevationUpdaterWaitMs = 10 * 60 * 1000;

        std::mutex coreElevationMutex;
        std::optional<Status> coreElevationStatus;
        std::atomic<bool> coreElevationGrantedCoreUnprivileged = false;

        struct CoreElevationInstall {
            QString gui;
            QString core;
            QString dir;
            QString sid;
            QString taskName;
            QString grantDir;
            QString updateDir;
        };

        QString coreElevationUserSid() {
            static const QString sid = [] {
                QString result;
                HANDLE token = nullptr;
                if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return result;
                DWORD size = 0;
                GetTokenInformation(token, TokenUser, nullptr, 0, &size);
                std::vector<BYTE> buffer(size);
                LPWSTR text = nullptr;
                if (size > 0 && GetTokenInformation(token, TokenUser, buffer.data(), size, &size)
                    && ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(buffer.data())->User.Sid, &text)) {
                    result = QString::fromWCharArray(text);
                    LocalFree(text);
                }
                CloseHandle(token);
                return result;
            }();
            return sid;
        }

        bool coreElevationQueryToken(TOKEN_INFORMATION_CLASS type, void *out, DWORD size) {
            HANDLE token = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
            DWORD returned = 0;
            const bool ok = GetTokenInformation(token, type, out, size, &returned);
            CloseHandle(token);
            return ok;
        }

        bool coreElevationIsElevated() {
            TOKEN_ELEVATION elevation{};
            return coreElevationQueryToken(TokenElevation, &elevation, sizeof(elevation)) && elevation.TokenIsElevated;
        }

        QString coreElevationBaseDir() {
            PWSTR raw = nullptr;
            QString programFiles;
            if (SUCCEEDED(SHGetKnownFolderPath(kCoreElevationProgramFiles, 0, nullptr, &raw)) && raw != nullptr) {
                programFiles = QString::fromWCharArray(raw);
            }
            CoTaskMemFree(raw);
            if (programFiles.isEmpty()) return {};
            return QDir::toNativeSeparators(QDir::cleanPath(programFiles)) + "\\Throne Core";
        }

        CoreElevationInstall coreElevationInstallFor(const QString &throneExe, const QString &core, const QString &sid) {
            CoreElevationInstall install;
            install.gui = QDir::toNativeSeparators(QDir::cleanPath(throneExe));
            install.core = QDir::toNativeSeparators(core);
            install.dir = QDir::toNativeSeparators(QFileInfo(install.gui).absolutePath());
            install.sid = sid;
            // The autorun task's naming scheme, so one install of one user maps to one task and one folder.
            const QString seed = QDir::cleanPath(throneExe).toLower() + "|" + sid;
            const QString key = QString::fromLatin1(QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Md5).toHex().left(8));
            install.taskName = "Throne Core " + key;
            const QString base = coreElevationBaseDir();
            if (!base.isEmpty() && !sid.isEmpty()) {
                install.grantDir = base + "\\" + key;
                install.updateDir = base + "\\update-" + key;
            }
            return install;
        }

        CoreElevationInstall coreElevationLocalInstall(const QString &sid) {
            return coreElevationInstallFor(QCoreApplication::applicationFilePath(), Configs::FindCoreRealPath(), sid);
        }

        DWORD coreElevationAttributes(const QString &path) {
            return GetFileAttributesW(qUtf16Printable(path));
        }

        bool coreElevationExists(const QString &path) {
            return !path.isEmpty() && coreElevationAttributes(path) != INVALID_FILE_ATTRIBUTES;
        }

        bool coreElevationStamp(const QString &path, qint64 *size, qint64 *mtimeMs) {
            WIN32_FILE_ATTRIBUTE_DATA data{};
            if (!GetFileAttributesExW(qUtf16Printable(path), GetFileExInfoStandard, &data)
                || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
            *size = qint64((quint64(data.nFileSizeHigh) << 32) | data.nFileSizeLow);
            const qint64 ticks = qint64((quint64(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime);
            // FILETIME counts 100 ns ticks from 1601-01-01 UTC.
            *mtimeMs = (ticks - 116444736000000000LL) / 10000;
            return true;
        }

        bool coreElevationTaskExists(const QString &name) {
            const TaskRoot root;
            return SUCCEEDED(root.status()) && root.find(name);
        }

        Status coreElevationEvaluate() {
            const CoreElevationInstall install = coreElevationLocalInstall(coreElevationUserSid());
            if (install.grantDir.isEmpty()) return Status::NotGranted;
            const bool task = coreElevationTaskExists(install.taskName);
            QFile file(install.grantDir + "\\grant.json");
            const bool recorded = file.open(QIODevice::ReadOnly);
            if (!task && !recorded) return Status::NotGranted;
            if (!task || !recorded) return Status::Stale;
            const QJsonObject record = QJsonDocument::fromJson(file.read(64 * 1024)).object();
            qint64 size = 0;
            qint64 mtime = 0;
            const bool current = coreElevationExists(install.grantDir + "\\ThroneCore.exe") && coreElevationStamp(install.core, &size, &mtime)
                && record.value("size").toInteger(-1) == size && record.value("mtime").toInteger(-1) == mtime
                && record.value("core").toString().compare(install.core, Qt::CaseInsensitive) == 0;
            return current ? Status::Granted : Status::Stale;
        }

        // WinCommander quotes every argument, so a trailing backslash would escape the closing quote.
        QString coreElevationArgPath(const QString &path) {
            QString native = QDir::toNativeSeparators(path);
            while (native.endsWith('\\')) native.chop(1);
            return native;
        }

        int coreElevationLaunch(const QStringList &arguments) {
            // ShellExecuteEx may hand the verb to shell extensions, which need COM on the calling thread.
            ComApartment apartment;
            const uint code = WinCommander::runProcessElevated(QDir::toNativeSeparators(QCoreApplication::applicationFilePath()),
                                                               QStringList{QString::fromLatin1(kCoreElevationHelperFlag)} + arguments,
                                                               QCoreApplication::applicationDirPath(), WinCommander::ShowHide, true);
            return code == uint(-1) ? -1 : int(code);
        }

        // A running image can be renamed but neither replaced nor deleted; the renamed file goes at the next reboot.
        bool coreElevationSetAside(const QString &path) {
            const QFileInfo info(path);
            const QString stem = QDir::toNativeSeparators(info.absolutePath()) + "\\" + info.completeBaseName();
            for (int n = 1; n < 1000; ++n) {
                const QString aside = QString("%1.%2.old").arg(stem, QString::number(n));
                if (MoveFileExW(qUtf16Printable(path), qUtf16Printable(aside), 0)) {
                    MoveFileExW(qUtf16Printable(aside), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
                    return true;
                }
                if (const DWORD error = GetLastError(); error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS) return false;
            }
            return false;
        }

        bool coreElevationRemoveDir(const QString &dir) {
            if (dir.isEmpty()) return true;
            const DWORD attributes = coreElevationAttributes(dir);
            if (attributes == INVALID_FILE_ATTRIBUTES) return true;
            if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) return RemoveDirectoryW(qUtf16Printable(dir)) || DeleteFileW(qUtf16Printable(dir));
            if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) return DeleteFileW(qUtf16Printable(dir));
            bool pending = false;
            for (const QString &name : QDir(dir).entryList(QDir::Files | QDir::Hidden | QDir::System)) {
                const QString path = dir + "\\" + name;
                if (DeleteFileW(qUtf16Printable(path))) continue;
                if (name.endsWith(".old", Qt::CaseInsensitive)) {
                    if (!MoveFileExW(qUtf16Printable(path), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT)) return false;
                } else if (!coreElevationSetAside(path)) {
                    return false;
                }
                pending = true;
            }
            if (RemoveDirectoryW(qUtf16Printable(dir))) return true;
            return pending && MoveFileExW(qUtf16Printable(dir), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        }

        // Program Files' inherited ACL leaves these writable by administrators only; a link in their place is refused.
        bool coreElevationMakeDir(const QString &dir) {
            if (dir.isEmpty()) return false;
            if (!CreateDirectoryW(qUtf16Printable(dir), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
            const DWORD attributes = coreElevationAttributes(dir);
            return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);
        }

        bool coreElevationWriteFile(const QString &path, const QByteArray &data) {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
            const bool ok = file.write(data) == data.size() && file.flush();
            file.close();
            if (!ok) file.remove();
            return ok;
        }

        // ThroneCore is a console program; as a GUI one, Task Scheduler starts it without a console window.
        bool coreElevationPatchSubsystem(QByteArray &image) {
            if (image.size() < 0x40 || image.at(0) != 'M' || image.at(1) != 'Z') return false;
            const qint64 header = qFromLittleEndian<quint32>(image.constData() + 0x3C);
            const qint64 optional = header + 24;
            if (optional + 70 > image.size() || std::memcmp(image.constData() + header, "PE\0\0", 4) != 0) return false;
            const quint16 optionalSize = qFromLittleEndian<quint16>(image.constData() + header + 20);
            const quint16 magic = qFromLittleEndian<quint16>(image.constData() + optional);
            if ((magic != 0x10b && magic != 0x20b) || optionalSize < 70) return false;
            // Subsystem sits at offset 68 of both the PE32 and the PE32+ optional header.
            char *subsystem = image.data() + optional + 68;
            if (qFromLittleEndian<quint16>(subsystem) != 3) return false;
            qToLittleEndian<quint16>(2, subsystem);
            return true;
        }

        QString coreElevationTaskXml(const CoreElevationInstall &install) {
            // The elevated core trusts these paths because only an administrator can have registered this task.
            const QJsonObject trusted{{"core", install.core}, {"gui", install.gui}};
            const QByteArray blob = QJsonDocument(trusted).toJson(QJsonDocument::Compact)
                                        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
            const QString author = qEnvironmentVariable("USERDOMAIN") + "\\" + qEnvironmentVariable("USERNAME");
            const QString command = install.grantDir + "\\ThroneCore.exe";
            return QString(
                "<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
                "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
                "  <RegistrationInfo>\n"
                "    <Author>%1</Author>\n"
                "  </RegistrationInfo>\n"
                "  <Principals>\n"
                "    <Principal id=\"Author\">\n"
                "      <UserId>%2</UserId>\n"
                "      <LogonType>InteractiveToken</LogonType>\n"
                "      <RunLevel>HighestAvailable</RunLevel>\n"
                "    </Principal>\n"
                "  </Principals>\n"
                "  <Settings>\n"
                "    <MultipleInstancesPolicy>Parallel</MultipleInstancesPolicy>\n"
                "    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\n"
                "    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\n"
                "    <AllowHardTerminate>true</AllowHardTerminate>\n"
                "    <AllowStartOnDemand>true</AllowStartOnDemand>\n"
                "    <Enabled>true</Enabled>\n"
                "    <Hidden>false</Hidden>\n"
                "    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\n"
                "    <Priority>5</Priority>\n"
                "  </Settings>\n"
                "  <Actions Context=\"Author\">\n"
                "    <Exec>\n"
                "      <Command>\"%3\"</Command>\n"
                "      <Arguments>--attach %4 $(Arg0)</Arguments>\n"
                "    </Exec>\n"
                "  </Actions>\n"
                "</Task>"
            ).arg(author.toHtmlEscaped(), install.sid.toHtmlEscaped(), command.toHtmlEscaped(), QString::fromLatin1(blob));
        }

        int coreElevationGrant(const CoreElevationInstall &install, const QString &crashDir) {
            if (install.grantDir.isEmpty() || !coreElevationMakeDir(coreElevationBaseDir()) || !coreElevationMakeDir(install.grantDir)) {
                return HelperCopyFailed;
            }
            qint64 size = 0;
            qint64 mtime = 0;
            QFile source(install.core);
            if (!coreElevationStamp(install.core, &size, &mtime) || !source.open(QIODevice::ReadOnly)) return HelperCopyFailed;
            QByteArray image = source.readAll();
            source.close();
            if (image.size() != size || !coreElevationPatchSubsystem(image)) return HelperCopyFailed;

            for (const QString &name : QDir(install.grantDir).entryList({"*.old", "*.tmp"}, QDir::Files | QDir::Hidden | QDir::System)) {
                DeleteFileW(qUtf16Printable(install.grantDir + "\\" + name));
            }
            const QString target = install.grantDir + "\\ThroneCore.exe";
            const QString temp = target + ".tmp";
            if (!coreElevationWriteFile(temp, image)) return HelperCopyFailed;
            if (!MoveFileExW(qUtf16Printable(temp), qUtf16Printable(target), MOVEFILE_REPLACE_EXISTING)
                && !(coreElevationSetAside(target) && MoveFileExW(qUtf16Printable(temp), qUtf16Printable(target), 0))) {
                DeleteFileW(qUtf16Printable(temp));
                return HelperCopyFailed;
            }

            const QJsonObject record{
                {"core", install.core},
                {"gui", install.gui},
                {"size", size},
                {"mtime", mtime},
                {"version", QString::fromUtf8(NKR_VERSION)},
                {"sid", install.sid},
            };
            if (!coreElevationWriteFile(install.grantDir + "\\grant.json", QJsonDocument(record).toJson(QJsonDocument::Compact))) {
                return HelperCopyFailed;
            }
            {
                const TaskRoot root;
                if (FAILED(root.status()) || FAILED(root.put(install.taskName, coreElevationTaskXml(install)))) return HelperTaskFailed;
            }

            if (!crashDir.isEmpty()) {
                Windows_RegisterWerDumps(qUtf16Printable(QFileInfo(install.gui).fileName()), qUtf16Printable(QDir::toNativeSeparators(crashDir)));
            }
            AutoRun_SetLeastPrivilege(install.gui);
            return HelperOk;
        }

        int coreElevationRevoke(const CoreElevationInstall &install) {
            bool ok = true;
            {
                const TaskRoot root;
                if (FAILED(root.status())) {
                    ok = false;
                } else if (root.find(install.taskName) && FAILED(root.remove(install.taskName))) {
                    ok = false;
                }
            }
            if (!coreElevationRemoveDir(install.grantDir)) ok = false;
            coreElevationRemoveDir(install.updateDir);
            // Succeeds only once no other install's or user's grant is left in it.
            if (const QString base = coreElevationBaseDir(); !base.isEmpty()) RemoveDirectoryW(qUtf16Printable(base));
            return ok ? HelperOk : HelperRemoveFailed;
        }

        int coreElevationUninstall() {
            const QString sid = coreElevationUserSid();
            if (sid.isEmpty()) return HelperOk;
            const CoreElevationInstall install = coreElevationLocalInstall(sid);
            if (!coreElevationTaskExists(install.taskName) && !coreElevationExists(install.grantDir)) return HelperOk;
            if (coreElevationIsElevated()) return coreElevationRevoke(install);
            const int code = coreElevationLaunch({QStringLiteral("revoke"), sid});
            return code == -1 ? HelperNotElevated : code;
        }

        // Every module this process mapped from the install dir: a copy elsewhere can wait out the updater without blocking it.
        bool coreElevationCopyModules(const QString &fromDir, const QString &toDir) {
            std::vector<HMODULE> modules(1024);
            DWORD needed = 0;
            if (!EnumProcessModules(GetCurrentProcess(), modules.data(), DWORD(modules.size() * sizeof(HMODULE)), &needed)) return false;
            modules.resize(std::min<size_t>(modules.size(), needed / sizeof(HMODULE)));
            std::vector<wchar_t> buffer(32768);
            for (const HMODULE module : modules) {
                const DWORD length = GetModuleFileNameW(module, buffer.data(), DWORD(buffer.size()));
                if (length == 0 || length >= buffer.size()) return false;
                const QString path = QString::fromWCharArray(buffer.data(), int(length));
                const QFileInfo info(path);
                if (QDir::cleanPath(info.absolutePath()).compare(QDir::cleanPath(fromDir), Qt::CaseInsensitive) != 0) continue;
                if (!CopyFileW(qUtf16Printable(path), qUtf16Printable(toDir + "\\" + info.fileName()), FALSE)) return false;
            }
            return true;
        }

        // Runs while the GUI still waits on this helper: everything that can fail early is done here, the rest in finish-update.
        int coreElevationStartUpdate(const CoreElevationInstall &install, const QString &zipPath, qint64 guiPid) {
            const QString zip = install.dir + "\\Throne.zip";
            if (QDir::cleanPath(zipPath).compare(QDir::cleanPath(zip), Qt::CaseInsensitive) != 0
                && !CopyFileW(qUtf16Printable(zipPath), qUtf16Printable(zip), FALSE)) return HelperUpdaterFailed;
            if (!CopyFileW(qUtf16Printable(install.dir + "\\updater.exe"), qUtf16Printable(install.dir + "\\updater.old"), FALSE)) {
                return HelperUpdaterFailed;
            }
            const bool granted = coreElevationTaskExists(install.taskName) || coreElevationExists(install.grantDir + "\\grant.json");
            // Run from the install dir, the waiting helper would keep Throne.exe and its DLLs mapped, and the updater could not replace them.
            if (!coreElevationRemoveDir(install.updateDir) || !coreElevationMakeDir(coreElevationBaseDir())
                || !coreElevationMakeDir(install.updateDir) || !coreElevationCopyModules(install.dir, install.updateDir)) {
                return HelperUpdaterFailed;
            }

            // Inherited by the copy, so the pid cannot be reused before it waits on it.
            const HANDLE gui = OpenProcess(SYNCHRONIZE, TRUE, DWORD(guiPid));
            const QString helper = install.updateDir + "\\" + QFileInfo(install.gui).fileName();
            std::wstring commandLine = QString("\"%1\" --elevated-helper finish-update %2 \"%3\" %4 %5")
                                           .arg(helper, install.sid, install.gui, QString::number(quintptr(gui)), QString::number(granted ? 1 : 0))
                                           .toStdWString();
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            const bool started = CreateProcessW(qUtf16Printable(helper), commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr,
                                                qUtf16Printable(install.updateDir), &startup, &process);
            if (gui != nullptr) CloseHandle(gui);
            if (!started) {
                coreElevationRemoveDir(install.updateDir);
                return HelperUpdaterFailed;
            }
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            return HelperOk;
        }

        int coreElevationFinishUpdate(const CoreElevationInstall &install, HANDLE gui, bool granted) {
            if (gui != nullptr) {
                WaitForSingleObject(gui, kCoreElevationGuiWaitMs);
                CloseHandle(gui);
            }
            int result = HelperOk;
            const QString updater = install.dir + "\\updater.old";
            std::wstring commandLine = QString("\"%1\"").arg(updater).toStdWString();
            STARTUPINFOW startup{};
            startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            // CREATE_NO_WINDOW as QProcess::startDetached from the GUI used, in case the updater is a console program.
            if (CreateProcessW(qUtf16Printable(updater), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                               qUtf16Printable(install.dir), &startup, &process)) {
                CloseHandle(process.hThread);
                WaitForSingleObject(process.hProcess, kCoreElevationUpdaterWaitMs);
                CloseHandle(process.hProcess);
            } else {
                result = HelperUpdaterFailed;
            }
            if (granted) {
                // The crash dir is left out: the original grant already wrote the LocalDumps keys.
                const int refreshed = coreElevationGrant(install, QString());
                if (result == HelperOk) result = refreshed;
            }
            // This copy cannot delete its own image and DLLs, so they are renamed aside and go at the next reboot.
            coreElevationRemoveDir(install.updateDir);
            return result;
        }
    } // namespace

    Status CurrentStatus() {
        const std::lock_guard lock(coreElevationMutex);
        if (!coreElevationStatus) coreElevationStatus = coreElevationEvaluate();
        return *coreElevationStatus;
    }

    void Invalidate() {
        const std::lock_guard lock(coreElevationMutex);
        coreElevationStatus.reset();
    }

    bool CanGrant() {
        TOKEN_ELEVATION_TYPE type = TokenElevationTypeDefault;
        return coreElevationQueryToken(TokenElevationType, &type, sizeof(type)) && type == TokenElevationTypeLimited && !Windows_IsInAdmin();
    }

    bool InstallDirWritable() {
        const QString probe = QDir::toNativeSeparators(QCoreApplication::applicationDirPath())
                              + QString("\\.throne-write-test-%1").arg(GetCurrentProcessId());
        const HANDLE file = CreateFileW(qUtf16Printable(probe), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW,
                                        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        CloseHandle(file);
        return true;
    }

    void ApplyEnvironment(QProcessEnvironment &env) {
        const QString name = QString::fromLatin1(kTaskEnv);
        env.remove(name);
        if (Windows_IsInAdmin() || CurrentStatus() != Status::Granted) return;
        env.insert(name, "\\" + coreElevationLocalInstall(coreElevationUserSid()).taskName);
    }

    void NoteCoreReport(bool throughGrant, bool privileged) {
        coreElevationGrantedCoreUnprivileged = throughGrant && !privileged;
    }

    bool GrantIneffective() {
        return coreElevationGrantedCoreUnprivileged && CurrentStatus() == Status::Granted;
    }

    int Grant() {
        const int code = coreElevationLaunch({QStringLiteral("grant"), coreElevationUserSid(), coreElevationArgPath(Logging::CrashDir())});
        if (code == HelperOk) coreElevationGrantedCoreUnprivileged = false;
        Invalidate();
        return code;
    }

    int Revoke() {
        const int code = coreElevationLaunch({QStringLiteral("revoke"), coreElevationUserSid()});
        Invalidate();
        return code;
    }

    void GrantAsync(QObject *context, std::function<void(int code)> done) {
        const QPointer<QObject> target(context);
        runOnNewThread([target, done = std::move(done)] {
            const int code = Grant();
            auto *app = QCoreApplication::instance();
            if (app == nullptr) return;
            // Checked on the main thread, where a context living there cannot be destroyed in between.
            QMetaObject::invokeMethod(app, [target, done, code] {
                if (target) QMetaObject::invokeMethod(target.data(), [done, code] { done(code); });
            }, Qt::QueuedConnection);
        });
    }

    int RunUpdater(const QString &zipPath, qint64 guiPid) {
        return coreElevationLaunch({QStringLiteral("run-updater"), coreElevationUserSid(), coreElevationArgPath(zipPath), QString::number(guiPid)});
    }

    QString ResultText(int code) {
        switch (code) {
            case -1:
                return QCoreApplication::translate("Sys::CoreElevation", "The Windows administrator prompt was declined or could not be shown.");
            case HelperOk:
                return QCoreApplication::translate("Sys::CoreElevation", "Done.");
            case HelperNotElevated:
                return QCoreApplication::translate("Sys::CoreElevation", "The helper did not receive administrator rights.");
            case HelperWrongUser:
                return QCoreApplication::translate("Sys::CoreElevation",
                                                   "Windows asked for a different administrator account; the grant must come from your own account.");
            case HelperBadArguments:
                return QCoreApplication::translate("Sys::CoreElevation", "The helper was started with invalid arguments.");
            case HelperCopyFailed:
                return QCoreApplication::translate("Sys::CoreElevation", "Could not copy ThroneCore into its protected folder.");
            case HelperTaskFailed:
                return QCoreApplication::translate("Sys::CoreElevation", "Could not register the scheduled task that starts ThroneCore as administrator.");
            case HelperRemoveFailed:
                return QCoreApplication::translate("Sys::CoreElevation", "Could not remove the scheduled task or the protected copy of ThroneCore.");
            case HelperUpdaterFailed:
                return QCoreApplication::translate("Sys::CoreElevation", "Could not prepare or start the updater.");
            default:
                return QCoreApplication::translate("Sys::CoreElevation", "The helper failed with exit code 0x%1.")
                    .arg(QString::number(quint32(code), 16).toUpper());
        }
    }

    bool IsHelperInvocation(int argc, char *argv[]) {
        return argc >= 2 && argv != nullptr && argv[1] != nullptr && std::strcmp(argv[1], kCoreElevationHelperFlag) == 0;
    }

    int RunHelper(int argc, char *argv[]) {
        // Hidden and unattended: a fault ends the process instead of waiting on an error dialog.
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        QCoreApplication app(argc, argv);
        const QStringList args = QCoreApplication::arguments().mid(2);
        const QString action = args.value(0);
        if (action == QLatin1String("uninstall")) return args.size() == 1 ? coreElevationUninstall() : HelperBadArguments;

        qsizetype minimum = 0;
        if (action == QLatin1String("grant") || action == QLatin1String("revoke")) minimum = 2;
        else if (action == QLatin1String("run-updater")) minimum = 4;
        else if (action == QLatin1String("finish-update")) minimum = 5;
        if (minimum == 0 || args.size() < minimum) return HelperBadArguments;
        if (!coreElevationIsElevated()) return HelperNotElevated;
        // An over-the-shoulder administrator would otherwise register the task for the wrong account.
        const QString sid = args.at(1);
        if (sid.isEmpty() || sid.compare(coreElevationUserSid(), Qt::CaseInsensitive) != 0) return HelperWrongUser;

        if (action == QLatin1String("grant")) return coreElevationGrant(coreElevationLocalInstall(sid), args.value(2));
        if (action == QLatin1String("revoke")) return coreElevationRevoke(coreElevationLocalInstall(sid));
        bool ok = false;
        if (action == QLatin1String("run-updater")) {
            const qint64 guiPid = args.at(3).toLongLong(&ok);
            if (!ok || guiPid <= 0 || args.at(2).isEmpty()) return HelperBadArguments;
            return coreElevationStartUpdate(coreElevationLocalInstall(sid), QDir::toNativeSeparators(args.at(2)), guiPid);
        }
        // finish-update <sid> <install Throne.exe> <inherited GUI handle> <1 when a grant existed>: this process is a copy outside it.
        const QString installExe = args.at(2);
        const quint64 gui = args.at(3).toULongLong(&ok);
        if (!ok || installExe.isEmpty()) return HelperBadArguments;
        const auto install = coreElevationInstallFor(installExe, QFileInfo(installExe).absolutePath() + "/ThroneCore.exe", sid);
        return coreElevationFinishUpdate(install, reinterpret_cast<HANDLE>(quintptr(gui)), args.at(4) == QLatin1String("1"));
    }

    void *OpenWatch(qint64 pid) {
        if (pid <= 0) return nullptr;
        return OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
    }

    bool WaitWatch(void *watch, int waitMs) {
        if (watch == nullptr) return true;
        return WaitForSingleObject(static_cast<HANDLE>(watch), DWORD(std::max(waitMs, 0))) != WAIT_TIMEOUT;
    }

    void CloseWatch(void *watch) {
        if (watch != nullptr) CloseHandle(static_cast<HANDLE>(watch));
    }
} // namespace Sys::CoreElevation
