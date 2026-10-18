//go:build linux || darwin

package xray

import (
	"errors"
	"net"
	"syscall"

	"golang.org/x/sys/unix"
)

// Xray sets SO_REUSEPORT on every listener here, so a probe that sets it too keeps its port reserved
// while the instance binds it. Without the overlap another socket could take the port in between, and a
// second Xray instance taking it would share it silently instead of failing.
const probesHoldThroughStart = true

var probeListenConfig = net.ListenConfig{
	Control: func(_, _ string, conn syscall.RawConn) error {
		var optErr error
		if err := conn.Control(func(fd uintptr) {
			optErr = unix.SetsockoptInt(int(fd), unix.SOL_SOCKET, unix.SO_REUSEPORT, 1)
		}); err != nil {
			return err
		}
		return optErr
	},
}

func isAddrInUse(err error) bool {
	return errors.Is(err, syscall.EADDRINUSE)
}
