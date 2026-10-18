//go:build !linux && !darwin

package xray

import (
	"errors"
	"net"
	"syscall"
)

// Xray's listeners here cannot share a port with the probe, so probes are released before the
// instance binds and a port lost in between surfaces as a start failure that is retried.
const probesHoldThroughStart = false

var probeListenConfig net.ListenConfig

const wsaeAddrInUse = syscall.Errno(10048)

func isAddrInUse(err error) bool {
	return errors.Is(err, syscall.EADDRINUSE) || errors.Is(err, wsaeAddrInUse)
}
