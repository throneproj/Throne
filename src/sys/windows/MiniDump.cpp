#include "include/sys/windows/MiniDump.h"
#include "include/global/Logger.hpp"
#include "NkrVersion.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tchar.h>
#include <dbghelp.h>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include <csignal>
#include <cstdlib>
#include <exception>

// The handler runs on the faulting thread with a possibly-corrupt heap: Win32 calls and stack buffers only, no Qt, no allocation.

typedef BOOL(WINAPI *MINIDUMPWRITEDUMP)(
    HANDLE hProcess,
    DWORD dwPid,
    HANDLE hFile,
    MINIDUMP_TYPE DumpType,
    CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

// Resolved from kernel32 at run time: MinGW's werapi.h lacks the flag, and wer.lib is not linked.
typedef HRESULT(WINAPI *WERSETFLAGS)(DWORD dwFlags);
static const DWORD THRONE_WER_FAULT_REPORTING_NO_UI = 0x20;

#if defined(_M_ARM64) || defined(__aarch64__)
#define THRONE_ARCH L"arm64"
#define THRONE_ARCH_A "arm64"
#elif defined(_M_X64) || defined(__x86_64__)
#define THRONE_ARCH L"amd64"
#define THRONE_ARCH_A "amd64"
#elif defined(_M_IX86) || defined(__i386__)
#define THRONE_ARCH L"386"
#define THRONE_ARCH_A "386"
#else
#define THRONE_ARCH L"unknown"
#define THRONE_ARCH_A "unknown"
#endif

// MiniDumpWithIndirectlyReferencedMemory is what makes objects on the stacks readable.
static const MINIDUMP_TYPE DUMP_TYPE = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithIndirectlyReferencedMemory |
    MiniDumpWithDataSegs |
    MiniDumpWithHandleData |
    MiniDumpWithThreadInfo |
    MiniDumpWithUnloadedModules |
    MiniDumpWithProcessThreadData);

// Failures that reach us with no SEH record get a synthesized one carrying these.
#define THRONE_EXC_TERMINATE    0xE0544501
#define THRONE_EXC_PURECALL     0xE0544502
#define THRONE_EXC_INVALIDPARAM 0xE0544503
#define THRONE_EXC_ABORT        0xE0544504
#define THRONE_EXC_NEWFAIL      0xE0544505

// Streams at or above LastReservedStream are free for application use.
static const ULONG32 THRONE_LOG_STREAM = 0x10000;

static wchar_t g_dumpPathTemplate[MAX_PATH] = L"";
static bool g_crashDirReady = false;
static bool g_interactive = true;
static LONG g_reporting = 0;
static char g_logStream[128 * 1024];

static const char *ExceptionName(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INVALID_DISPOSITION: return "INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
        case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
        case 0xE06D7363: return "C++ exception (unhandled)";
        case 0xC0000409: return "STACK_BUFFER_OVERRUN / __fastfail";
        case 0xC0000374: return "HEAP_CORRUPTION";
        case THRONE_EXC_TERMINATE: return "std::terminate (uncaught C++ exception)";
        case THRONE_EXC_PURECALL: return "pure virtual call";
        case THRONE_EXC_INVALIDPARAM: return "CRT invalid parameter";
        case THRONE_EXC_ABORT: return "abort()";
        case THRONE_EXC_NEWFAIL: return "operator new failed";
        default: return "unknown";
    }
}

// wsprintf documents no %p, so pointers are rendered by hand and passed as %s.
static void HexPtrA(char out[17], const void *p) {
    static const char digits[] = "0123456789ABCDEF";
    const ULONG64 v = static_cast<ULONG64>(reinterpret_cast<ULONG_PTR>(p));
    for (int i = 0; i < 16; ++i) out[i] = digits[(v >> ((15 - i) * 4)) & 0xF];
    out[16] = '\0';
}

static void HexPtrW(wchar_t out[17], const void *p) {
    char narrow[17];
    HexPtrA(narrow, p);
    for (int i = 0; i < 17; ++i) out[i] = static_cast<wchar_t>(narrow[i]);
}

