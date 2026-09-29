//go:build !linux

package mobile

import (
	"net"
	"os"
)

func getTunnelName(fd int32) (string, error) {
	return "", os.ErrInvalid
}

func dup(fd int) (int, error) {
	return 0, os.ErrInvalid
}

func linkFlags(rawFlags uint32) net.Flags {
	return 0
}
