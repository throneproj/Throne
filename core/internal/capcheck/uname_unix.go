//go:build linux || darwin

package capcheck

import "golang.org/x/sys/unix"

func kernelRelease() string {
	var name unix.Utsname
	if unix.Uname(&name) != nil {
		return ""
	}
	return unix.ByteSliceToString(name.Release[:])
}
