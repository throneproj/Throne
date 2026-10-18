package capcheck

import "os"

func Report() Result {
	root := os.Geteuid() == 0
	result := Result{KillSwitchPrivilege: flag(root)}
	if !root {
		result.Detail = "the kill switch needs the core to run as root (a setuid-root core)"
	}
	return result
}

func nftablesUsable() (*bool, string) {
	return nil, ""
}
