//go:build windows

package parentcheck

import (
	"fmt"

	"golang.org/x/sys/windows"
)

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
	return windows.UTF16ToString(buf[:size]), nil
}
