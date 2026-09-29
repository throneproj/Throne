#include "include/sys/AutoRun.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <vector>
#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

#include "3rdparty/WinCommander.hpp"

#include <windows.h>
#include <sddl.h>
#include <taskschd.h>

namespace {
    // WRL's ComPtr is off limits: its roapi.h declares namespace Windows, which clashes with osType::Windows.
    template <typename T>
    class TaskPtr {
    public:
        TaskPtr() = default;
        TaskPtr(TaskPtr &&other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
        TaskPtr(const TaskPtr &) = delete;
        TaskPtr &operator=(const TaskPtr &) = delete;
        ~TaskPtr() {
            if (ptr) ptr->Release();
        }
        T **put() { return &ptr; }
        T *get() const { return ptr; }
        T *operator->() const { return ptr; }
        explicit operator bool() const { return ptr != nullptr; }

    private:
        T *ptr = nullptr;
    };

    struct ComApartment {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        ~ComApartment() {
            if (SUCCEEDED(hr)) CoUninitialize();
        }
    };

    struct TaskBstr {
        BSTR value;
        explicit TaskBstr(const QString &s) : value(SysAllocStringLen(reinterpret_cast<const OLECHAR *>(s.utf16()), UINT(s.size()))) {}
        ~TaskBstr() { SysFreeString(value); }
        TaskBstr(const TaskBstr &) = delete;
        TaskBstr &operator=(const TaskBstr &) = delete;
        operator BSTR() const { return value; }
    };

    class TaskRoot {
    public:
        TaskRoot() {
            hr = CoCreateInstance(__uuidof(TaskScheduler), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(service.put()));
            if (SUCCEEDED(hr)) hr = service->Connect(VARIANT{}, VARIANT{}, VARIANT{}, VARIANT{});
            if (SUCCEEDED(hr)) hr = service->GetFolder(TaskBstr("\\"), folder.put());
        }

        HRESULT status() const { return hr; }

        TaskPtr<IRegisteredTask> find(const QString &name) const {
            TaskPtr<IRegisteredTask> task;
            folder->GetTask(TaskBstr(name), task.put());
            return task;
        }

        HRESULT put(const QString &name, const QString &xml) const {
            TaskPtr<IRegisteredTask> task;
            return folder->RegisterTask(TaskBstr(name), TaskBstr(xml), TASK_CREATE_OR_UPDATE, VARIANT{}, VARIANT{},
                                        TASK_LOGON_INTERACTIVE_TOKEN, VARIANT{}, task.put());
        }

        HRESULT remove(const QString &name) const { return folder->DeleteTask(TaskBstr(name), 0); }

    private:
        ComApartment apartment;
        TaskPtr<ITaskService> service;
        TaskPtr<ITaskFolder> folder;
        HRESULT hr = E_FAIL;
    };
}

static QString hresultText(HRESULT hr) {
    LPWSTR text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, DWORD(hr), 0,
                   reinterpret_cast<LPWSTR>(&text), 0, nullptr);
    const QString message = text ? QString::fromWCharArray(text).trimmed() : QString();
    LocalFree(text);
    return QString("%1 (0x%2)").arg(message, QString::number(quint32(hr), 16).toUpper()).trimmed();
}

static QString autoRunUserSid() {
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

static QString autoRunUserName() {
    return qEnvironmentVariable("USERDOMAIN") + "\\" + qEnvironmentVariable("USERNAME");
}

static QString autoRunTaskName(const QString &seed) {
    const QByteArray hash = QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Md5).toHex().left(8);
    return "Throne AutoRun " + QString::fromLatin1(hash);
}

static QString autoRunExeKey() {
    return QDir::cleanPath(QCoreApplication::applicationFilePath()).toLower();
}

// Older builds registered one task for any user's logon, named after the executable alone.
static QString legacyTaskName() {
    return autoRunTaskName(autoRunExeKey());
}

static QString userTaskName() {
    return autoRunTaskName(autoRunExeKey() + "|" + autoRunUserSid());
}

static bool autoRunElevated() {
    return Configs::IsAdmin() && !Configs::dataManager->settingsRepo->disable_run_admin;
}

