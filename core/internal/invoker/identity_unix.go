//go:build linux || darwin

package invoker

import "os"

type identity struct{ ruid, euid, rgid, egid int }

func boundary() bool {
	return os.Getuid() != os.Geteuid() || os.Getgid() != os.Getegid()
}

// Read per call rather than cached: startup may still change the process's identity, e.g. by shedding root.
func invokingUser() (identity, bool) {
	id := identity{os.Getuid(), os.Geteuid(), os.Getgid(), os.Getegid()}
	return id, id.ruid != id.euid || id.rgid != id.egid
}
