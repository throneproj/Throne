//go:build windows

package parentcheck

import (
	"fmt"
    "strings"
	"golang.org/x/sys/windows"
)

func getDrivePath(devicePath string) string {
	if len(devicePath) >= 3 && devicePath[1] == ':' && devicePath[2] == '\\' {
		return devicePath
	}

	size, err := windows.GetLogicalDriveStrings(0, nil)
	if err != nil || size == 0 {
		return devicePath
	}

	buf := make([]uint16, size)
	_, err = windows.GetLogicalDriveStrings(size, &buf[0])
	if err != nil {
		return devicePath
	}

	var drives []string
	start := 0
	for i := 0; i < len(buf); i++ {
		if buf[i] == 0 {
			if i == start {
				break
			}
			drive := windows.UTF16ToString(buf[start:i])
			drive = strings.TrimSuffix(drive, `\`)
			if drive != "" {
				drives = append(drives, drive)
			}
			start = i + 1
		}
	}

	for _, drive := range drives {
		targetBuf := make([]uint16, windows.MAX_PATH)
		drivePtr, _ := windows.UTF16PtrFromString(drive)

		targetSize, err := windows.QueryDosDevice(drivePtr, &targetBuf[0], uint32(len(targetBuf)))
		if err != nil || targetSize == 0 {
			continue
		}

		targetDevice := windows.UTF16ToString(targetBuf)

		if strings.HasPrefix(devicePath, targetDevice) {
			return drive + strings.TrimPrefix(devicePath, targetDevice)
		}
	}

	return devicePath
}

func getParentExePath(pid int) (string, error) {
	h, err := windows.OpenProcess(windows.PROCESS_QUERY_LIMITED_INFORMATION, false, uint32(pid))
	if err != nil {
		return "", fmt.Errorf("OpenProcess: %w", err)
	}
	defer windows.CloseHandle(h)

	var size uint32 = 4096
	buf := make([]uint16, size)
	if err := windows.QueryFullProcessImageName(h, 0, &buf[0], &size); err != nil {
		return "", fmt.Errorf("QueryFullProcessImageName: %w", err)
	}
	
	return getDrivePath(windows.UTF16ToString(buf[:size])), nil
}