static QString autoRunTaskXml(bool elevated) {
    const QString runLevel = elevated ? QStringLiteral("HighestAvailable") : QStringLiteral("LeastPrivilege");
    const QString command = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return QString(
        "<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
        "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
        "  <RegistrationInfo>\n"
        "    <Author>%1</Author>\n"
        "  </RegistrationInfo>\n"
        "  <Triggers>\n"
        "    <LogonTrigger>\n"
        "      <Enabled>true</Enabled>\n"
        "      <UserId>%2</UserId>\n"
        "    </LogonTrigger>\n"
        "  </Triggers>\n"
        "  <Principals>\n"
        "    <Principal id=\"Author\">\n"
        "      <UserId>%2</UserId>\n"
        "      <LogonType>InteractiveToken</LogonType>\n"
        "      <RunLevel>%3</RunLevel>\n"
        "    </Principal>\n"
        "  </Principals>\n"
        "  <Settings>\n"
        "    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\n"
        "    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\n"
        "    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\n"
        "    <AllowHardTerminate>false</AllowHardTerminate>\n"
        "    <StartWhenAvailable>false</StartWhenAvailable>\n"
        "    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>\n"
        "    <IdleSettings>\n"
        "      <StopOnIdleEnd>true</StopOnIdleEnd>\n"
        "      <RestartOnIdle>false</RestartOnIdle>\n"
        "    </IdleSettings>\n"
        "    <AllowStartOnDemand>true</AllowStartOnDemand>\n"
        "    <Enabled>true</Enabled>\n"
        "    <Hidden>false</Hidden>\n"
        "    <RunOnlyIfIdle>false</RunOnlyIfIdle>\n"
        "    <WakeToRun>false</WakeToRun>\n"
        "    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\n"
        // Task Scheduler's default of 7 is BELOW_NORMAL_PRIORITY_CLASS, which the core process inherits from us; 4-6 are the interactive band.
        "    <Priority>5</Priority>\n"
        "  </Settings>\n"
        "  <Actions Context=\"Author\">\n"
        "    <Exec>\n"
        "      <Command>\"%4\"</Command>\n"
        "    </Exec>\n"
        "  </Actions>\n"
        "</Task>"
    ).arg(autoRunUserName().toHtmlEscaped(), autoRunUserSid(), runLevel, command.toHtmlEscaped());
}

static bool taskStartsAtLogon(IRegisteredTask *task) {
    VARIANT_BOOL enabled = VARIANT_FALSE;
    TaskPtr<ITaskDefinition> definition;
    TaskPtr<ITriggerCollection> triggers;
    LONG count = 0;
    if (FAILED(task->get_Enabled(&enabled)) || !enabled || FAILED(task->get_Definition(definition.put()))
        || FAILED(definition->get_Triggers(triggers.put())) || FAILED(triggers->get_Count(&count))) return false;
    for (LONG i = 1; i <= count; ++i) {
        TaskPtr<ITrigger> trigger;
        TASK_TRIGGER_TYPE2 type = TASK_TRIGGER_EVENT;
        VARIANT_BOOL triggerEnabled = VARIANT_FALSE;
        if (SUCCEEDED(triggers->get_Item(i, trigger.put())) && SUCCEEDED(trigger->get_Type(&type)) && type == TASK_TRIGGER_LOGON
            && SUCCEEDED(trigger->get_Enabled(&triggerEnabled)) && triggerEnabled) return true;
    }
    return false;
}

// Older tasks carry priority 7, and a missing element means the same; a deliberate bump to a higher priority is left alone.
static bool taskIsStale(IRegisteredTask *task, bool elevated) {
    TaskPtr<ITaskDefinition> definition;
    TaskPtr<IPrincipal> principal;
    TaskPtr<ITaskSettings> settings;
    TASK_RUNLEVEL_TYPE runLevel = TASK_RUNLEVEL_LUA;
    int priority = 7;
    if (FAILED(task->get_Definition(definition.put())) || FAILED(definition->get_Principal(principal.put()))
        || FAILED(definition->get_Settings(settings.put())) || FAILED(principal->get_RunLevel(&runLevel))
        || FAILED(settings->get_Priority(&priority))) return false;
    return (runLevel == TASK_RUNLEVEL_HIGHEST) != elevated || priority > 6;
}

