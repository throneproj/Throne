package rpc

import (
	"context"
	"encoding/json"
	"errors"
	"os"
	"runtime"
	"sync/atomic"
	"syscall"

	"ThroneCore/gen"
	"ThroneCore/internal/netmon"
	"ThroneCore/internal/scan"

	"github.com/sagernet/sing-box/option"
	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/control"
	M "github.com/sagernet/sing/common/metadata"
)

func currentEgress() (iface string, mark uint32) {
	return defaultInterfaceFinder(), autoRedirectMark.Load()
}

func defaultInterfaceFinder() string {
	ifc := netmon.DefaultInterface()
	if ifc == nil {
		return ""
	}
	return ifc.Name
}

var autoRedirectMark atomic.Uint32

// The interface choice of sing-box's auto_detect_interface: the running box's own, else the same rule on the core's monitor.
type scanEgress struct{}

func (scanEgress) Control() (control.Func, error) {
	box := currentBox()
	if netmon.DefaultInterface() == nil {
		if box != nil {
			return nil, scan.ErrNoEgress
		}
		return nil, nil
	}
	var bind control.Func
	if box != nil {
		bind = box.Network().AutoDetectInterfaceFunc()
	}
	if bind == nil {
		bind = coreAutoDetectInterfaceFunc()
	}
	if bind == nil {
		return nil, nil
	}
	controlFunc := control.Func(func(network, address string, conn syscall.RawConn) error {
		err := bind(network, address, conn)
		// Linux before 5.7 refuses SO_BINDTODEVICE without CAP_NET_RAW; with no box there is no Tun to steer around.
		if err != nil && currentBox() == nil && errors.Is(err, os.ErrPermission) {
			return nil
		}
		return err
	})
	if mark := autoRedirectMark.Load(); mark != 0 {
		controlFunc = control.Append(controlFunc, control.RoutingMark(mark))
	}
	return controlFunc, nil
}

// route.NetworkManager.AutoDetectInterfaceFunc over the core's always-on monitor; no box means no Tun of ours to skip.
func coreAutoDetectInterfaceFunc() control.Func {
	monitor, finder := netmon.Monitor(), netmon.Finder()
	if monitor == nil || finder == nil {
		return nil
	}
	return control.BindToInterfaceFunc(finder, func(network, address string) (string, int, error) {
		if remote := M.ParseSocksaddr(address).Addr; remote.IsValid() {
			if iif, err := finder.ByAddr(remote); err == nil {
				return iif.Name, iif.Index, nil
			}
		}
		ifc := monitor.DefaultInterface()
		if ifc == nil {
			return "", -1, tun.ErrNoRoute
		}
		return ifc.Name, ifc.Index, nil
	})
}

// Latches any default-interface loss until stop: the probe box's auto-detect then fails every dial.
func watchScanEgress() (lost func() bool, stop func()) {
	monitor := netmon.Monitor()
	if monitor == nil {
		return func() bool { return false }, func() {}
	}
	var dropped atomic.Bool
	element := monitor.RegisterCallback(func(ifc *control.Interface, _ int) {
		if ifc == nil {
			dropped.Store(true)
		}
	})
	lost = func() bool {
		return dropped.Load() || monitor.DefaultInterface() == nil
	}
	return lost, func() { monitor.UnregisterCallback(element) }
}

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

// Before the first request: the handlers read the monitor, and every Xray instance follows its default interface.
func startEgressMonitor() {
	netmon.Start()
	monitor := netmon.Monitor()
	if monitor == nil {
		return
	}
	monitor.RegisterCallback(func(ifc *control.Interface, _ int) {
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
	ifc := netmon.DefaultInterface()
	if ifc == nil {
		return nil, errors.New("no default interface")
	}
	return &gen.GetDefaultInterfaceResponse{
		Name:  To(ifc.Name),
		Index: To(int32(ifc.Index)),
	}, nil
}