static void AppendA(HANDLE file, const char *text) {
    if (text == nullptr) return;
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(lstrlenA(text)), &written, nullptr);
}

// memcpy only: the ring entries were already encoded while the process was healthy.
static ULONG32 BuildLogStream() {
    static const QByteArray *lines[Logging::MAX_RECENT_LINES];
    const int count = Logging::RecentLinesRaw(lines, Logging::MAX_RECENT_LINES);
    ULONG32 used = 0;
    for (int i = 0; i < count; ++i) {
        if (lines[i] == nullptr) continue;
        const int n = lines[i]->size();
        if (used + static_cast<ULONG32>(n) + 2 >= sizeof(g_logStream)) break;
        CopyMemory(g_logStream + used, lines[i]->constData(), n);
        used += n;
        g_logStream[used++] = '\r';
        g_logStream[used++] = '\n';
    }
    return used;
}

static void WriteSidecar(const wchar_t *path, EXCEPTION_POINTERS *pException, bool dumpWritten) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;

    char line[1024];
    SYSTEMTIME st;
    GetLocalTime(&st);

    AppendA(file, "Throne crash report\r\n===================\r\n");
    wsprintfA(line, "time      : %04d-%02d-%02d %02d:%02d:%02d\r\n",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    AppendA(file, line);
    AppendA(file, "version   : " NKR_VERSION "\r\n");
    AppendA(file, "arch      : " THRONE_ARCH_A "\r\n");
    AppendA(file, "built     : " __DATE__ " " __TIME__ "\r\n");
    wsprintfA(line, "pid/tid   : %lu / %lu\r\n", GetCurrentProcessId(), GetCurrentThreadId());
    AppendA(file, line);

    const EXCEPTION_RECORD *record = pException != nullptr ? pException->ExceptionRecord : nullptr;
    if (record != nullptr) {
        char hex[17];
        wsprintfA(line, "exception : 0x%08X (%s)\r\n", record->ExceptionCode, ExceptionName(record->ExceptionCode));
        AppendA(file, line);
        HexPtrA(hex, record->ExceptionAddress);
        wsprintfA(line, "address   : 0x%s\r\nflags     : 0x%08X\r\nparams    : %u\r\n",
                  hex, record->ExceptionFlags, record->NumberParameters);
        AppendA(file, line);

        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            const ULONG_PTR op = record->ExceptionInformation[0];
            HexPtrA(hex, reinterpret_cast<void *>(record->ExceptionInformation[1]));
            wsprintfA(line, "av        : %s at 0x%s\r\n",
                      op == 0 ? "read" : (op == 1 ? "write" : "execute"), hex);
            AppendA(file, line);
        }

        HMODULE mod = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCWSTR>(record->ExceptionAddress), &mod) && mod != nullptr) {
            wchar_t modPath[MAX_PATH] = L"";
            if (GetModuleFileNameW(mod, modPath, MAX_PATH) > 0) {
                const ULONG_PTR offset = reinterpret_cast<ULONG_PTR>(record->ExceptionAddress) -
                                         reinterpret_cast<ULONG_PTR>(mod);
                AppendA(file, "module    : ");
                char narrow[MAX_PATH * 2] = "";
                WideCharToMultiByte(CP_UTF8, 0, modPath, -1, narrow, sizeof(narrow) - 1, nullptr, nullptr);
                AppendA(file, narrow);
                HexPtrA(hex, reinterpret_cast<void *>(offset));
                wsprintfA(line, " + 0x%s\r\n", hex);
                AppendA(file, line);
            }
        }
    }
    AppendA(file, dumpWritten ? "minidump  : written alongside this file\r\n"
                              : "minidump  : FAILED to write\r\n");

    AppendA(file, "\r\n--- recent log ---\r\n");
    // Static, not a local: a stack-overflow crash gets here with little stack left.
    static const QByteArray *lines[Logging::MAX_RECENT_LINES];
    const int count = Logging::RecentLinesRaw(lines, Logging::MAX_RECENT_LINES);
    for (int i = 0; i < count; ++i) {
        if (lines[i] == nullptr || lines[i]->isEmpty()) continue;
        DWORD written = 0;
        WriteFile(file, lines[i]->constData(), static_cast<DWORD>(lines[i]->size()), &written, nullptr);
        WriteFile(file, "\r\n", 2, &written, nullptr);
    }
    if (count == 0) AppendA(file, "(no log lines captured)\r\n");

    FlushFileBuffers(file);
    CloseHandle(file);
}