static QString taskAuthor(IRegisteredTask *task) {
    TaskPtr<ITaskDefinition> definition;
    TaskPtr<IRegistrationInfo> info;
    BSTR author = nullptr;
    if (FAILED(task->get_Definition(definition.put())) || FAILED(definition->get_RegistrationInfo(info.put()))
        || FAILED(info->get_Author(&author))) return {};
    const QString result = QString::fromWCharArray(author, SysStringLen(author));
    SysFreeString(author);
    return result;
}

// A task registered from an elevated process may refuse a plain user; only an explicit disable is worth a UAC prompt.
static QString removeTask(const TaskRoot &root, const QString &name) {
    if (!root.find(name)) return {};
    HRESULT hr = root.remove(name);
    if (hr == E_ACCESSDENIED) {
        wchar_t systemDir[MAX_PATH];
        const UINT length = GetSystemDirectoryW(systemDir, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            const QString schtasks = QString::fromWCharArray(systemDir, length) + "\\schtasks.exe";
            WinCommander::runProcessElevated(schtasks, {"/delete", "/tn", name, "/f"}, "", WinCommander::ShowHide, true);
        }
        if (!root.find(name)) hr = S_OK;
    }
    return SUCCEEDED(hr) ? QString() : hresultText(hr);
}

bool AutoRun_SetEnabled(bool enable, QString *error) {
    const TaskRoot root;
    QString failure;
    if (FAILED(root.status())) {
        failure = hresultText(root.status());
    } else if (enable) {
        if (const HRESULT hr = root.put(userTaskName(), autoRunTaskXml(autoRunElevated())); FAILED(hr)) failure = hresultText(hr);
    } else {
        failure = removeTask(root, userTaskName());
        if (failure.isEmpty()) failure = removeTask(root, legacyTaskName());
    }
    if (failure.isEmpty()) return true;
    LOG_WARN(QString("autorun: %1 failed: %2").arg(enable ? QStringLiteral("enabling") : QStringLiteral("disabling"), failure));
    if (error) *error = failure;
    return false;
}

bool AutoRun_IsEnabled() {
    const TaskRoot root;
    if (FAILED(root.status())) return false;
    for (const QString &name : {userTaskName(), legacyTaskName()}) {
        const auto task = root.find(name);
        if (task && taskStartsAtLogon(task.get())) return true;
    }
    return false;
}

void AutoRun_FixTaskIfNeeded() {
    const TaskRoot root;
    if (FAILED(root.status())) return;
    const bool elevated = autoRunElevated();
    const QString name = userTaskName();

    // Both tasks would fire at logon, so the per-user replacement is dropped again when the old one refuses deletion.
    const auto legacy = root.find(legacyTaskName());
    if (legacy && taskStartsAtLogon(legacy.get()) && taskAuthor(legacy.get()).compare(autoRunUserName(), Qt::CaseInsensitive) == 0) {
        HRESULT hr = root.put(name, autoRunTaskXml(elevated));
        if (SUCCEEDED(hr) && FAILED(hr = root.remove(legacyTaskName()))) root.remove(name);
        if (FAILED(hr)) LOG_WARN(QString("autorun: keeping any-user task %1: %2").arg(legacyTaskName(), hresultText(hr)));
        return;
    }

    const auto task = root.find(name);
    if (!task || !taskIsStale(task.get(), elevated)) return;
    if (const HRESULT hr = root.put(name, autoRunTaskXml(elevated)); FAILED(hr)) {
        LOG_WARN(QString("autorun: refreshing %1 failed: %2").arg(name, hresultText(hr)));
    }
}

void AutoRun_MigrateIfNeeded() {
    QSettings run("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    const QString name = QFileInfo(QCoreApplication::applicationFilePath()).baseName();
    const QString command = "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\" -tray";
    if (run.value(name).toString() == command && AutoRun_SetEnabled(true)) run.remove(name);
}
