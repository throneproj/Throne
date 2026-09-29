//go:build !windows

package boxdns

import "github.com/sagernet/sing/common/control"

// No-op here (darwin goes through sysdns.SetSystemDNS); still registered so the interface monitor behaves the same everywhere.
func (d *DnsManager) HandleSystemDNS(ifc *control.Interface, flag int) {}
