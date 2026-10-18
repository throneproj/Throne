#include "include/sys/ServiceManager.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QStringList>

#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "include/global/Logger.hpp"

#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <shlobj.h>
// <windows.h> defines SetPort, which under unity builds clobbers Configs::outbound::SetPort.
#undef SetPort

namespace Sys::ServiceManager {
    namespace {
        constexpr DWORD kServiceManagerWinAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN | SERVICE_ACCEPT_SHUTDOWN;
        constexpr DWORD kServiceManagerWinStartHintMs = 30000;
        constexpr DWORD kServiceManagerWinStopHintMs = 10000;
        constexpr DWORD kServiceManagerWinHeartbeatMs = 2000;
        constexpr ULONGLONG kServiceManagerWinHeartbeatCapMs = 45000;
        constexpr DWORD kServiceManagerWinPreshutdownMs = 45000;
        constexpr DWORD kServiceManagerWinConsoleWaitMs = 4500;
        constexpr qint64 kServiceManagerWinMaxHintMs = 10 * 60 * 1000;
        constexpr size_t kServiceManagerWinMaxHeld = 4;
        constexpr wchar_t kServiceManagerWinWindowClass[] = L"ThroneHeadlessWindow";
        constexpr wchar_t kServiceManagerWinOwnedSddl[] = L"O:SYG:SYD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";
        constexpr wchar_t kServiceManagerWinDaclSddl[] = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";
        // FOLDERID_ProgramData, spelled out so no uuid import library is needed.
        constexpr GUID kServiceManagerWinProgramData = {0x62ab5d82, 0xfdc1, 0x4dc3, {0xa9, 0xdd, 0x07, 0x0d, 0x1d, 0x49, 0x5d, 0x97}};

        struct ServiceManagerWinState {
            std::mutex mutex;
            // SCM status, under mutex. Nothing is reported once stopped is set (T13).
            SERVICE_STATUS_HANDLE handle = nullptr;
            SERVICE_STATUS status{};
            bool stopped = false;
            bool heartbeat = false;
            bool scmStopSeen = false;
            // Stop delivery, under mutex: armed implies a live app object, since aboutToQuit disarms under the same mutex.
            bool armed = false;
            bool closed = false;
            std::vector<std::pair<StopKind, QString>> held;
            // Written by ServiceMain before it sets started.
            std::wstring name;
            QString preshutdown;
            HANDLE started = nullptr;
            std::atomic<bool> serviceMainRan{false};
            std::atomic<bool> isService{false};
            std::atomic<int> consoleInterrupts{0};
            // Main thread only.
            std::function<void(StopKind)> handler;
            std::function<void()> sessionEnd;
            QStringList notes;
            bool signalsInstalled = false;
        };

        ServiceManagerWinState &serviceManagerWinState() {
            // Never destroyed: the dispatcher, heartbeat and console threads may still run while statics are torn down.
            static auto *state = new ServiceManagerWinState;
            return *state;
        }

        HANDLE serviceManagerWinShutdownDone() {
            static const HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            return event;
        }

        QString serviceManagerWinError(DWORD code) {
            wchar_t *text = nullptr;
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                           reinterpret_cast<LPWSTR>(&text), 0, nullptr);
            const QString message = text != nullptr ? QString::fromWCharArray(text).trimmed() : QString();
            if (text != nullptr) LocalFree(text);
            if (message.isEmpty()) return QStringLiteral("error %1").arg(code);
            return QStringLiteral("%1 (error %2)").arg(message, QString::number(code));
        }

        QString serviceManagerWinSidText(PSID sid) {
            LPWSTR text = nullptr;
            if (sid == nullptr || !ConvertSidToStringSidW(sid, &text)) return QStringLiteral("an unknown account");
            const QString result = QString::fromWCharArray(text);
            LocalFree(text);
            return result;
        }

        PSID serviceManagerWinSid(const std::vector<BYTE> &sid) {
            return sid.empty() ? nullptr : const_cast<BYTE *>(sid.data());
        }

        // A copy of the token's user SID; empty on failure.
        std::vector<BYTE> serviceManagerWinTokenSid(HANDLE token) {
            DWORD size = 0;
            GetTokenInformation(token, TokenUser, nullptr, 0, &size);
            if (size == 0) return {};
            std::vector<BYTE> buffer(size);
            if (!GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) return {};
            const PSID sid = reinterpret_cast<TOKEN_USER *>(buffer.data())->User.Sid;
            if (!IsValidSid(sid)) return {};
            std::vector<BYTE> result(GetLengthSid(sid));
            if (!CopySid(static_cast<DWORD>(result.size()), result.data(), sid)) return {};
            return result;
        }

