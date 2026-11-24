//go:build windows

package scan

import (
	"errors"
	"slices"
	"syscall"

	"golang.org/x/sys/windows"
)

var routingErrnos = []syscall.Errno{
	windows.WSAENETUNREACH, windows.WSAEHOSTUNREACH, windows.WSAEADDRNOTAVAIL, windows.WSAEAFNOSUPPORT, windows.WSAENETDOWN,
	windows.ERROR_NETWORK_UNREACHABLE, windows.ERROR_HOST_UNREACHABLE,
	// Go's invented errno, which wireguard-go's bind returns for a family whose socket never opened.
	syscall.EAFNOSUPPORT,
}

func routingError(err error) bool {
	var errno syscall.Errno
	return errors.As(err, &errno) && slices.Contains(routingErrnos, errno)
}
