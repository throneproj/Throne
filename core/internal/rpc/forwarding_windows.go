package rpc

import (
	"fmt"
	"sync"
	"time"

	"ThroneCore/internal/winipcfg"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing/common/control"
	"golang.org/x/sys/windows"
)

// The GUI matches this stdout line to raise its warning.
const egressForwardingMarker = "IPv4 forwarding is enabled on the Tun egress adapter: "

type egressForwardingWatch struct {
	defaultInterface func() *control.Interface

	access sync.Mutex
	timer  *time.Timer
	closed bool

	checkAccess sync.Mutex
	reported    string
}

// Windows ignores IP_UNICAST_IF on a forwarding adapter, so the core's own egress falls back into the Tun routes.
func watchEgressForwarding(network adapter.NetworkManager) func() {
	networkMonitor, interfaceMonitor := network.NetworkMonitor(), network.InterfaceMonitor()
	if !network.AutoDetectInterface() || networkMonitor == nil || interfaceMonitor == nil {
		return nil
	}
	w := &egressForwardingWatch{defaultInterface: interfaceMonitor.DefaultInterface}
	w.timer = time.AfterFunc(0, w.check)
	networkElement := networkMonitor.RegisterCallback(w.schedule)
	interfaceElement := interfaceMonitor.RegisterCallback(func(*control.Interface, int) { w.schedule() })
	return func() {
		networkMonitor.UnregisterCallback(networkElement)
		interfaceMonitor.UnregisterCallback(interfaceElement)
		w.access.Lock()
		w.closed = true
		w.timer.Stop()
		w.access.Unlock()
	}
}

// Called on the OS notification thread; the delay lets sing-tun's 1 s default-interface recheck land first.
func (w *egressForwardingWatch) schedule() {
	w.access.Lock()
	defer w.access.Unlock()
	if !w.closed {
		w.timer.Reset(2 * time.Second)
	}
}

func (w *egressForwardingWatch) check() {
	w.checkAccess.Lock()
	defer w.checkAccess.Unlock()
	var name string
	if iface := w.defaultInterface(); iface != nil && forwardingEnabled(iface.Index) {
		name = iface.Name
	}
	w.access.Lock()
	closed := w.closed
	w.access.Unlock()
	if closed || name == w.reported {
		return
	}
	previous := w.reported
	w.reported = name
	if name != "" {
		fmt.Println(egressForwardingMarker + name)
	} else {
		fmt.Println("IPv4 forwarding is disabled again on the Tun egress adapter:", previous)
	}
}

func forwardingEnabled(index int) bool {
	luid, err := winipcfg.LUIDFromIndex(uint32(index))
	if err != nil {
		return false
	}
	row, err := luid.IPInterface(windows.AF_INET)
	return err == nil && row.ForwardingEnabled
}