        const std::vector<BYTE> &serviceManagerWinSelfSid() {
            static const std::vector<BYTE> sid = [] {
                std::vector<BYTE> result;
                HANDLE token = nullptr;
                if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
                    result = serviceManagerWinTokenSid(token);
                    CloseHandle(token);
                }
                return result;
            }();
            return sid;
        }

        bool serviceManagerWinTrustedOwner(PSID owner) {
            if (owner == nullptr || !IsValidSid(owner)) return false;
            if (IsWellKnownSid(owner, WinLocalSystemSid) || IsWellKnownSid(owner, WinBuiltinAdministratorsSid)) return true;
            const PSID self = serviceManagerWinSid(serviceManagerWinSelfSid());
            return self != nullptr && EqualSid(owner, self);
        }

        bool serviceManagerWinSelfElevated() {
            HANDLE token = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
            TOKEN_ELEVATION elevation{};
            DWORD size = 0;
            const bool elevated =
                GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) && elevation.TokenIsElevated != 0;
            CloseHandle(token);
            return elevated;
        }

        // No account but SYSTEM, Administrators and self holds any of rights (pipe: add instances or rewrite the descriptor; dir: write).
        bool serviceManagerWinClosedToOthers(PACL dacl, PSID self, ACCESS_MASK rights) {
            if (dacl == nullptr) return false;
            for (DWORD index = 0; index < dacl->AceCount; ++index) {
                LPVOID raw = nullptr;
                if (!GetAce(dacl, index, &raw) || raw == nullptr) return false;
                const auto *header = static_cast<const ACE_HEADER *>(raw);
                if ((header->AceFlags & INHERIT_ONLY_ACE) != 0) continue;
                switch (header->AceType) {
                    case ACCESS_DENIED_ACE_TYPE:
                    case ACCESS_DENIED_CALLBACK_ACE_TYPE:
                    case ACCESS_DENIED_OBJECT_ACE_TYPE:
                    case ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE:
                        continue;
                    default:
                        break;
                }
                // Every ACE type keeps its mask right after the header; only the two plain allow types keep the SID right after it.
                const auto *ace = static_cast<const ACCESS_ALLOWED_ACE *>(raw);
                if ((ace->Mask & rights) == 0) continue;
                if (header->AceType != ACCESS_ALLOWED_ACE_TYPE && header->AceType != ACCESS_ALLOWED_CALLBACK_ACE_TYPE) return false;
                const auto sid = reinterpret_cast<PSID>(const_cast<DWORD *>(&ace->SidStart));
                if (IsWellKnownSid(sid, WinLocalSystemSid) || IsWellKnownSid(sid, WinBuiltinAdministratorsSid)) continue;
                if (self != nullptr && EqualSid(sid, self)) continue;
                return false;
            }
            return true;
        }

        // Caller holds the mutex.
        void serviceManagerWinReportLocked(ServiceManagerWinState &state) {
            if (state.handle != nullptr && !state.stopped) SetServiceStatus(state.handle, &state.status);
        }

        // STOP_PENDING checkpoints must advance while main blocks in waitForFinished (T15), so a thread of its own bumps them.
        DWORD WINAPI serviceManagerWinHeartbeat(LPVOID) {
            auto &state = serviceManagerWinState();
            const ULONGLONG begin = GetTickCount64();
            while (true) {
                Sleep(kServiceManagerWinHeartbeatMs);
                if (GetTickCount64() - begin > kServiceManagerWinHeartbeatCapMs) break;
                std::lock_guard lock(state.mutex);
                if (state.stopped || state.status.dwCurrentState != SERVICE_STOP_PENDING) break;
                ++state.status.dwCheckPoint;
                serviceManagerWinReportLocked(state);
            }
            return 0;
        }

        // Caller holds the mutex; idempotent.
        void serviceManagerWinStopPendingLocked(ServiceManagerWinState &state) {
            if (state.handle == nullptr || state.stopped || state.status.dwCurrentState == SERVICE_STOP_PENDING) return;
            state.status.dwCurrentState = SERVICE_STOP_PENDING;
            state.status.dwCheckPoint = 1;
            state.status.dwWaitHint = kServiceManagerWinStopHintMs;
            serviceManagerWinReportLocked(state);
            if (std::exchange(state.heartbeat, true)) return;
            if (const HANDLE thread = CreateThread(nullptr, 0, serviceManagerWinHeartbeat, nullptr, 0, nullptr)) CloseHandle(thread);
        }

        // Caller holds the mutex; START_PENDING only, so a stop or READY is never undone.
        void serviceManagerWinStartProgressLocked(ServiceManagerWinState &state, DWORD waitHintMs) {
            if (state.handle == nullptr || state.stopped || state.status.dwCurrentState != SERVICE_START_PENDING) return;
            ++state.status.dwCheckPoint;
            state.status.dwWaitHint = waitHintMs;
            serviceManagerWinReportLocked(state);
        }

        // Main thread.
        void serviceManagerWinDeliver(StopKind kind, const QString &source) {
            if (kind == StopKind::Fast) {
                LOG_INFO(QStringLiteral("Stop request (fast): %1.").arg(source));
            } else {
                LOG_INFO(QStringLiteral("Stop request: %1.").arg(source));
            }
            const auto handler = serviceManagerWinState().handler;
            if (handler) handler(kind);
        }

        // Caller holds the mutex and has checked armed.
        void serviceManagerWinQueueLocked(StopKind kind, const QString &source) {
            QMetaObject::invokeMethod(
                QCoreApplication::instance(), [kind, source] { serviceManagerWinDeliver(kind, source); }, Qt::QueuedConnection);
        }

        // Any thread. Before ArmStopHandler() the stop is held: QCoreApplication::exit() before exec() is a no-op (T14).
        void serviceManagerWinPost(StopKind kind, const QString &source) {
            auto &state = serviceManagerWinState();
            std::lock_guard lock(state.mutex);
            if (state.closed) return;
            if (!state.armed) {
                if (state.held.size() < kServiceManagerWinMaxHeld) state.held.emplace_back(kind, source);
                return;
            }
            serviceManagerWinQueueLocked(kind, source);
        }

        // Dispatcher thread.
        DWORD WINAPI serviceManagerWinControl(DWORD control, DWORD, LPVOID, LPVOID) {
            QString source;
            switch (control) {
                case SERVICE_CONTROL_INTERROGATE:
                    return NO_ERROR;
                case SERVICE_CONTROL_STOP:
                    source = QStringLiteral("the Service Control Manager (stop)");
                    break;
                case SERVICE_CONTROL_PRESHUTDOWN:
                case SERVICE_CONTROL_SHUTDOWN:
                    source = QStringLiteral("the Service Control Manager (system shutdown)");
                    break;
                default:
                    return ERROR_CALL_NOT_IMPLEMENTED;
            }
            auto &state = serviceManagerWinState();
            {
                std::lock_guard lock(state.mutex);
                serviceManagerWinStopPendingLocked(state);
                if (std::exchange(state.scmStopSeen, true)) return NO_ERROR;
            }
            serviceManagerWinPost(StopKind::Graceful, source);
            return NO_ERROR;
        }

        // Raises the preshutdown timeout to 45 s (T16) unless the service already has at least that; best effort.
        QString serviceManagerWinConfigurePreshutdown(const std::wstring &name) {
            const SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
            if (manager == nullptr) return QStringLiteral("unchanged: %1").arg(serviceManagerWinError(GetLastError()));
            QString note;
            const SC_HANDLE service = OpenServiceW(manager, name.c_str(), SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG);
            if (service == nullptr) {
                note = QStringLiteral("unchanged: %1").arg(serviceManagerWinError(GetLastError()));
            } else {
                SERVICE_PRESHUTDOWN_INFO info{};
                DWORD needed = 0;
                if (QueryServiceConfig2W(service, SERVICE_CONFIG_PRESHUTDOWN_INFO, reinterpret_cast<LPBYTE>(&info), sizeof(info), &needed) &&
                    info.dwPreshutdownTimeout >= kServiceManagerWinPreshutdownMs) {
                    note = QStringLiteral("%1 s").arg(info.dwPreshutdownTimeout / 1000);
                } else {
                    info.dwPreshutdownTimeout = kServiceManagerWinPreshutdownMs;
                    note = ChangeServiceConfig2W(service, SERVICE_CONFIG_PRESHUTDOWN_INFO, &info)
                               ? QStringLiteral("raised to %1 s").arg(kServiceManagerWinPreshutdownMs / 1000)
                               : QStringLiteral("unchanged: %1").arg(serviceManagerWinError(GetLastError()));
                }
                CloseServiceHandle(service);
            }
            CloseServiceHandle(manager);
            return note;
        }

        void WINAPI serviceManagerWinMain(DWORD argc, LPWSTR *argv) {
            auto &state = serviceManagerWinState();
            if (argc > 0 && argv != nullptr && argv[0] != nullptr) state.name = argv[0];
            {
                // Held across the registration, so a control arriving at once sees the handle and the START_PENDING status.
                std::lock_guard lock(state.mutex);
                state.handle = RegisterServiceCtrlHandlerExW(state.name.c_str(), serviceManagerWinControl, nullptr);
                if (state.handle != nullptr) {
                    state.status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
                    state.status.dwCurrentState = SERVICE_START_PENDING;
                    state.status.dwControlsAccepted = kServiceManagerWinAccepted;
                    state.status.dwWin32ExitCode = NO_ERROR;
                    state.status.dwServiceSpecificExitCode = 0;
                    state.status.dwCheckPoint = 1;
                    state.status.dwWaitHint = kServiceManagerWinStartHintMs;
                    serviceManagerWinReportLocked(state);
                }
            }
            if (state.handle != nullptr) {
                state.preshutdown = serviceManagerWinConfigurePreshutdown(state.name);
                state.serviceMainRan = true;
            }
            SetEvent(state.started);
        }

        DWORD WINAPI serviceManagerWinDispatcher(LPVOID) {
            static wchar_t serviceName[] = L"";
            const SERVICE_TABLE_ENTRYW table[] = {{serviceName, serviceManagerWinMain}, {nullptr, nullptr}};
            // Blocks until SERVICE_STOPPED went out; fails at once (ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) outside the SCM.
            StartServiceCtrlDispatcherW(table);
            SetEvent(serviceManagerWinState().started);
            return 0;
        }

        BOOL WINAPI serviceManagerWinConsole(DWORD type) {
            switch (type) {
                case CTRL_C_EVENT:
                case CTRL_BREAK_EVENT: {
                    const QString key = type == CTRL_C_EVENT ? QStringLiteral("Ctrl+C") : QStringLiteral("Ctrl+Break");
                    if (serviceManagerWinState().consoleInterrupts.fetch_add(1) == 0) {
                        serviceManagerWinPost(StopKind::Graceful, key);
                    } else {
                        serviceManagerWinPost(StopKind::Fast, key + QStringLiteral(" again"));
                    }
                    return TRUE;
                }
                case CTRL_CLOSE_EVENT:
                case CTRL_LOGOFF_EVENT:
                case CTRL_SHUTDOWN_EVENT: {
                    QString source = QStringLiteral("the console window was closed");
                    if (type == CTRL_LOGOFF_EVENT) source = QStringLiteral("the user is logging off");
                    if (type == CTRL_SHUTDOWN_EVENT) source = QStringLiteral("the system is shutting down");
                    serviceManagerWinPost(StopKind::Graceful, source);
                    // The process is ended as soon as this returns (T21); the system's own limit is about 5 s.
                    WaitForSingleObject(serviceManagerWinShutdownDone(), kServiceManagerWinConsoleWaitMs);
                    return TRUE;
                }
                default:
                    return FALSE;
            }
        }

        // Main thread (QEventDispatcherWin32 dispatches every message of the thread, nested loops included).
        LRESULT CALLBACK serviceManagerWinWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
            switch (message) {
                case WM_CLOSE:
                    serviceManagerWinPost(StopKind::Graceful, QStringLiteral("WM_CLOSE (taskkill without /F)"));
                    return 0;
                case WM_QUERYENDSESSION:
                    return TRUE;
                case WM_ENDSESSION:
                    if (wParam != FALSE) {
                        LOG_INFO(QStringLiteral("Stop request: the Windows session is ending."));
                        // Windows may end the process as soon as this returns (T19), so the teardown runs right here.
                        const auto sessionEnd = serviceManagerWinState().sessionEnd;
                        if (sessionEnd) sessionEnd();
                    }
                    return 0;
                default:
                    return DefWindowProcW(window, message, wParam, lParam);
            }
        }

        // A new directory with the protected DACL, owned by SYSTEM when this process may name it; a Win32 error otherwise.
        DWORD serviceManagerWinCreateDir(const std::wstring &target) {
            const wchar_t *const descriptors[] = {kServiceManagerWinOwnedSddl, kServiceManagerWinDaclSddl};
            for (const wchar_t *sddl : descriptors) {
                PSECURITY_DESCRIPTOR descriptor = nullptr;
                if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &descriptor, nullptr)) return GetLastError();
                SECURITY_ATTRIBUTES attributes{};
                attributes.nLength = sizeof(attributes);
                attributes.lpSecurityDescriptor = descriptor;
                attributes.bInheritHandle = FALSE;
                const DWORD code = CreateDirectoryW(target.c_str(), &attributes) ? ERROR_SUCCESS : GetLastError();
                LocalFree(descriptor);
                // Only SYSTEM itself may name SYSTEM as the owner; another service account keeps its default owner.
                if (code != ERROR_INVALID_OWNER) return code;
            }
            return ERROR_INVALID_OWNER;
        }

        // FILE_FLAG_OPEN_REPARSE_POINT opens a link or junction itself, never what it points at.
        HANDLE serviceManagerWinOpenNoFollow(const std::wstring &path, DWORD access) {
            return CreateFileW(path.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                               FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        }

        DWORD serviceManagerWinAttributes(HANDLE handle, DWORD *attributes) {
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(handle, &info)) return GetLastError();
            *attributes = info.dwFileAttributes;
            return ERROR_SUCCESS;
        }

        DWORD serviceManagerWinHandleOwner(HANDLE handle, bool *trusted, QString *ownerText) {
            PSID owner = nullptr;
            PSECURITY_DESCRIPTOR security = nullptr;
            if (const DWORD code =
                    GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, nullptr, nullptr, nullptr, &security);
                code != ERROR_SUCCESS) {
                return code;
            }
            *trusted = serviceManagerWinTrustedOwner(owner);
            *ownerText = serviceManagerWinSidText(owner);
            LocalFree(security);
            return ERROR_SUCCESS;
        }

        // An entry under the data dir: a refusal, or empty (also when it vanished meanwhile) with *directory set.
        QString serviceManagerWinEntryRefusal(const std::wstring &path, const QString &shown, bool *directory) {
            *directory = false;
            const HANDLE handle = serviceManagerWinOpenNoFollow(path, READ_CONTROL | FILE_READ_ATTRIBUTES);
            if (handle == INVALID_HANDLE_VALUE) {
                const DWORD code = GetLastError();
                if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) return {};
                return QStringLiteral("the owner of %1 could not be read: %2; remove it").arg(shown, serviceManagerWinError(code));
            }
            QString refusal;
            DWORD attributes = 0;
            bool trusted = false;
            QString ownerText;
            if (const DWORD code = serviceManagerWinAttributes(handle, &attributes); code != ERROR_SUCCESS) {
                refusal = QStringLiteral("%1 could not be inspected: %2").arg(shown, serviceManagerWinError(code));
            } else if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                refusal = QStringLiteral("%1 is a link or junction; remove it").arg(shown);
            } else if (const DWORD ownerCode = serviceManagerWinHandleOwner(handle, &trusted, &ownerText); ownerCode != ERROR_SUCCESS) {
                refusal = QStringLiteral("the owner of %1 could not be read: %2; remove it").arg(shown, serviceManagerWinError(ownerCode));
            } else if (!trusted) {
                refusal = QStringLiteral("%1 is owned by %2, not SYSTEM or Administrators; remove it").arg(shown, ownerText);
            } else {
                *directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            }
            CloseHandle(handle);
            return refusal;
        }

        // Re-owning is no fix: a handle opened while Users could write here keeps its access, so such entries are refused.
        QString serviceManagerWinUntrustedEntry(const std::wstring &root) {
            // \\?\ takes names literally: an entry named with a trailing dot or space is opened, not skipped as vanished.
            const bool drive = root.size() > 3 && root[1] == L':' && root[2] == L'\\';
            const std::wstring literal = drive ? L"\\\\?\\" + root : root;
            std::vector<std::wstring> pending{std::wstring()};
            while (!pending.empty()) {
                const std::wstring relative = std::move(pending.back());
                pending.pop_back();
                const std::wstring base = relative.empty() ? literal : literal + L'\\' + relative;
                const auto listError = [&relative](DWORD code) {
                    if (relative.empty()) return QStringLiteral("its entries could not be listed: %1").arg(serviceManagerWinError(code));
                    return QStringLiteral("the entries of %1 could not be listed: %2")
                        .arg(QString::fromStdWString(relative), serviceManagerWinError(code));
                };
                WIN32_FIND_DATAW data{};
                const std::wstring pattern = base + L"\\*";
                const HANDLE find =
                    FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
                if (find == INVALID_HANDLE_VALUE) {
                    const DWORD code = GetLastError();
                    if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) continue;
                    return listError(code);
                }
                QString refusal;
                do {
                    const std::wstring name = data.cFileName;
                    if (name == L"." || name == L"..") continue;
                    const std::wstring entry = relative.empty() ? name : relative + L'\\' + name;
                    bool directory = false;
                    refusal = serviceManagerWinEntryRefusal(literal + L'\\' + entry, QString::fromStdWString(entry), &directory);
                    if (!refusal.isEmpty()) break;
                    if (directory) pending.push_back(entry);
                } while (FindNextFileW(find, &data));
                const DWORD code = refusal.isEmpty() ? GetLastError() : ERROR_NO_MORE_FILES;
                FindClose(find);
                if (!refusal.isEmpty()) return refusal;
                if (code != ERROR_NO_MORE_FILES) return listError(code);
            }
            return {};
        }

        // An existing data dir, checked and locked down through one handle so a swap for a junction cannot redirect the DACL.
        QString serviceManagerWinSecureExisting(HANDLE directory, std::wstring &target) {
            DWORD attributes = 0;
            if (const DWORD code = serviceManagerWinAttributes(directory, &attributes); code != ERROR_SUCCESS) {
                return QStringLiteral("cannot be read: %1").arg(serviceManagerWinError(code));
            }
            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) return QStringLiteral("is a link or junction; remove it");
            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) return QStringLiteral("is not a directory; remove it");

            bool trusted = false;
            QString ownerText;
            if (const DWORD code = serviceManagerWinHandleOwner(directory, &trusted, &ownerText); code != ERROR_SUCCESS) {
                return QStringLiteral("its owner could not be read: %1").arg(serviceManagerWinError(code));
            }
            if (!trusted) return QStringLiteral("owned by %1, not SYSTEM or Administrators; remove it").arg(ownerText);

            // A handle opened while others could write here keeps that access after the DACL changes and can still turn the empty
            // dir into a junction, so such a dir is refused before anything is changed (Throne's own dirs never grant it).
            PACL current = nullptr;
            PSECURITY_DESCRIPTOR currentSecurity = nullptr;
            if (const DWORD read = GetSecurityInfo(directory, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &current,
                                                   nullptr, &currentSecurity);
                read != ERROR_SUCCESS) {
                return QStringLiteral("its access list could not be read: %1").arg(serviceManagerWinError(read));
            }
            constexpr ACCESS_MASK writeRights = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | FILE_DELETE_CHILD |
                                                DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_WRITE | GENERIC_ALL;
            const bool closed = serviceManagerWinClosedToOthers(current, serviceManagerWinSid(serviceManagerWinSelfSid()), writeRights);
            LocalFree(currentSecurity);
            if (!closed) return QStringLiteral("other users can write to it; remove it");

            PSECURITY_DESCRIPTOR wanted = nullptr;
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(kServiceManagerWinDaclSddl, SDDL_REVISION_1, &wanted, nullptr)) {
                return QStringLiteral("its access list could not be built: %1").arg(serviceManagerWinError(GetLastError()));
            }
            BOOL present = FALSE;
            BOOL defaulted = FALSE;
            PACL dacl = nullptr;
            DWORD code = ERROR_INVALID_SECURITY_DESCR;
            constexpr SECURITY_INFORMATION what = DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION;
            if (GetSecurityDescriptorDacl(wanted, &present, &dacl, &defaulted) && present && dacl != nullptr) {
                code = SetSecurityInfo(directory, SE_FILE_OBJECT, what, nullptr, nullptr, dacl, nullptr);
                // By name as well: only SetNamedSecurityInfoW pushes the inheritable ACEs down to throne.db, api.key and the rest (T12).
                if (code == ERROR_SUCCESS) {
                    code = SetNamedSecurityInfoW(target.data(), SE_FILE_OBJECT, what, nullptr, nullptr, dacl, nullptr);
                }
            }
            LocalFree(wanted);
            // A handle opened while Users could write here can still turn the directory into a junction.
            if (const DWORD recheck = serviceManagerWinAttributes(directory, &attributes); recheck != ERROR_SUCCESS) {
                return QStringLiteral("cannot be read: %1").arg(serviceManagerWinError(recheck));
            }
            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) return QStringLiteral("is a link or junction; remove it");
            // Walked even when propagation failed: another account's entry it could not open is the likely cause, and gets named.
            if (const QString entry = serviceManagerWinUntrustedEntry(target); !entry.isEmpty()) return entry;
            if (code != ERROR_SUCCESS) {
                return QStringLiteral("access could not be limited to SYSTEM and Administrators: %1").arg(serviceManagerWinError(code));
            }
            return {};
        }
    } // namespace

    bool StartServiceDispatcher(int waitMs) {
        auto &state = serviceManagerWinState();
        static const bool started = [&state, waitMs] {
            state.started = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (state.started == nullptr) return false;
            const HANDLE thread = CreateThread(nullptr, 0, serviceManagerWinDispatcher, nullptr, 0, nullptr);
            if (thread == nullptr) return false;
            CloseHandle(thread);
            WaitForSingleObject(state.started, static_cast<DWORD>(qMax(0, waitMs)));
            return state.serviceMainRan.load();
        }();
        state.isService = started;
        return started;
    }

    bool SelfElevated() {
        return serviceManagerWinSelfElevated();
    }

    bool IsService() {
        return serviceManagerWinState().isService.load();
    }

    void SetStopHandler(std::function<void(StopKind)> handler) {
        serviceManagerWinState().handler = std::move(handler);
    }

    void ArmStopHandler() {
        auto &state = serviceManagerWinState();
        auto *app = QCoreApplication::instance();
        if (app == nullptr) return;
        for (const auto &note : std::exchange(state.notes, {})) LOG_WARN(note);
        if (state.isService) {
            LOG_INFO(QStringLiteral("[Service] Running under the Service Control Manager as \"%1\"; preshutdown timeout %2.")
                         .arg(QString::fromStdWString(state.name), state.preshutdown));
        }
        {
            std::lock_guard lock(state.mutex);
            if (state.armed || state.closed) return;
            state.armed = true;
            for (const auto &[kind, source] : std::exchange(state.held, {})) serviceManagerWinQueueLocked(kind, source);
        }
        QObject::connect(
            app, &QCoreApplication::aboutToQuit, app,
            [] {
                auto &state = serviceManagerWinState();
                std::lock_guard lock(state.mutex);
                state.armed = false;
                state.closed = true;
                state.held.clear();
            },
            Qt::DirectConnection);
    }

    void InstallShutdownSignals(std::function<void()> sessionEnd) {
        auto &state = serviceManagerWinState();
        if (state.isService || std::exchange(state.signalsInstalled, true)) return;
        state.sessionEnd = std::move(sessionEnd);
        // Created here, not first inside a console handler.
        if (serviceManagerWinShutdownDone() == nullptr) {
            state.notes << QStringLiteral("Could not create the shutdown event: %1").arg(serviceManagerWinError(GetLastError()));
        }
        // GetConsoleCP() is 0 without an attached console.
        if (GetConsoleCP() != 0 && !SetConsoleCtrlHandler(serviceManagerWinConsole, TRUE)) {
            state.notes << QStringLiteral("Could not install the console control handler: %1").arg(serviceManagerWinError(GetLastError()));
        }
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = serviceManagerWinWindowProc;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = kServiceManagerWinWindowClass;
        if (RegisterClassExW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            state.notes << QStringLiteral("Could not register the shutdown window class: %1").arg(serviceManagerWinError(GetLastError()));
            return;
        }
        // Top level and never shown: an HWND_MESSAGE window gets no WM_QUERYENDSESSION, WM_ENDSESSION or taskkill WM_CLOSE (T20).
        if (CreateWindowExW(0, kServiceManagerWinWindowClass, L"Throne headless", WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, instance,
                            nullptr) == nullptr) {
            state.notes << QStringLiteral("Could not create the shutdown window (taskkill and session end will not stop Throne cleanly): %1")
                               .arg(serviceManagerWinError(GetLastError()));
        }
    }

    void SignalShutdownComplete() {
        if (const HANDLE event = serviceManagerWinShutdownDone()) SetEvent(event);
    }

    bool NotifyEnabled() {
        return IsService();
    }

    void NotifyReady(const QString &status) {
        Q_UNUSED(status);
        auto &state = serviceManagerWinState();
        std::lock_guard lock(state.mutex);
        if (state.handle == nullptr || state.stopped || state.status.dwCurrentState != SERVICE_START_PENDING) return;
        state.status.dwCurrentState = SERVICE_RUNNING;
        state.status.dwCheckPoint = 0;
        state.status.dwWaitHint = 0;
        serviceManagerWinReportLocked(state);
    }

    void NotifyStatus(const QString &status) {
        Q_UNUSED(status);
    }

    void NotifyStopping(const QString &status) {
        Q_UNUSED(status);
        auto &state = serviceManagerWinState();
        std::lock_guard lock(state.mutex);
        serviceManagerWinStopPendingLocked(state);
    }

    // A service never restarts in place: its 75 goes to the SCM's recovery, so this is a plain stop.
    void NotifyReloading(const QString &status) {
        NotifyStopping(status);
    }

    void NotifyWatchdog() {}

    void NotifyExtendTimeout(qint64 usec) {
        if (usec <= 0) return;
        const auto hintMs = static_cast<DWORD>(qBound<qint64>(1000, usec / 1000, kServiceManagerWinMaxHintMs));
        auto &state = serviceManagerWinState();
        std::lock_guard lock(state.mutex);
        serviceManagerWinStartProgressLocked(state, hintMs);
    }

    qint64 WatchdogUsec() {
        return 0;
    }

    void NotifyStartProgress() {
        auto &state = serviceManagerWinState();
        std::lock_guard lock(state.mutex);
        serviceManagerWinStartProgressLocked(state, kServiceManagerWinStartHintMs);
    }

    void NotifyStopped(int exitCode) {
        auto &state = serviceManagerWinState();
        SERVICE_STATUS_HANDLE handle = nullptr;
        SERVICE_STATUS report{};
        {
            // Every other report holds the mutex and checks stopped, so none can follow this one.
            std::lock_guard lock(state.mutex);
            if (state.handle == nullptr || state.stopped) return;
            state.stopped = true;
            state.status.dwCurrentState = SERVICE_STOPPED;
            state.status.dwControlsAccepted = 0;
            // A stop the SCM asked for reports success whatever the code, or recovery would restart the service it stopped.
            const bool requested = state.scmStopSeen;
            state.status.dwWin32ExitCode = exitCode != 0 && !requested ? ERROR_SERVICE_SPECIFIC_ERROR : NO_ERROR;
            state.status.dwServiceSpecificExitCode = requested ? 0 : static_cast<DWORD>(exitCode);
            state.status.dwCheckPoint = 0;
            state.status.dwWaitHint = 0;
            handle = state.handle;
            report = state.status;
        }
        SetServiceStatus(handle, &report);
    }

    bool RunningAsLocalSystem() {
        const PSID self = serviceManagerWinSid(serviceManagerWinSelfSid());
        return self != nullptr && IsWellKnownSid(self, WinLocalSystemSid);
    }

    QString SystemDataDir(bool service) {
        if (!service && !RunningAsLocalSystem()) return {};
        PWSTR raw = nullptr;
        QString base;
        if (SUCCEEDED(SHGetKnownFolderPath(kServiceManagerWinProgramData, 0, nullptr, &raw)) && raw != nullptr) {
            base = QString::fromWCharArray(raw);
        }
        CoTaskMemFree(raw);
        if (base.isEmpty()) return {};
        return QDir::fromNativeSeparators(base) + QStringLiteral("/Throne");
    }

    bool SecureDataDir(const QString &path, bool created, QString *error) {
        // The dir as it is now decides, not what the caller saw a moment earlier.
        Q_UNUSED(created);
        const auto fail = [error](const QString &text) {
            if (error != nullptr) *error = text;
            return false;
        };
        const QString clean = QDir::cleanPath(path);
        std::wstring target = QDir::toNativeSeparators(clean).toStdWString();
        constexpr DWORD access = READ_CONTROL | WRITE_DAC | FILE_READ_ATTRIBUTES;

        HANDLE directory = serviceManagerWinOpenNoFollow(target, access);
        if (directory == INVALID_HANDLE_VALUE) {
            DWORD code = GetLastError();
            if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) {
                QDir().mkpath(QFileInfo(clean).absolutePath());
                code = serviceManagerWinCreateDir(target);
                if (code == ERROR_SUCCESS) return true;
                if (code != ERROR_ALREADY_EXISTS) return fail(QStringLiteral("could not be created: %1").arg(serviceManagerWinError(code)));
                directory = serviceManagerWinOpenNoFollow(target, access);
                if (directory == INVALID_HANDLE_VALUE) code = GetLastError();
            }
            if (directory == INVALID_HANDLE_VALUE) {
                if (code == ERROR_ACCESS_DENIED) {
                    return fail(QStringLiteral("denies this account access, so its owner cannot be checked; remove it"));
                }
                return fail(QStringLiteral("could not be opened: %1").arg(serviceManagerWinError(code)));
            }
        }
        const QString refusal = serviceManagerWinSecureExisting(directory, target);
        CloseHandle(directory);
        if (!refusal.isEmpty()) return fail(refusal);
        return true;
    }

    QString UsersCanWriteDataDir(const QString &path) {
        const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        PACL dacl = nullptr;
        if (GetNamedSecurityInfoW(native.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl, nullptr,
                                  &descriptor) != ERROR_SUCCESS) {
            return {};
        }
        // A null DACL grants everyone everything.
        QString who = dacl == nullptr ? QStringLiteral("Everyone") : QString();
        const std::pair<WELL_KNOWN_SID_TYPE, const char *> groups[] = {
            {WinBuiltinUsersSid, "Users"}, {WinAuthenticatedUserSid, "Authenticated Users"}, {WinWorldSid, "Everyone"}};
        constexpr ACCESS_MASK writeRights = FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | FILE_DELETE_CHILD |
                                            DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_WRITE | GENERIC_ALL;
        for (const auto &[type, name] : groups) {
            if (dacl == nullptr) break;
            BYTE sid[SECURITY_MAX_SID_SIZE];
            DWORD size = sizeof(sid);
            if (!CreateWellKnownSid(type, nullptr, sid, &size)) continue;
            TRUSTEE_W trustee{};
            BuildTrusteeWithSidW(&trustee, sid);
            ACCESS_MASK rights = 0;
            if (GetEffectiveRightsFromAclW(dacl, &trustee, &rights) == ERROR_SUCCESS && (rights & writeRights) != 0) {
                who = QString::fromLatin1(name);
                break;
            }
        }
        LocalFree(descriptor);
        return who;
    }

    // A PID can be reused once its process is gone; callers check right after accepting the connection.
    bool LocalClientIsAdmin(qintptr descriptor) {
        const auto pipe = reinterpret_cast<HANDLE>(descriptor);
        if (pipe == nullptr || pipe == INVALID_HANDLE_VALUE) return false;
        ULONG pid = 0;
        if (!GetNamedPipeClientProcessId(pipe, &pid) || pid == 0) return false;
        const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process == nullptr) return false;
        HANDLE token = nullptr;
        const bool opened = OpenProcessToken(process, TOKEN_QUERY | TOKEN_DUPLICATE, &token);
        CloseHandle(process);
        if (!opened) return false;

        bool allowed = false;
        const auto user = serviceManagerWinTokenSid(token);
        if (!user.empty() && IsWellKnownSid(serviceManagerWinSid(user), WinLocalSystemSid)) {
            allowed = true;
        } else {
            // CheckTokenMembership needs an impersonation token; a UAC-filtered admin's Administrators SID is deny-only and fails.
            HANDLE identification = nullptr;
            if (DuplicateToken(token, SecurityIdentification, &identification)) {
                alignas(DWORD) BYTE admins[SECURITY_MAX_SID_SIZE];
                DWORD size = sizeof(admins);
                BOOL member = FALSE;
                allowed = CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, admins, &size) &&
                          CheckTokenMembership(identification, admins, &member) && member;
                CloseHandle(identification);
            }
        }
        CloseHandle(token);
        return allowed;
    }

    bool PipeServerTrusted(qintptr descriptor, QString *why) {
        const auto refuse = [why](const QString &text) {
            if (why != nullptr) *why = text;
            return false;
        };
        const auto pipe = reinterpret_cast<HANDLE>(descriptor);
        if (pipe == nullptr || pipe == INVALID_HANDLE_VALUE) return refuse(QStringLiteral("the pipe is not connected"));
        const PSID self = serviceManagerWinSid(serviceManagerWinSelfSid());
        if (self == nullptr) return refuse(QStringLiteral("this process's account could not be read"));

        // All instances of a name share the first one's descriptor, so its owner proves nothing about who serves this one.
        PSID owner = nullptr;
        PACL dacl = nullptr;
        PSECURITY_DESCRIPTOR security = nullptr;
        constexpr SECURITY_INFORMATION what = OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
        if (const DWORD code = GetSecurityInfo(pipe, SE_KERNEL_OBJECT, what, &owner, nullptr, &dacl, nullptr, &security);
            code != ERROR_SUCCESS) {
            return refuse(QStringLiteral("the pipe's owner could not be read: %1").arg(serviceManagerWinError(code)));
        }
        const bool ownerTrusted = serviceManagerWinTrustedOwner(owner);
        const QString ownerText = serviceManagerWinSidText(owner);
        const bool closedToOthers =
            serviceManagerWinClosedToOthers(dacl, self, FILE_CREATE_PIPE_INSTANCE | WRITE_DAC | WRITE_OWNER | GENERIC_WRITE | GENERIC_ALL);
        LocalFree(security);
        if (!ownerTrusted) return refuse(QStringLiteral("the pipe belongs to %1, not this user or SYSTEM").arg(ownerText));

        ULONG pid = 0;
        if (!GetNamedPipeServerProcessId(pipe, &pid) || pid == 0) {
            return refuse(QStringLiteral("the pipe's server process is unknown: %1").arg(serviceManagerWinError(GetLastError())));
        }
        // An elevated or SYSTEM caller can open a real Throne service; anyone else needs a pipe no other account can serve.
        const auto denied = [&refuse, closedToOthers, pid] {
            if (RunningAsLocalSystem() || serviceManagerWinSelfElevated()) {
                return refuse(
                    QStringLiteral("PID %1 serving the pipe cannot be inspected even by an elevated caller, unlike a Throne service")
                        .arg(QString::number(pid)));
            }
            if (!closedToOthers) {
                return refuse(QStringLiteral("PID %1 serving the pipe cannot be inspected, and the pipe lets other users serve it")
                                  .arg(QString::number(pid)));
            }
            return true;
        };
        const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process == nullptr) {
            const DWORD code = GetLastError();
            if (code == ERROR_ACCESS_DENIED) return denied();
            return refuse(
                QStringLiteral("PID %1 serving the pipe cannot be inspected: %2").arg(QString::number(pid), serviceManagerWinError(code)));
        }
        HANDLE token = nullptr;
        const bool opened = OpenProcessToken(process, TOKEN_QUERY, &token);
        const DWORD tokenCode = opened ? ERROR_SUCCESS : GetLastError();
        CloseHandle(process);
        if (!opened) {
            if (tokenCode == ERROR_ACCESS_DENIED) return denied();
            return refuse(
                QStringLiteral("PID %1 serving the pipe cannot be inspected: %2").arg(QString::number(pid), serviceManagerWinError(tokenCode)));
        }
        const auto server = serviceManagerWinTokenSid(token);
        CloseHandle(token);
        const PSID serverSid = serviceManagerWinSid(server);
        if (serverSid == nullptr) return refuse(QStringLiteral("the account of PID %1 serving the pipe could not be read").arg(pid));
        if (EqualSid(serverSid, self) || IsWellKnownSid(serverSid, WinLocalSystemSid)) return true;
        return refuse(QStringLiteral("PID %1 serving the pipe runs as %2, not this user or SYSTEM")
                          .arg(QString::number(pid), serviceManagerWinSidText(serverSid)));
    }
} // namespace Sys::ServiceManager
