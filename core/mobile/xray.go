package mobile

import (
	"sync"
	"sync/atomic"
	"syscall"
	"time"

	xinternet "github.com/xtls/xray-core/transport/internet"
)

type protectorState struct {
	platform PlatformInterface
	enabled  bool
}

var (
	protector            atomic.Pointer[protectorState]
	dialerControllerOnce sync.Once
)

// The controller is registered once per process and reads the live platform per dial, so every
// Xray socket (TCP, UDP, Happy-Eyeballs, DNS) gets VpnService.protect for whichever instance is up.
func installProtector(platform PlatformInterface) {
	if platform == nil {
		return
	}
	protector.Store(&protectorState{platform: platform, enabled: platform.UsePlatformAutoDetectInterfaceControl()})
	dialerControllerOnce.Do(func() {
		_ = xinternet.RegisterDialerController(protectDial)
	})
}

func releaseProtector(platform PlatformInterface) {
	if state := protector.Load(); state != nil && state.platform == platform {
		protector.CompareAndSwap(state, nil)
	}
}

func protectDial(_, _ string, conn syscall.RawConn) error {
	state := protector.Load()
	if state == nil || !state.enabled {
		return nil
	}
	var protectErr error
	if err := conn.Control(func(fd uintptr) {
		protectErr = state.platform.AutoDetectInterfaceControl(int32(fd))
	}); err != nil {
		return err
	}
	return protectErr
}

// One network switch reaches Recycle twice: through the box's interface monitor, and through the app's
// reset-on-network-change calling ResetNetwork right after it reports the new interface.
const xrayRecycleCooldown = time.Second
