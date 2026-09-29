package rpc

import (
	"context"
	"encoding/json"
	"errors"
	"runtime"
	"sync/atomic"

	"ThroneCore/gen"
	"ThroneCore/internal/boxdns"

	"github.com/sagernet/sing-box/option"
	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/control"
)

func currentEgress() (iface string, mark uint32) {
	return defaultInterfaceFinder(), autoRedirectMark.Load()
}

func defaultInterfaceFinder() string {
	ifc := boxdns.DefaultInterface()
	if ifc == nil {
		return ""
	}
	return ifc.Name
}

var autoRedirectMark atomic.Uint32

func autoRedirectMarkFor(coreConfig []byte) uint32 {
	// auto_redirect, and SO_MARK itself, are Linux-only.
	if runtime.GOOS != "linux" {
		return 0
	}
	return configAutoRedirectMark(coreConfig)
}

func configAutoRedirectMark(coreConfig []byte) uint32 {
	var config struct {
		Inbounds []json.RawMessage `json:"inbounds"`
	}
	if json.Unmarshal(coreConfig, &config) != nil {
		return 0
	}
	for _, raw := range config.Inbounds {
		var inbound struct {
			Type                   string        `json:"type"`
			AutoRedirect           bool          `json:"auto_redirect"`
			AutoRedirectOutputMark option.FwMark `json:"auto_redirect_output_mark"`
		}
		if json.Unmarshal(raw, &inbound) != nil || inbound.Type != "tun" || !inbound.AutoRedirect {
			continue
		}
		if inbound.AutoRedirectOutputMark != 0 {
			return uint32(inbound.AutoRedirectOutputMark)
		}
		return tun.DefaultAutoRedirectOutputMark
	}
	return 0
}

func init() {
	m := boxdns.DnsManagerInstance
	if m == nil || m.Monitor == nil {
		return
	}
	m.Monitor.RegisterCallback(func(ifc *control.Interface, _ int) {
		name := ""
		if ifc != nil {
			name = ifc.Name
		}
		// The callback's interface is fresher than currentEgress would report here; the mark carries over unchanged.
		for _, inst := range liveXrayInstances() {
			inst.SetEgress(name, autoRedirectMark.Load())
		}
	})
}

func (s *server) GetDefaultInterface(ctx context.Context, in *gen.EmptyReq) (*gen.GetDefaultInterfaceResponse, error) {
	ifc := boxdns.DefaultInterface()
	if ifc == nil {
		return nil, errors.New("no default interface")
	}
	return &gen.GetDefaultInterfaceResponse{
		Name:  To(ifc.Name),
		Index: To(int32(ifc.Index)),
	}, nil
}
