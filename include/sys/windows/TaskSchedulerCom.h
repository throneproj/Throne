#pragma once

#include <QString>

#include <windows.h>
#include <taskschd.h>

// Shared by AutoRun.cpp and CoreElevation.cpp, which an MSVC unity batch may compile as one translation unit.
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
