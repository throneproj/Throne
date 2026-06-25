//go:build !windows

package main

import (
	"ThroneCore/gen"
	"ThroneCore/internal/boxdns"
	"context"
	"errors"
)

// GetDefaultInterface returns the interface selected by the core's default
// interface monitor.
func (s *server) GetDefaultInterface(ctx context.Context, in *gen.EmptyReq) (*gen.GetDefaultInterfaceResponse, error) {
	if boxdns.DnsManagerInstance == nil || boxdns.DnsManagerInstance.Monitor == nil {
		return nil, errors.New("interface monitor not available")
	}
	ifc := boxdns.DnsManagerInstance.Monitor.DefaultInterface()
	if ifc == nil {
		return nil, errors.New("no default interface")
	}
	return &gen.GetDefaultInterfaceResponse{
		Name:  To(ifc.Name),
		Index: To(int32(ifc.Index)),
	}, nil
}
