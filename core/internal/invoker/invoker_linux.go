package invoker

import (
	"errors"
	"fmt"

	"golang.org/x/sys/unix"
)

// setfsuid/setfsgid change only the calling thread and report no error: a repeated call returns the id now in effect.
func setfs(trap uintptr, id int) error {
	unix.RawSyscallNoError(trap, uintptr(id), 0, 0)
	if now, _ := unix.RawSyscallNoError(trap, uintptr(id), 0, 0); uint32(now) != uint32(id) {
		return fmt.Errorf("id is %d, not %d", uint32(now), id)
	}
	return nil
}

func (id identity) assume() error {
	if err := setfs(sysSetfsgid, id.rgid); err != nil {
		return fmt.Errorf("setfsgid: %w", err)
	}
	if err := setfs(sysSetfsuid, id.ruid); err != nil {
		return fmt.Errorf("setfsuid: %w", err)
	}
	return nil
}

func (id identity) revert() error {
	var errs []error
	if err := setfs(sysSetfsuid, id.euid); err != nil {
		errs = append(errs, fmt.Errorf("setfsuid: %w", err))
	}
	if err := setfs(sysSetfsgid, id.egid); err != nil {
		errs = append(errs, fmt.Errorf("setfsgid: %w", err))
	}
	return errors.Join(errs...)
}
