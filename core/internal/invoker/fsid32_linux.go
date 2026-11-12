//go:build 386 || arm

package invoker

import "golang.org/x/sys/unix"

// The plain numbers here are the legacy 16-bit-id calls.
const (
	sysSetfsuid = unix.SYS_SETFSUID32
	sysSetfsgid = unix.SYS_SETFSGID32
)
