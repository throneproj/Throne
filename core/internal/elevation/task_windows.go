package elevation

import (
	"errors"
	"fmt"
	"runtime"

	"github.com/go-ole/go-ole"
	"github.com/go-ole/go-ole/oleutil"
)

const (
	taskRunIgnoreConstraints = 0x2
	taskRunUseSessionID      = 0x4
	hresultSFalse            = 1
)

// runTask starts the task through IRegisteredTask::RunEx, which needs no UAC prompt for a task of the caller's own user.
func runTask(path, arg0 string, session uint32) error {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	if err := ole.CoInitializeEx(0, ole.COINIT_MULTITHREADED); err != nil {
		var oleErr *ole.OleError
		if !errors.As(err, &oleErr) || oleErr.Code() != hresultSFalse {
			return fmt.Errorf("initialize COM: %w", err)
		}
	}
	defer ole.CoUninitialize()

	unknown, err := oleutil.CreateObject("Schedule.Service")
	if err != nil {
		return fmt.Errorf("task scheduler: %w", err)
	}
	defer unknown.Release()
	service, err := unknown.QueryInterface(ole.IID_IDispatch)
	if err != nil {
		return fmt.Errorf("task scheduler: %w", err)
	}
	defer service.Release()
	if _, err = oleutil.CallMethod(service, "Connect"); err != nil {
		return fmt.Errorf("connect to the task scheduler: %w", err)
	}
	folder, err := oleutil.CallMethod(service, "GetFolder", `\`)
	if err != nil {
		return fmt.Errorf("task scheduler root folder: %w", err)
	}
	defer folder.Clear()
	task, err := oleutil.CallMethod(folder.ToIDispatch(), "GetTask", path)
	if err != nil {
		return fmt.Errorf("task %s: %w", path, err)
	}
	defer task.Clear()
	running, err := oleutil.CallMethod(task.ToIDispatch(), "RunEx",
		arg0, int32(taskRunIgnoreConstraints|taskRunUseSessionID), int32(session), "")
	if err != nil {
		return fmt.Errorf("run task %s: %w", path, err)
	}
	running.Clear()
	return nil
}
