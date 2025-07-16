package boxdns

import (
	"Core/internal/boxdns/winipcfg"
	"fmt"
	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/control"
	logger2 "github.com/sagernet/sing/common/logger"
	"log"
)

func init() {
	logger := logger2.NOP()
	updMonitor, err := tun.NewNetworkUpdateMonitor(logger)
	if err != nil {
		fmt.Println("Could not create NetworkUpdateMonitor")
		return
	}
	monitor, err := tun.NewDefaultInterfaceMonitor(updMonitor, logger, tun.DefaultInterfaceMonitorOptions{
		InterfaceFinder: control.NewDefaultInterfaceFinder(),
	})
	if err != nil {
		fmt.Println("Could not create DefaultInterfaceMonitor")
		return
	}
	DnsManagerInstance = &DnsManager{Monitor: monitor}
	monitor.RegisterCallback(DnsManagerInstance.HandleUnderlyingDNS)
	monitor.RegisterCallback(DnsManagerInstance.HandleSystemDNS)
	if err = updMonitor.Start(); err != nil {
		fmt.Println("Could not start updMonitor")
		return
	}
	if err = monitor.Start(); err != nil {
		fmt.Println("Could not start monitor")
		return
	}
}

var DnsManagerInstance *DnsManager

type DnsManager struct {
	Monitor tun.DefaultInterfaceMonitor
	lastIfc *control.Interface
}

func (d *DnsManager) HandleUnderlyingDNS(ifc *control.Interface, flag int) {
	if d == nil {
		log.Println("No DnsManager, you may need to restart Throne")
		return
	}
	if ifc == nil {
		log.Println("Default interface is nil!")
		return
	}
	luid, err := winipcfg.LUIDFromIndex(uint32(ifc.Index))
	if err != nil {
		log.Println("Could not get LUID from index")
		return
	}
	dns := getFirstDNS(luid)
	if dns != "" && dns != underlyingDNS {
		underlyingDNS = dns
		log.Println("underlyingDNS:", underlyingDNS)
	}
}

func getFirstDNS(luid winipcfg.LUID) string {
	dns, err := getNameServersForInterface(luid)
	if err != nil || len(dns) == 0 {
		return ""
	}
	return dns[0]
}

func getNameServersForInterface(luid winipcfg.LUID) ([]string, error) {
	nameservers := make([]string, 0, 4)
	nsAddrs, err := luid.DNS()
	if err != nil {
		return nil, err
	}
	isSystemDNSAltered := false
	for _, server := range nsAddrs {
		if server.String() == setMarkAddr || server.String() == dhcpMarkAddr {
			isSystemDNSAltered = true
		}
		if server.IsValid() && server.String() != setMarkAddr && server.String() != dhcpMarkAddr {
			nameservers = append(nameservers, server.String())
		}
	}
	if isSystemDNSAltered && len(nameservers) > 0 && nameservers[0] == "127.0.0.1" {
		nameservers = nameservers[1:]
	}

	if len(nameservers) == 0 {
		log.Println("getNameServersForInterface: no nameservers found")
	}

	return nameservers, nil
}
