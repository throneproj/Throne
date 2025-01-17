package sys

import (
	tun "github.com/sagernet/sing-tun"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/shell"
	"net/netip"
	"strings"
)

func SetSystemDNS(addr string, interfaceMonitor tun.DefaultInterfaceMonitor) error {
	interfaceName := interfaceMonitor.DefaultInterfaceName(netip.IPv4Unspecified())
	interfaceDisplayName, err := getInterfaceDisplayName(interfaceName)
	if err != nil {
		return err
	}

	err = shell.Exec("/usr/sbin/networksetup", "-setdnsservers", interfaceDisplayName, addr).Attach().Run()
	if err != nil {
		return err
	}

	return nil
}

func getInterfaceDisplayName(name string) (string, error) {
	content, err := shell.Exec("/usr/sbin/networksetup", "-listallhardwareports").ReadOutput()
	if err != nil {
		return "", err
	}
	for _, deviceSpan := range strings.Split(string(content), "Ethernet Address") {
		if strings.Contains(deviceSpan, "Device: "+name) {
			substr := "Hardware Port: "
			deviceSpan = deviceSpan[strings.Index(deviceSpan, substr)+len(substr):]
			deviceSpan = deviceSpan[:strings.Index(deviceSpan, "\n")]
			return deviceSpan, nil
		}
	}
	return "", E.New(name, " not found in networksetup -listallhardwareports")
}
