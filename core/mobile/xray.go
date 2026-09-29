package mobile

import (
	"context"
	"runtime"
	"sync"
	"sync/atomic"
	"syscall"
	"time"

	"ThroneCore/internal/xray"
	"ThroneCore/internal/xraydns"

	xcore "github.com/xtls/xray-core/core"
	xinternet "github.com/xtls/xray-core/transport/internet"
)

// One per box, never a package global: a probe box must not answer for the running instance.
type boxContextHolder struct {
	ctx atomic.Pointer[context.Context]
}

func (h *boxContextHolder) publish(ctx context.Context) {
	h.ctx.Store(&ctx)
}

func (h *boxContextHolder) get() context.Context {
	if stored := h.ctx.Load(); stored != nil {
		return *stored
	}
	return nil
}

// Runs between core.New and Start like rpc.xrayPreparer, minus SetEgress: BindToDevice needs
// CAP_NET_RAW, which an app never has, and an unwired instance takes the plain dial path, which the
// protect controller below covers instead.
func xrayPreparer(dnsStrategy string, boxCtx xraydns.BoxProvider) func(*xcore.Instance) error {
	return func(instance *xcore.Instance) error {
		if dnsStrategy == "" || boxCtx == nil {
			return nil
		}
		instance.SetOutboundDNS(xraydns.New(boxCtx), xinternet.ParseDomainStrategy(dnsStrategy))
		return nil
	}
}

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

type xrayStack struct {
	instance      *xcore.Instance
	gate          *xray.Gate
	fullGates     []*xray.Gate
	fullInstances []*xcore.Instance
}

func (s *xrayStack) close() {
	if s == nil {
		return
	}
	if s.gate != nil {
		s.gate.Close()
	}
	for _, gate := range s.fullGates {
		gate.Close()
	}
	closeXrayInstances(s.fullInstances)
	if s.instance != nil {
		_ = s.instance.Close()
	}
}

func startXrayInstance(config string, prepare func(*xcore.Instance) error) (*xcore.Instance, error) {
	instance, err := xray.CreateXrayInstance(config)
	if err != nil {
		return nil, err
	}
	if err = prepare(instance); err != nil {
		_ = instance.Close()
		return nil, err
	}
	if err = instance.Start(); err != nil {
		_ = instance.Close()
		return nil, err
	}
	return instance, nil
}

func startMainXray(options *StartOptions, prepare func(*xcore.Instance) error) (*xrayStack, error) {
	stack := new(xrayStack)
	if options.NeedXray {
		if options.XrayLazyStart {
			gate, err := xray.StartGate(options.XrayConfig, time.Duration(options.XrayIdleSeconds)*time.Second, prepare)
			if err != nil {
				return nil, err
			}
			stack.gate = gate
		} else {
			instance, err := startXrayInstance(options.XrayConfig, prepare)
			if err != nil {
				return nil, err
			}
			stack.instance = instance
		}
	}
	if len(options.xrayFullConfigs) > 0 {
		gates, err := startXrayFullGates(options.xrayFullConfigs, time.Duration(options.XrayFullIdleSeconds)*time.Second, prepare)
		if err != nil {
			stack.close()
			return nil, err
		}
		stack.fullGates = gates
	}
	return stack, nil
}

// Eager instances for a probe env; on failure the ones already started are torn down.
func startXrayFullConfigs(configs []string, prepare func(*xcore.Instance) error) ([]*xcore.Instance, error) {
	instances := make([]*xcore.Instance, 0, len(configs))
	for _, config := range configs {
		instance, err := startXrayInstance(config, prepare)
		if err != nil {
			closeXrayInstances(instances)
			return nil, err
		}
		instances = append(instances, instance)
	}
	return instances, nil
}

// Copy of rpc.startXrayFullGates: the first config runs alone because it loads the geo tables the
// rest share, and parallelizing it races to load them all.
func startXrayFullGates(configs []string, idle time.Duration, prepare func(*xcore.Instance) error) ([]*xray.Gate, error) {
	if len(configs) == 0 {
		return nil, nil
	}
	gates := make([]*xray.Gate, len(configs))
	errs := make([]error, len(configs))

	unwind := func() {
		for _, opened := range gates {
			if opened != nil {
				opened.Close()
			}
		}
	}

	gates[0], errs[0] = xray.StartGate(configs[0], idle, prepare)
	if errs[0] != nil {
		return nil, errs[0]
	}

	workers := runtime.NumCPU()
	if workers < 1 {
		workers = 1
	}
	slots := make(chan struct{}, workers)
	var wg sync.WaitGroup
	for i := 1; i < len(configs); i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			slots <- struct{}{}
			defer func() { <-slots }()
			gates[i], errs[i] = xray.StartGate(configs[i], idle, prepare)
		}()
	}
	wg.Wait()

	for _, err := range errs {
		if err == nil {
			continue
		}
		unwind()
		return nil, err
	}
	return gates, nil
}

func closeXrayInstances(instances []*xcore.Instance) {
	for _, instance := range instances {
		_ = instance.Close()
	}
}
