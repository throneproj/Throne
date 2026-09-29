//go:build darwin

package ipc

import "golang.org/x/sys/unix"

func getServerPid(fd int) (int, error) {
	return unix.GetsockoptInt(fd, unix.SOL_LOCAL, unix.LOCAL_PEERPID)
}
