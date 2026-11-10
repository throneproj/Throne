
#include "include/sys/windows/guihelper.h"

#include <QDir>
#include <QWidget>

#include <string>

#include <windows.h>
#include <shlobj.h>

void Windows_QWidget_SetForegroundWindow(QWidget *w) {
    HWND hForgroundWnd = GetForegroundWindow();
    DWORD dwForeID = ::GetWindowThreadProcessId(hForgroundWnd, NULL);
    DWORD dwCurID = ::GetCurrentThreadId();
    const bool attach = dwForeID != 0 && dwForeID != dwCurID &&
        AttachThreadInput(dwCurID, dwForeID, TRUE);
    SetForegroundWindow((HWND) w->winId());
    if (attach) AttachThreadInput(dwCurID, dwForeID, FALSE);
}

int isThisAdmin = -1;

bool Windows_IsInAdmin() {
    if (isThisAdmin >= 0) return isThisAdmin;
    isThisAdmin = IsUserAnAdmin();
    return isThisAdmin;
}

QString Windows_FinalPath(const QString& path) {
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    HANDLE file = CreateFileW(native.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetFinalPathNameByHandleW(file, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(file);
    if (length == 0 || length >= buffer.size()) return {};
    QString result = QString::fromWCharArray(buffer.data(), static_cast<qsizetype>(length));
    if (result.startsWith(QStringLiteral("\\\\?\\UNC\\"))) return QStringLiteral("\\\\") + result.mid(8);
    if (result.startsWith(QStringLiteral("\\\\?\\"))) return result.mid(4);
    return result;
}
