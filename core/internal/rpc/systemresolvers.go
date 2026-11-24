package rpc

import (
	"context"
	"net"
	"net/netip"
	"slices"

	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/netmon"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/systemdns"
	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/service"
)

// The physical network's resolvers: the egress interface's own, else the OS configuration; never our own Tun's.
func systemResolvers() []netip.Addr {
	box := currentBox()
	var servers []netip.Addr
	if ifc := netmon.DefaultInterface(); ifc != nil {
		servers = interfaceResolvers(ifc.Index)
	}
	if len(servers) == 0 {
		servers = configuredResolvers(box)
	}
	own := ownTunNetworks(box)
	resolvers := make([]netip.Addr, 0, len(servers))
	for _, server := range servers {
		server = server.Unmap()
		if !usableResolver(server) || slices.Contains(resolvers, server) ||
			slices.ContainsFunc(own, func(it *net.IPNet) bool { return it.Contains(server.AsSlice()) }) {
			continue
		}
		resolvers = append(resolvers, server)
	}
	return resolvers
}

// systemdns reads only these two from its network manager, and with no box running there is none to lend it.
type monitorNetworkManager struct {
	adapter.NetworkManager
}

func (monitorNetworkManager) InterfaceMonitor() tun.DefaultInterfaceMonitor {
	return netmon.Monitor()
}

func (monitorNetworkManager) DefaultNetworkInterface() *adapter.NetworkInterface {
	return nil
}

func configuredResolvers(box *boxmain.Box) []netip.Addr {
	var ctx context.Context
	if box != nil {
		ctx = box.Context()
	} else {
		ctx = service.ContextWith[adapter.NetworkManager](context.Background(), monitorNetworkManager{})
	}
	source, err := systemdns.NewSource(ctx)
	if err != nil {
		return nil
	}
	defer source.Close()
	var servers []netip.Addr
	for _, server := range source.Servers() {
		if server.IsIP() {
			servers = append(servers, server.Addr)
		}
	}
	return servers
}

func ownTunNetworks(box *boxmain.Box) []*net.IPNet {
	if box == nil {
		return nil
	}
	monitor := box.Network().InterfaceMonitor()
	if monitor == nil {
		return nil
	}
	var networks []*net.IPNet
	for _, name := range monitor.MyInterfaces() {
		netInterface, err := net.InterfaceByName(name)
		if err != nil {
			continue
		}
		addresses, err := netInterface.Addrs()
		if err != nil {
			continue
		}
		for _, address := range addresses {
			if ipNet, isIPNet := address.(*net.IPNet); isIPNet {
				networks = append(networks, ipNet)
			}
		}
	}
	return networks
}

func usableResolver(addr netip.Addr) bool {
	if !addr.IsValid() || addr.IsUnspecified() || addr.IsLoopback() || addr.IsLinkLocalUnicast() || addr.IsMulticast() || addr.Zone() != "" {
		return false
	}
	// Windows lists fec0::/10 site-local resolvers when no IPv6 one is configured.
	return !addr.Is6() || addr.As16()[0] != 0xfe || addr.As16()[1]&0xc0 != 0xc0
}
