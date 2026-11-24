//go:build !windows

package rpc

import "net/netip"

// Only Windows lists resolvers per interface here; darwin's OS configuration is already the default interface's.
func interfaceResolvers(int) []netip.Addr {
	return nil
}
