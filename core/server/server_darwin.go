//go:build darwin

package main

import (
	"ThroneCore/gen"
	"context"
	"errors"
	"net"
	"os/exec"
	"strings"
)

// GetDefaultInterface reports the default-route interface on macOS.
func (s *server) GetDefaultInterface(ctx context.Context, in *gen.EmptyReq) (*gen.GetDefaultInterfaceResponse, error) {
	out, err := exec.CommandContext(ctx, "/sbin/route", "-n", "get", "default").Output()
	if err != nil {
		return nil, err
	}
	for _, line := range strings.Split(string(out), "\n") {
		line = strings.TrimSpace(line)
		if !strings.HasPrefix(line, "interface:") {
			continue
		}
		name := strings.TrimSpace(strings.TrimPrefix(line, "interface:"))
		if name == "" {
			break
		}
		if name == "lo0" || strings.HasPrefix(name, "utun") || strings.HasPrefix(name, "tun") || strings.HasPrefix(name, "tap") {
			break
		}
		ifc, err := net.InterfaceByName(name)
		if err != nil {
			return nil, err
		}
		return &gen.GetDefaultInterfaceResponse{
			Name:  To(ifc.Name),
			Index: To(int32(ifc.Index)),
		}, nil
	}
	return nil, errors.New("no default interface")
}
