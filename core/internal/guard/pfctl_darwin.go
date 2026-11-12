package guard

import (
	"regexp"

	"ThroneCore/internal/pfctl"
)

// /etc/pf.conf evaluates the com.apple/* children in name order: "000." runs before Apple's and the bridge's.
const pfAnchorPrefix = pfctl.RootAnchor + "/000.throne-guard."

var pfAnchorIDPattern = regexp.MustCompile(`^[0-9a-f]{8}$`)

func pfLockPath(id string) string {
	return "/var/run/throne-guard." + id + ".lock"
}
