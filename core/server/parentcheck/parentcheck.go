//go:build !debug

package parentcheck

import (
	"log"
	"os"
	"path/filepath"
	"runtime"
	"strings"
)

func CheckParentProcess() {
	ppid := os.Getppid()

	parentPath, err := getParentExePath(ppid)
	if err != nil {
		log.Fatalf("parent check: cannot read parent executable: %v", err)
	}

	selfPath, err := os.Executable()
	if err != nil {
		log.Fatalf("parent check: cannot read own executable: %v", err)
	}
	if resolved, err := filepath.EvalSymlinks(selfPath); err == nil {
		selfPath = resolved
	}

	selfDir := filepath.Dir(selfPath)
	parentDir := filepath.Dir(parentPath)
	parentBase := filepath.Base(parentPath)

	if runtime.GOOS == "windows" {
		if !strings.EqualFold(parentDir, selfDir) || !strings.EqualFold(parentBase, "Throne.exe") {
			log.Fatalf("parent check failed: unexpected parent %q", parentPath)
		}
		return
	}

	if parentDir != selfDir || parentBase != "Throne" {
		log.Fatalf("parent check failed: unexpected parent %q", parentPath)
	}
}
