#include "include/global/DeviceDetailsHelper.hpp"

#include "include/global/Logger.hpp"

#include <QString>
#include <QSysInfo>
#include <QFile>
#include <QDir>
#include <QMutex>
#include <QSaveFile>
#include <QUuid>
#include <vector>   
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#include "include/sys/windows/WinVersion.h"
#include <include/sys/Process.hpp>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#endif

#ifdef Q_OS_WIN
static QString queryWmiProperty(const QString& wmiClass, const QString& property) {
    HRESULT hres;

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return QString();

    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );
    if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
        CoUninitialize();
        return QString();
    }

    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(
        CLSID_WbemLocator, 0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc
    );
    if (FAILED(hres)) {
        CoUninitialize();
        return QString();
    }

    IWbemServices* pSvc = NULL;
    BSTR bstrNamespace = SysAllocString(L"ROOT\\CIMV2");
    hres = pLoc->ConnectServer(
        bstrNamespace,
        NULL, NULL, NULL, 0, NULL, 0, &pSvc
    );
    SysFreeString(bstrNamespace);
    if (FAILED(hres)) {
        pLoc->Release();
        CoUninitialize();
        return QString();
    }

    hres = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE
    );
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return QString();
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    QString query = QString("SELECT %1 FROM %2").arg(property, wmiClass);
    BSTR bstrWQL = SysAllocString(L"WQL");
    BSTR bstrQuery = SysAllocString(query.toStdWString().c_str());
    hres = pSvc->ExecQuery(
        bstrWQL,
        bstrQuery,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );
    SysFreeString(bstrWQL);
    SysFreeString(bstrQuery);
    if (FAILED(hres)) {
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return QString();
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    QString result;

    if (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (uReturn) {
            VARIANT vtProp;
            VariantInit(&vtProp);
            hr = pclsObj->Get(property.toStdWString().c_str(), 0, &vtProp, 0, 0);
            if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR) {
                result = QString::fromWCharArray(vtProp.bstrVal);
            }
            VariantClear(&vtProp);
            pclsObj->Release();
        }
        pEnumerator->Release();
    }

    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    return result;
}

static QString winBaseBoard() {
    return queryWmiProperty("Win32_BaseBoard", "Product");
}

static QString winModel() {
    return queryWmiProperty("Win32_ComputerSystem", "Model");
}
#endif

namespace {
    QMutex g_deviceDetailsStoreMutex;
    QString g_deviceDetailsStoreDir;

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
    QString deviceDetailsStoreDir() {
        QMutexLocker locker(&g_deviceDetailsStoreMutex);
        return g_deviceDetailsStoreDir;
    }

    // The machine-id format: 32 hex digits ("uninitialized" and an empty file fail).
    bool deviceDetailsIsMachineId(const QString &id) {
        if (id.size() != 32) return false;
        for (const QChar c : id) {
            const char16_t u = c.unicode();
            if (!((u >= u'0' && u <= u'9') || (u >= u'a' && u <= u'f') || (u >= u'A' && u <= u'F'))) return false;
        }
        return true;
    }

    // Unix without a usable machine-id: a generated ID kept in <configDir>/hwid, so the x-hwid header stays the same across runs.
    QString deviceDetailsPersistedId() {
        const QString dir = deviceDetailsStoreDir();
        if (dir.isEmpty()) return {};
        const QString path = QDir(dir).absoluteFilePath(QStringLiteral("hwid"));
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly)) {
            const QString id = QString::fromLatin1(existing.read(256)).trimmed();
            if (deviceDetailsIsMachineId(id)) return id;
        }
        const QString id = QUuid::createUuid().toString(QUuid::Id128);
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            file.write(id.toLatin1() + '\n');
            if (file.commit()) {
                LOG_INFO(QStringLiteral("no usable /etc/machine-id; generated a device ID in %1").arg(path));
                return id;
            }
        }
        // A per-run ID would count as a new device on every launch; sending none is the lesser harm.
        LOG_WARN(QStringLiteral("no usable /etc/machine-id, and %1 cannot be written (%2); subscriptions are fetched without a device ID")
                     .arg(path, file.errorString()));
        return {};
    }
#endif

#ifdef Q_OS_LINUX
    QString deviceDetailsMachineId(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        const QString id = QString::fromUtf8(file.read(256)).trimmed();
        return deviceDetailsIsMachineId(id) ? id : QString();
    }
#endif
} // namespace

void SetDeviceIdStore(const QString &configDir) {
    QMutexLocker locker(&g_deviceDetailsStoreMutex);
    g_deviceDetailsStoreDir = configDir;
}

DeviceDetails GetDeviceDetails() {
    static const DeviceDetails details = []() {
        DeviceDetails d;

    #ifdef Q_OS_WIN
        d.hwid = QSysInfo::machineUniqueId();
        if (d.hwid.isEmpty()) {
            auto productType = QSysInfo::productType().toUtf8();
            d.hwid = QString("%1-%2").arg(QSysInfo::machineHostName(), QString::fromUtf8(productType));
        }

        d.os = QStringLiteral("Windows");

        VersionInfo info;
        WinVersion::GetVersion(info);
        d.osVersion = QString("%1.%2.%3").arg(info.Major).arg(info.Minor).arg(info.BuildNum);
        
        auto wm = winModel();
        auto wbb = winBaseBoard();
        d.model = (wm == wbb) ? wm : wm + "/" + wbb;
        if (d.hwid.isEmpty()) d.model = QSysInfo::prettyProductName();
    #elif defined(Q_OS_LINUX)
        QString mid = deviceDetailsMachineId(QStringLiteral("/etc/machine-id"));
        if (mid.isEmpty()) mid = deviceDetailsMachineId(QStringLiteral("/var/lib/dbus/machine-id"));
        if (mid.isEmpty()) mid = deviceDetailsPersistedId();
        d.hwid = mid;
        d.os = QStringLiteral("Linux");
        d.osVersion = QSysInfo::kernelVersion();
        d.model = QSysInfo::prettyProductName();
    #elif defined(Q_OS_MACOS)
        d.hwid = QSysInfo::machineUniqueId();
        d.os = QStringLiteral("macOS");
        d.osVersion = QSysInfo::productVersion();
        d.model = QSysInfo::prettyProductName();
    #else
        d.hwid = QSysInfo::machineUniqueId();
        if (d.hwid.isEmpty()) d.hwid = deviceDetailsPersistedId();
        d.os = QSysInfo::productType();
        d.osVersion = QSysInfo::productVersion();
        d.model = QSysInfo::prettyProductName();
    #endif
        return d;
    }();
    return details;
}
