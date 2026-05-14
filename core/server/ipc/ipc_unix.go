//go:build linux || darwin

package ipc

import "net"

func ConnectIPC(socketName string) (net.Conn, error) {
	return net.Dial("unix", socketName)
}
