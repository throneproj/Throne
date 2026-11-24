//go:build windows

package rpc

import (
	"net/netip"

	"ThroneCore/internal/winipcfg"
)

func interfaceResolvers(index int) []netip.Addr {
	luid, err := winipcfg.LUIDFromIndex(uint32(index))
	if err != nil {
		return nil
	}
	servers, err := luid.DNS()
	if err != nil {
		return nil
	}
	return servers
}
