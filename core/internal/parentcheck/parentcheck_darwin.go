//go:build darwin

package parentcheck

/*
#include <libproc.h>
*/
import "C"
import (
	"fmt"
	"path/filepath"
	"unsafe"
)

func getParentExePath(pid int) (string, error) {
	buf := make([]byte, C.PROC_PIDPATHINFO_MAXSIZE)
	ret := C.proc_pidpath(C.int(pid), unsafe.Pointer(&buf[0]), C.uint32_t(len(buf)))
	if ret <= 0 {
		return "", fmt.Errorf("proc_pidpath failed for pid %d", pid)
	}
	return string(buf[:int(ret)]), nil
}

func resolveFinalPath(path string) string {
	if resolved, err := filepath.EvalSymlinks(path); err == nil {
		return resolved
	}
	return path
}
