//go:build windows

package ipc

import (
	"net"
	"time"

	"github.com/tailscale/go-winio"
)

func ConnectIPC(socketName string) (net.Conn, error) {
	timeout := 5 * time.Second
	return winio.DialPipe(socketName, &timeout)
}
