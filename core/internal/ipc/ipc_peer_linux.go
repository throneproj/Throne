//go:build linux

package ipc

import "golang.org/x/sys/unix"

func getServerPid(fd int) (int, error) {
	ucred, err := unix.GetsockoptUcred(fd, unix.SOL_SOCKET, unix.SO_PEERCRED)
	if err != nil {
		return 0, err
	}
	return int(ucred.Pid), nil
}
