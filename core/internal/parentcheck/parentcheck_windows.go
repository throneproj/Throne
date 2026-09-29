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

// Not filepath.EvalSymlinks: Go 1.23+ stopped reporting NTFS junctions as ModeSymlink, so it bails with ENOTDIR on one (e.g. Scoop's "current").
func resolveFinalPath(path string) string {
	pathPtr, err := windows.UTF16PtrFromString(path)
	if err != nil {
		return path
	}
	h, err := windows.CreateFile(
		pathPtr,
		0, // no access requested; a handle is enough to query the final path
		windows.FILE_SHARE_READ|windows.FILE_SHARE_WRITE|windows.FILE_SHARE_DELETE,
		nil,
		windows.OPEN_EXISTING,
		windows.FILE_FLAG_BACKUP_SEMANTICS, // required for directories; harmless for files
		0,
	)
	if err != nil {
		return path
	}
	defer windows.CloseHandle(h)

	// flags=0 → FILE_NAME_NORMALIZED | VOLUME_NAME_DOS (drive-letter form).
	buf := make([]uint16, windows.MAX_PATH)
	n, err := windows.GetFinalPathNameByHandle(h, &buf[0], uint32(len(buf)), 0)
	if err != nil {
		return path
	}
	if n > uint32(len(buf)) {
		// First call's return is the required size including the NUL.
		buf = make([]uint16, n)
		n, err = windows.GetFinalPathNameByHandle(h, &buf[0], n, 0)
		if err != nil || n == 0 || n > uint32(len(buf)) {
			return path
		}
	}
	resolved := windows.UTF16ToString(buf[:n])
	// GetFinalPathNameByHandle always returns the extended-length form.
	if strings.HasPrefix(resolved, `\\?\UNC\`) {
		return `\\` + resolved[len(`\\?\UNC\`):]
	}
	return strings.TrimPrefix(resolved, `\\?\`)
}
