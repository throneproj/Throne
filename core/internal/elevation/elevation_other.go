//go:build !windows

package elevation

import "log"

func Requested() bool { return false }

func IsAttached() bool { return false }

func RunStub(Mode) {
	log.Fatal("elevation: the elevated core stub exists only on Windows")
}

func RunAttach([]string, func(Attached)) {
	log.Fatal("elevation: --attach exists only on Windows")
}
