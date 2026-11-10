//go:build windows

package ipc

import (
	"errors"
	"fmt"
	"io"
	"log"
	"net"
	"os"
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

	if err := CheckPipeServer(windows.Handle(fc.Fd()), expectedPid); err != nil {
		conn.Close()
		log.Fatalf("ipc: %v", err)
	}
	return conn, nil
}

// DialPipeHandle connects like ConnectIPC but leaves the handle off this process's IO completion port, so another process
// can adopt it: a file object joins only one port.
func DialPipeHandle(socketName string, expectedPid int) (windows.Handle, error) {
	path, err := windows.UTF16PtrFromString(socketName)
	if err != nil {
		return 0, err
	}
	deadline := time.Now().Add(5 * time.Second)
	var h windows.Handle
	for {
		h, err = windows.CreateFile(path,
			windows.GENERIC_READ|windows.GENERIC_WRITE,
			0,
			nil,
			windows.OPEN_EXISTING,
			windows.FILE_FLAG_OVERLAPPED|windows.SECURITY_SQOS_PRESENT|windows.SECURITY_ANONYMOUS,
			0)
		if err == nil {
			break
		}
		if !errors.Is(err, windows.ERROR_PIPE_BUSY) || time.Now().After(deadline) {
			return 0, &os.PathError{Op: "open", Path: socketName, Err: err}
		}
		time.Sleep(10 * time.Millisecond)
	}

	if err := CheckPipeServer(h, expectedPid); err != nil {
		windows.CloseHandle(h)
		log.Fatalf("ipc: %v", err)
	}
	return h, nil
}

func CheckPipeServer(h windows.Handle, expectedPid int) error {
	var serverPid uint32
	if err := windows.GetNamedPipeServerProcessId(h, &serverPid); err != nil {
		return fmt.Errorf("GetNamedPipeServerProcessId: %w", err)
	}
	if int(serverPid) != expectedPid {
		return fmt.Errorf("pipe server pid %d does not match expected parent pid %d", serverPid, expectedPid)
	}
	return nil
}

// NewPipeConn adopts an overlapped pipe handle another process dialled; it owns h only on success.
func NewPipeConn(h windows.Handle) (net.Conn, error) {
	file, err := winio.NewOpenFile(h)
	if err != nil {
		return nil, err
	}
	return pipeConn{file}, nil
}

type pipeConn struct {
	io.ReadWriteCloser
}

type deadliner interface {
	SetReadDeadline(time.Time) error
	SetWriteDeadline(time.Time) error
}

type pipeAddr struct{}

func (pipeAddr) Network() string { return "pipe" }
func (pipeAddr) String() string  { return "pipe" }

func (pipeConn) LocalAddr() net.Addr  { return pipeAddr{} }
func (pipeConn) RemoteAddr() net.Addr { return pipeAddr{} }

func (c pipeConn) SetDeadline(t time.Time) error {
	if err := c.SetReadDeadline(t); err != nil {
		return err
	}
	return c.SetWriteDeadline(t)
}

func (c pipeConn) SetReadDeadline(t time.Time) error {
	if d, ok := c.ReadWriteCloser.(deadliner); ok {
		return d.SetReadDeadline(t)
	}
	return errors.ErrUnsupported
}

func (c pipeConn) SetWriteDeadline(t time.Time) error {
	if d, ok := c.ReadWriteCloser.(deadliner); ok {
		return d.SetWriteDeadline(t)
	}
	return errors.ErrUnsupported
}
