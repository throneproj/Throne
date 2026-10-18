//go:build !linux && !darwin

package capcheck

func Report() Result {
	return Result{}
}

func nftablesUsable() (*bool, string) {
	return nil, ""
}

func kernelRelease() string {
	return ""
}
