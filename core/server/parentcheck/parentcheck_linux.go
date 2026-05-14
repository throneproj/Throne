//go:build linux

package parentcheck

import (
	"fmt"
	"os"
	"strings"
)

func getParentExePath(pid int) (string, error) {
	path, err := os.Readlink(fmt.Sprintf("/proc/%d/exe", pid))
	if err != nil {
		return "", err
	}
	// Linux appends " (deleted)" when the binary has been replaced on disk
	return strings.TrimSuffix(path, " (deleted)"), nil
}
