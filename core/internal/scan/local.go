package scan

import (
	"net"
	"net/netip"
)

func localFailure(egress Egress, addr netip.Addr, err error) bool {
	return routingError(err) && routeMissing(egress, addr)
}

// A remote unreachable can raise the same errno; a UDP connect sends nothing but repeats the local route lookup.
func routeMissing(egress Egress, addr netip.Addr) bool {
	controlFunc, err := egress.Control()
	if err != nil {
		return true
	}
	network := "udp4"
	if addr.Is6() {
		network = "udp6"
	}
	conn, err := (&net.Dialer{Control: controlFunc}).Dial(network, netip.AddrPortFrom(addr, 9).String())
	if err != nil {
		return true
	}
	_ = conn.Close()
	return false
}
