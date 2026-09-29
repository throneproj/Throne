//go:build !windows

package rpc

import (
	"io/fs"
	"os"
	"path/filepath"
)

// A setuid-root core would otherwise leave the tree root-owned and unwritable to the GUI.
func adoptExtracted(root string) error {
	if os.Geteuid() != 0 {
		return nil
	}
	ruid, rgid := os.Getuid(), os.Getgid()
	if ruid == 0 {
		return nil
	}
	return filepath.WalkDir(root, func(path string, _ fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		return os.Lchown(path, ruid, rgid)
	})
}
