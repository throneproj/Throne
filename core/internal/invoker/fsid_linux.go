//go:build !386 && !arm

package invoker

import "golang.org/x/sys/unix"

const (
	sysSetfsuid = unix.SYS_SETFSUID
	sysSetfsgid = unix.SYS_SETFSGID
)
