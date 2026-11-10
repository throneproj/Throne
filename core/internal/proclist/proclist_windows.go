package proclist

import (
	"errors"
	"path/filepath"
	"strings"
	"syscall"
	"unsafe"

	"golang.org/x/sys/windows"
	"golang.org/x/sys/windows/registry"
)

type processIDInformation struct {
	ProcessID uintptr
	ImageName windows.NTUnicodeString
}

type dosDevice struct {
	prefix string
	drive  string
}

func List() ([]Entry, error) {
	snapshot, err := windows.CreateToolhelp32Snapshot(windows.TH32CS_SNAPPROCESS, 0)
	if err != nil {
		return nil, err
	}
	defer windows.CloseHandle(snapshot)
	devices := dosDevices()
	buf := make([]uint16, syscall.MAX_LONG_PATH)
	var entries []Entry
	process := windows.ProcessEntry32{Size: uint32(unsafe.Sizeof(windows.ProcessEntry32{}))}
	for err = windows.Process32First(snapshot, &process); err == nil; err = windows.Process32Next(snapshot, &process) {
		pid := process.ProcessID
		if pid == 0 || pid == 4 {
			continue
		}
		name := windows.UTF16ToString(process.ExeFile[:])
		path, pathErr := processPath(pid, buf)
		if pathErr != nil {
			path = dosPath(ntImagePath(pid, buf), devices)
		}
		if path != "" {
			name = filepath.Base(path)
		}
		if name == "" {
			continue
		}
		entries = append(entries, Entry{PID: pid, Name: name, Path: path})
	}
	entries = append(entries, services()...)
	if errors.Is(err, windows.ERROR_NO_MORE_FILES) {
		err = nil
	}
	return entries, err
}

func processPath(pid uint32, buf []uint16) (string, error) {
	handle, err := windows.OpenProcess(windows.PROCESS_QUERY_LIMITED_INFORMATION, false, pid)
	if err != nil {
		return "", err
	}
	defer windows.CloseHandle(handle)
	size := uint32(len(buf))
	err = windows.QueryFullProcessImageName(handle, 0, &buf[0], &size)
	if err != nil {
		return "", err
	}
	return windows.UTF16ToString(buf[:size]), nil
}

// SystemProcessIdInformation needs no process handle, so it still names the processes an unelevated core cannot open.
func ntImagePath(pid uint32, buf []uint16) string {
	info := processIDInformation{
		ProcessID: uintptr(pid),
		ImageName: windows.NTUnicodeString{MaximumLength: uint16(min(len(buf)*2, 0xFFFE)), Buffer: &buf[0]},
	}
	if windows.NtQuerySystemInformation(windows.SystemProcessIdInformation, unsafe.Pointer(&info), uint32(unsafe.Sizeof(info)), nil) != nil {
		return ""
	}
	return windows.UTF16ToString(buf[:info.ImageName.Length/2])
}

func dosDevices() []dosDevice {
	devices := []dosDevice{{prefix: `\Device\Mup`, drive: `\`}}
	target := make([]uint16, windows.MAX_PATH)
	for letter := 'A'; letter <= 'Z'; letter++ {
		device := [3]uint16{uint16(letter), ':', 0}
		n, err := windows.QueryDosDevice(&device[0], &target[0], uint32(len(target)))
		if err != nil || n == 0 {
			continue
		}
		prefix := windows.UTF16ToString(target[:n])
		if prefix == "" {
			continue
		}
		devices = append(devices, dosDevice{prefix: prefix, drive: string(letter) + ":"})
	}
	return devices
}

func dosPath(ntPath string, devices []dosDevice) string {
	for _, device := range devices {
		n := len(device.prefix)
		if len(ntPath) > n && ntPath[n] == '\\' && strings.EqualFold(ntPath[:n], device.prefix) {
			return device.drive + ntPath[n:]
		}
	}
	return ""
}

func services() []Entry {
	scm, err := windows.OpenSCManager(nil, nil, windows.SC_MANAGER_CONNECT|windows.SC_MANAGER_ENUMERATE_SERVICE)
	if err != nil {
		return nil
	}
	defer windows.CloseServiceHandle(scm)
	var buf []byte
	var needed, count uint32
	for {
		var p *byte
		if len(buf) > 0 {
			p = &buf[0]
		}
		err = windows.EnumServicesStatusEx(scm, windows.SC_ENUM_PROCESS_INFO, windows.SERVICE_WIN32, windows.SERVICE_ACTIVE, p, uint32(len(buf)), &needed, &count, nil, nil)
		if err == nil {
			break
		}
		if !errors.Is(err, windows.ERROR_MORE_DATA) || needed == 0 {
			return nil
		}
		buf = make([]byte, len(buf)+int(needed))
	}
	if count == 0 {
		return nil
	}
	var entries []Entry
	for _, service := range unsafe.Slice((*windows.ENUM_SERVICE_STATUS_PROCESS)(unsafe.Pointer(&buf[0])), count) {
		pid := service.ServiceStatusProcess.ProcessId
		if pid == 0 {
			continue
		}
		dllPath, err := serviceDLLPath(windows.UTF16PtrToString(service.ServiceName))
		if err != nil || dllPath == "" {
			continue
		}
		entries = append(entries, Entry{
			PID:     pid,
			Name:    filepath.Base(dllPath),
			Path:    dllPath,
			Service: windows.UTF16PtrToString(service.DisplayName),
		})
	}
	return entries
}

func serviceDLLPath(serviceName string) (string, error) {
	serviceKeyPath := `SYSTEM\CurrentControlSet\Services\` + serviceName
	var lastErr error
	for _, keyPath := range []string{serviceKeyPath + `\Parameters`, serviceKeyPath} {
		key, err := registry.OpenKey(registry.LOCAL_MACHINE, keyPath, registry.QUERY_VALUE)
		if err != nil {
			lastErr = err
			continue
		}
		value, valueType, err := key.GetStringValue("ServiceDll")
		key.Close()
		if err != nil {
			lastErr = err
			continue
		}
		if valueType == registry.EXPAND_SZ {
			return registry.ExpandString(value)
		}
		return value, nil
	}
	return "", lastErr
}
