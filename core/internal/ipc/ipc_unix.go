//go:build linux || darwin

package ipc

import (
	"log"
	"net"
)

func ConnectIPC(socketName string, expectedPid int) (net.Conn, error) {
	conn, err := net.Dial("unix", socketName)
	if err != nil {
		return nil, err
	}

	uc, ok := conn.(*net.UnixConn)
	if !ok {
		conn.Close()
		log.Fatalf("ipc: dial returned %T, expected *net.UnixConn", conn)
	}
	rc, err := uc.SyscallConn()
	if err != nil {
		conn.Close()
		log.Fatalf("ipc: SyscallConn: %v", err)
	}

	var peerPid int
	var sockErr error
	if cerr := rc.Control(func(fd uintptr) {
		peerPid, sockErr = getServerPid(int(fd))
	}); cerr != nil {
		conn.Close()
		log.Fatalf("ipc: Control: %v", cerr)
	}
	if sockErr != nil {
		conn.Close()
		log.Fatalf("ipc: get peer pid: %v", sockErr)
	}
	if peerPid != expectedPid {
		conn.Close()
		log.Fatalf("ipc: socket peer pid %d does not match expected parent pid %d", peerPid, expectedPid)
	}
	return conn, nil
}
