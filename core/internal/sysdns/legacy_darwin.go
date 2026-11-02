package sysdns

import (
	"encoding/json"
	"log"
	"net/netip"
	"os"
	"slices"
	"strings"

	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/shell"
)

// 1.4.0-beta.4 and beta.5 pointed the default service's DNS at a lo0 listener and recorded how to undo it.
const (
	legacyStateDir  = "/var/db/Throne"
	legacyStateFile = legacyStateDir + "/system-dns.json"
	legacyLockFile  = legacyStateDir + "/system-dns.lock"
	legacyAddress   = "127.0.0.53"
)

// RestoreLegacy undoes what a beta.4 or beta.5 core that died left in the system DNS.
func RestoreLegacy() {
	if os.Geteuid() != 0 {
		return
	}
	data, err := os.ReadFile(legacyStateFile)
	if err != nil {
		return
	}
	var stored struct {
		Alias    bool                `json:"alias,omitempty"`
		Services map[string][]string `json:"services,omitempty"`
	}
	if err = json.Unmarshal(data, &stored); err != nil {
		log.Println("system DNS: read the legacy record:", err)
	}
	for service, previous := range stored.Services {
		// A service that no longer points at the listener was changed by the user since.
		if current, readErr := legacyDNSServers(service); readErr != nil || !slices.Equal(current, []string{legacyAddress}) {
			continue
		}
		args := append([]string{"-setdnsservers", service}, previous...)
		if len(previous) == 0 {
			args = append(args, "Empty")
		}
		output, setErr := shell.Exec("/usr/sbin/networksetup", args...).Read()
		if setErr = withOutput(setErr, output); setErr != nil {
			log.Println("system DNS: restore", service+":", setErr)
		}
	}
	if stored.Alias {
		_ = shell.Exec("/sbin/ifconfig", "lo0", "-alias", legacyAddress).Run()
	}
	_ = os.Remove(legacyStateFile)
	_ = os.Remove(legacyLockFile)
	_ = os.Remove(legacyStateDir)
}

func legacyDNSServers(service string) ([]string, error) {
	output, err := shell.Exec("/usr/sbin/networksetup", "-getdnsservers", service).ReadOutput()
	if err != nil {
		return nil, err
	}
	var servers []string
	for line := range strings.Lines(output) {
		line = strings.TrimSpace(line)
		if _, parseErr := netip.ParseAddr(line); parseErr == nil {
			servers = append(servers, line)
		} else if !strings.HasPrefix(line, "There aren't any DNS Servers") {
			return nil, E.New("read the DNS servers of ", service, ": ", output)
		}
	}
	return servers, nil
}
