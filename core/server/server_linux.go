//go:build linux

package main

import (
	"ThroneCore/gen"
	"bufio"
	"context"
	"errors"
	"net"
	"os"
	"strconv"
	"strings"
)

func isVirtualDefaultInterface(name string) bool {
	name = strings.ToLower(name)
	if name == "lo" || name == "throne-tun" {
		return true
	}
	virtualPrefixes := []string{
		"tun", "tap", "utun", "wg", "tailscale", "docker", "br-",
		"veth", "virbr", "zt", "nebula", "ham", "cni", "podman",
	}
	for _, prefix := range virtualPrefixes {
		if strings.HasPrefix(name, prefix) {
			return true
		}
	}
	return false
}

func interfaceIsUsable(name string) bool {
	ifc, err := net.InterfaceByName(name)
	if err != nil {
		return false
	}
	return ifc.Flags&net.FlagUp != 0 && ifc.Flags&net.FlagLoopback == 0
}

// GetDefaultInterface reports the physical default-route interface on Linux.
// When Throne's TUN is active it may own the lowest-metric default route, so we
// skip loopback/TUN-style interfaces and use the best remaining default route.
func (s *server) GetDefaultInterface(ctx context.Context, in *gen.EmptyReq) (*gen.GetDefaultInterfaceResponse, error) {
	file, err := os.Open("/proc/net/route")
	if err != nil {
		return nil, err
	}
	defer file.Close()

	type candidate struct {
		name   string
		metric int
	}
	var best *candidate

	scanner := bufio.NewScanner(file)
	if scanner.Scan() {
		// header
	}
	for scanner.Scan() {
		fields := strings.Fields(scanner.Text())
		if len(fields) < 8 || fields[1] != "00000000" {
			continue
		}
		name := fields[0]
		if isVirtualDefaultInterface(name) || !interfaceIsUsable(name) {
			continue
		}
		metric, err := strconv.Atoi(fields[6])
		if err != nil {
			metric = 0
		}
		if best == nil || metric < best.metric {
			best = &candidate{name: name, metric: metric}
		}
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}
	if best == nil {
		return nil, errors.New("no physical default interface")
	}

	ifc, err := net.InterfaceByName(best.name)
	if err != nil {
		return nil, err
	}
	return &gen.GetDefaultInterfaceResponse{
		Name:  To(ifc.Name),
		Index: To(int32(ifc.Index)),
	}, nil
}
