// Package capcheck reports what the core's own privileges allow: a setuid or file-caps core differs from Throne itself.
package capcheck

import (
	"encoding/json"
	"io"
	"os"
)

// Result is what IsPrivileged forwards; a nil field is one this OS cannot tell.
type Result struct {
	TunDevice           *bool  `json:"tun_device"`
	NetAdmin            *bool  `json:"net_admin"`
	KillSwitchPrivilege *bool  `json:"kill_switch_privilege"`
	Detail              string `json:"detail"`
}

// Check is the line ThroneCore --check prints.
type Check struct {
	Result
	Euid          int    `json:"euid"`
	Ruid          int    `json:"ruid"`
	Setuid        bool   `json:"setuid"`
	Kernel        string `json:"kernel"`
	Nftables      *bool  `json:"nftables"`
	NftablesError string `json:"nftables_error"`
}

func Run() Check {
	check := Check{
		Result: Report(),
		Euid:   os.Geteuid(),
		Ruid:   os.Getuid(),
		Setuid: setuidBinary(),
		Kernel: kernelRelease(),
	}
	check.Nftables, check.NftablesError = nftablesUsable()
	return check
}

// Print writes the --check line: one JSON object and a newline, in a single write.
func Print(w io.Writer) error {
	encoder := json.NewEncoder(w)
	encoder.SetEscapeHTML(false)
	return encoder.Encode(Run())
}

func setuidBinary() bool {
	path, err := os.Executable()
	if err != nil {
		return false
	}
	info, err := os.Stat(path)
	return err == nil && info.Mode()&os.ModeSetuid != 0
}

func flag(value bool) *bool {
	return &value
}
