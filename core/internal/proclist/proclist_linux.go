package proclist

import (
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

func List() ([]Entry, error) {
	dirs, err := os.ReadDir("/proc")
	if err != nil {
		return nil, err
	}
	var entries []Entry
	for _, dir := range dirs {
		if !dir.IsDir() {
			continue
		}
		pid, err := strconv.ParseUint(dir.Name(), 10, 32)
		if err != nil {
			continue
		}
		path, err := os.Readlink(filepath.Join("/proc", dir.Name(), "exe"))
		if err != nil || path == "" {
			continue
		}
		// The kernel appends " (deleted)" once the binary is replaced on disk; the rule is for the restarted program.
		path = strings.TrimSuffix(path, " (deleted)")
		entries = append(entries, Entry{PID: uint32(pid), Name: filepath.Base(path), Path: path})
	}
	return entries, nil
}