static void WriteCrashArtifacts(EXCEPTION_POINTERS *pException) {
    // A fault inside this path would re-enter through the filter forever.
    if (InterlockedCompareExchange(&g_reporting, 1, 0) != 0) return;

    Logging::FlushForCrash();
    if (!g_crashDirReady) return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t dumpPath[MAX_PATH];
    wchar_t sidecarPath[MAX_PATH];
    wsprintfW(dumpPath, L"%s\\Throne_%s_%s_%04d%02d%02d-%02d%02d%02d.dmp",
              g_dumpPathTemplate, L"" NKR_VERSION, THRONE_ARCH,
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    wsprintfW(sidecarPath, L"%s\\Throne_%s_%s_%04d%02d%02d-%02d%02d%02d.txt",
              g_dumpPathTemplate, L"" NKR_VERSION, THRONE_ARCH,
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    bool dumpWritten = false;
    HMODULE dllHandle = LoadLibrary(_T("DBGHELP.DLL"));
    if (dllHandle != nullptr) {
        MINIDUMPWRITEDUMP dump = (MINIDUMPWRITEDUMP) GetProcAddress(dllHandle, "MiniDumpWriteDump");
        if (dump != nullptr) {
            HANDLE dumpHandle = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr,
                                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (dumpHandle != INVALID_HANDLE_VALUE) {
                MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
                dumpInfo.ExceptionPointers = pException;
                dumpInfo.ThreadId = GetCurrentThreadId();
                // In-process dump: dbghelp reads the pointers directly.
                dumpInfo.ClientPointers = FALSE;

                MINIDUMP_USER_STREAM logStream;
                logStream.Type = THRONE_LOG_STREAM;
                logStream.BufferSize = BuildLogStream();
                logStream.Buffer = g_logStream;
                MINIDUMP_USER_STREAM_INFORMATION streamInfo;
                streamInfo.UserStreamCount = logStream.BufferSize > 0 ? 1 : 0;
                streamInfo.UserStreamArray = &logStream;

                dumpWritten = dump(GetCurrentProcess(), GetCurrentProcessId(), dumpHandle,
                                   DUMP_TYPE, &dumpInfo, &streamInfo, nullptr) != FALSE;
                CloseHandle(dumpHandle);
            }
        }
    }

    WriteSidecar(sidecarPath, pException, dumpWritten);

    if (!g_interactive) {
        // Nobody may be there to dismiss a box (a service, a headless run), and it would hold the process forever.
        const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
        if (err != nullptr && err != INVALID_HANDLE_VALUE) {
            char narrow[MAX_PATH * 3] = "";
            WideCharToMultiByte(CP_UTF8, 0, g_dumpPathTemplate, -1, narrow, sizeof(narrow) - 1, nullptr, nullptr);
            AppendA(err, "Throne crashed; a crash report was saved to ");
            AppendA(err, narrow);
            AppendA(err, "\r\n");
        }
        return;
    }

    // Win32 MessageBox, not QMessageBox: a QWidget built off the GUI thread faults again and re-enters the filter.
    wchar_t msg[MAX_PATH + 512];
    const EXCEPTION_RECORD *record = pException != nullptr ? pException->ExceptionRecord : nullptr;
    wchar_t addrHex[17];
    HexPtrW(addrHex, record != nullptr ? record->ExceptionAddress : nullptr);
    wsprintfW(msg,
              L"Throne %s (%s) crashed.\n\nException: 0x%08X\nAddress: 0x%s\n\n"
              L"A crash report was saved to:\n%s\n\nPlease attach the .txt file (and the .dmp if you can) to a bug report.",
              L"" NKR_VERSION, THRONE_ARCH,
              record != nullptr ? record->ExceptionCode : 0,
              addrHex,
              g_dumpPathTemplate);
    MessageBoxW(nullptr, msg, L"Throne crashed", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

LONG __stdcall CreateCrashHandler(EXCEPTION_POINTERS *pException) {
    WriteCrashArtifacts(pException);
    return EXCEPTION_EXECUTE_HANDLER;
}

// The CRT failure paths below terminate the process without raising an SEH exception, so the filter never sees them.
static void ReportSynthetic(DWORD code) {
    CONTEXT context;
    RtlCaptureContext(&context);

    EXCEPTION_RECORD record;
    ZeroMemory(&record, sizeof(record));
    record.ExceptionCode = code;
    record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
#if defined(_M_ARM64) || defined(__aarch64__)
    record.ExceptionAddress = reinterpret_cast<PVOID>(context.Pc);
#elif defined(_M_X64) || defined(__x86_64__)
    record.ExceptionAddress = reinterpret_cast<PVOID>(context.Rip);
#else
    record.ExceptionAddress = reinterpret_cast<PVOID>(context.Eip);
#endif

    EXCEPTION_POINTERS pointers;
    pointers.ExceptionRecord = &record;
    pointers.ContextRecord = &context;
    WriteCrashArtifacts(&pointers);

    // Do not let the CRT continue unwinding into undefined behaviour.
    TerminateProcess(GetCurrentProcess(), code);
}

static void OnTerminate() { ReportSynthetic(THRONE_EXC_TERMINATE); }

static void OnPureCall() { ReportSynthetic(THRONE_EXC_PURECALL); }

static void OnInvalidParameter(const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t) {
    ReportSynthetic(THRONE_EXC_INVALIDPARAM);
}

static void OnAbort(int) { ReportSynthetic(THRONE_EXC_ABORT); }

static void OnNewFailed() { ReportSynthetic(THRONE_EXC_NEWFAIL); }

void Windows_SetCrashHandler(bool interactive) {
    g_interactive = interactive;
    SetErrorMode(interactive ? SEM_FAILCRITICALERRORS
                             : SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    if (!interactive) {
        // A __fastfail or heap corruption never reaches the filter, and WER's own dialog would hold a session-0 process forever.
        const HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        const WERSETFLAGS werSetFlags = kernel != nullptr
            ? reinterpret_cast<WERSETFLAGS>(reinterpret_cast<void (*)()>(GetProcAddress(kernel, "WerSetFlags")))
            : nullptr;
        if (werSetFlags != nullptr) werSetFlags(THRONE_WER_FAULT_REPORTING_NO_UI);
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif
    }
    SetUnhandledExceptionFilter(CreateCrashHandler);

    // STATUS_STACK_OVERFLOW reaches the filter with one guard page left, not enough to write a dump (per-thread setting).
    ULONG stackGuarantee = 64 * 1024;
    SetThreadStackGuarantee(&stackGuarantee);

    std::set_terminate(OnTerminate);
    std::set_new_handler(OnNewFailed);
    _set_purecall_handler(OnPureCall);
    _set_invalid_parameter_handler(OnInvalidParameter);
    signal(SIGABRT, OnAbort);
}

bool Windows_WindowStationVisible() {
    const HWINSTA station = GetProcessWindowStation();
    if (station == nullptr) return false;
    USEROBJECTFLAGS flags;
    ZeroMemory(&flags, sizeof(flags));
    DWORD needed = 0;
    // Unknown keeps the interactive default: only a station known to be invisible (session 0) drops the box.
    if (!GetUserObjectInformationW(station, UOI_FLAGS, &flags, sizeof(flags), &needed)) return true;
    return (flags.dwFlags & WSF_VISIBLE) != 0;
}

void Windows_SetCrashDumpPath() {
    const wchar_t *dir = Logging::CrashDirNative();
    if (dir == nullptr || *dir == L'\0') return;
    lstrcpynW(g_dumpPathTemplate, dir, MAX_PATH);
    g_crashDirReady = true;
}

enum class WerRegistration { Written, Kept, Failed };

// WER reads LocalDumps from HKLM only; an HKCU copy is silently ignored, so this needs admin.
static WerRegistration RegisterWerApp(const wchar_t *exeName, const wchar_t *folder, bool onlyIfAbsent) {
    wchar_t subKey[512];
    wsprintfW(subKey, L"SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting\\LocalDumps\\%s", exeName);

    HKEY key = nullptr;
    // Readable without admin, so an unelevated headless still reports a present key as kept, not as a failure.
    if (onlyIfAbsent && RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return WerRegistration::Kept;
    }
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) {
        return WerRegistration::Failed;
    }
    if (onlyIfAbsent && disposition == REG_OPENED_EXISTING_KEY) {
        RegCloseKey(key);
        return WerRegistration::Kept;
    }
    RegSetValueExW(key, L"DumpFolder", 0, REG_EXPAND_SZ,
                   reinterpret_cast<const BYTE *>(folder),
                   static_cast<DWORD>((lstrlenW(folder) + 1) * sizeof(wchar_t)));
    const DWORD dumpCount = 5;
    RegSetValueExW(key, L"DumpCount", 0, REG_DWORD,
                   reinterpret_cast<const BYTE *>(&dumpCount), sizeof(dumpCount));
    // DumpType 0 = custom, so CustomDumpFlags decides.
    const DWORD dumpType = 0;
    RegSetValueExW(key, L"DumpType", 0, REG_DWORD,
                   reinterpret_cast<const BYTE *>(&dumpType), sizeof(dumpType));
    const DWORD customFlags = static_cast<DWORD>(DUMP_TYPE);
    RegSetValueExW(key, L"CustomDumpFlags", 0, REG_DWORD,
                   reinterpret_cast<const BYTE *>(&customFlags), sizeof(customFlags));
    RegCloseKey(key);
    return WerRegistration::Written;
}

void Windows_ConfigureWER(bool onlyIfAbsent) {
    if (!g_crashDirReady) return;
    // A non-interactive run is headless whatever the caller passed: the GUI and the service share Throne.exe's key.
    const bool keepExisting = onlyIfAbsent || !g_interactive;

    wchar_t exePath[MAX_PATH] = L"";
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) == 0) return;
    const wchar_t *exeName = exePath;
    for (const wchar_t *p = exePath; *p != L'\0'; ++p) {
        if (*p == L'\\' || *p == L'/') exeName = p + 1;
    }

    // The core is a separate process: its fatal Go runtime aborts are invisible to anything installed in this one.
    const WerRegistration self = RegisterWerApp(exeName, g_dumpPathTemplate, keepExisting);
    const WerRegistration core = RegisterWerApp(L"ThroneCore.exe", g_dumpPathTemplate, keepExisting);
    const QString selfName = QString::fromWCharArray(exeName);
    if (self == WerRegistration::Failed || core == WerRegistration::Failed) {
        LOG_INFO("WER LocalDumps not registered (needs admin); crashes that bypass "
                 "the exception filter will go to %LOCALAPPDATA%\\CrashDumps if "
                 "LocalDumps is enabled machine-wide");
    } else if (self == WerRegistration::Kept && core == WerRegistration::Kept) {
        LOG_INFO(QStringLiteral("WER LocalDumps already registered for %1 and ThroneCore.exe; left as is").arg(selfName));
    } else if (self == WerRegistration::Kept || core == WerRegistration::Kept) {
        LOG_INFO(QStringLiteral("WER LocalDumps registered for %1; the existing key for %2 was left as is")
                     .arg(self == WerRegistration::Written ? selfName : QStringLiteral("ThroneCore.exe"),
                          self == WerRegistration::Kept ? selfName : QStringLiteral("ThroneCore.exe")));
    } else {
        LOG_INFO(QStringLiteral("WER LocalDumps registered for %1 and ThroneCore.exe").arg(selfName));
    }
}
