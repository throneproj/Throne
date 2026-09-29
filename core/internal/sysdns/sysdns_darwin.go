package sysdns

import (
	"strings"

	tun "github.com/sagernet/sing-tun"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/shell"
)

func SetSystemDNS(addr string, interfaceMonitor tun.DefaultInterfaceMonitor) error {
	interfaceName := interfaceMonitor.DefaultInterface().Name
	interfaceDisplayName, err := getInterfaceDisplayName(interfaceName)
	if err != nil {
		return err
	}

	// networksetup commits through configd; only its plist backup needs root, so its "Permission denied" is noise.
	output, err := shell.Exec("/usr/sbin/networksetup", "-setdnsservers", interfaceDisplayName, addr).Read()
	if err != nil {
		if output = strings.TrimSpace(output); output != "" {
			return E.Cause(err, output)
		}
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
