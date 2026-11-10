package proclist

import (
	"path/filepath"
	"syscall"
	"unsafe"

	"golang.org/x/sys/unix"
)

func List() ([]Entry, error) {
	procs, err := unix.SysctlKinfoProcSlice("kern.proc.all")
	if err != nil {
		return nil, err
	}
	entries := make([]Entry, 0, len(procs))
	for _, proc := range procs {
		pid := uint32(proc.Proc.P_pid)
		if pid == 0 {
			continue
		}
		path, err := execPath(pid)
		if err != nil || path == "" {
			continue
		}
		entries = append(entries, Entry{PID: pid, Name: filepath.Base(path), Path: path})
	}
	return entries, nil
}

func execPath(pid uint32) (string, error) {
	const (
		procpidpathinfo     = 0xb
		procpidpathinfosize = 1024
		proccallnumpidinfo  = 0x2
	)
	buf := make([]byte, procpidpathinfosize)
	_, _, errno := syscall.Syscall6(
		syscall.SYS_PROC_INFO,
		proccallnumpidinfo,
		uintptr(pid),
		procpidpathinfo,
		0,
		uintptr(unsafe.Pointer(&buf[0])),
		procpidpathinfosize)
	if errno != 0 {
		return "", errno
	}
	return unix.ByteSliceToString(buf), nil
}
