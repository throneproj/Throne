package invoker

import (
	"errors"
	"fmt"
	"syscall"
)

// KAUTH_UID_NONE and KAUTH_GID_NONE (xnu bsd/sys/kauth.h): settid with them drops the thread's own identity.
const kauthNone = ^uint32(0) - 100

// settid is pthread_setugid_np: unlike seteuid, which is process-wide, it sets an identity for the calling thread only.
func settid(uid, gid uint32) error {
	if _, _, errno := syscall.RawSyscall(syscall.SYS_SETTID, uintptr(uid), uintptr(gid), 0); errno != 0 {
		return errno
	}
	return nil
}

func runsAs(uid, gid int) error {
	if euid, egid := syscall.Geteuid(), syscall.Getegid(); euid != uid || egid != gid {
		return fmt.Errorf("thread runs as %d:%d, not %d:%d", euid, egid, uid, gid)
	}
	return nil
}

func (id identity) assume() error {
	if err := settid(uint32(id.ruid), uint32(id.rgid)); err != nil {
		return fmt.Errorf("settid: %w", err)
	}
	return runsAs(id.ruid, id.rgid)
}

// settid refuses to revert a thread that assumed nothing, which is fine as long as it runs as the process.
func (id identity) revert() error {
	err := settid(kauthNone, kauthNone)
	if checkErr := runsAs(id.euid, id.egid); checkErr != nil {
		return errors.Join(err, checkErr)
	}
	return nil
}
