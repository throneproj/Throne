//go:build windows

package ipc

import (
	"log"
	"net"
	"time"

	"github.com/tailscale/go-winio"
	"golang.org/x/sys/windows"
)

func ConnectIPC(socketName string, expectedPid int) (net.Conn, error) {
	timeout := 5 * time.Second
	conn, err := winio.DialPipe(socketName, &timeout)
	if err != nil {
		return nil, err
	}

	type fdConn interface{ Fd() uintptr }
	fc, ok := conn.(fdConn)
	if !ok {
		conn.Close()
		log.Fatalf("ipc: pipe connection does not expose Fd() (type %T)", conn)
	}

	var serverPid uint32
	if err := windows.GetNamedPipeServerProcessId(windows.Handle(fc.Fd()), &serverPid); err != nil {
		conn.Close()
		log.Fatalf("ipc: GetNamedPipeServerProcessId: %v", err)
	}
	if int(serverPid) != expectedPid {
		conn.Close()
		log.Fatalf("ipc: pipe server pid %d does not match expected parent pid %d", serverPid, expectedPid)
	}
	return conn, nil
}
