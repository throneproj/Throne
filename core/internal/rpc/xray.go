package rpc

import (
	"context"
	"runtime"
	"sync"
	"sync/atomic"
	"time"

	"ThroneCore/internal/boxbox"
	"ThroneCore/internal/xray"
	"ThroneCore/internal/xraydns"

	"github.com/xtls/xray-core/core"
	xinternet "github.com/xtls/xray-core/transport/internet"
)

// On failure the started ones are torn down; on success the caller must close them.
func startXrayFullConfigs(configs []string, prepare func(*core.Instance) error) ([]*core.Instance, error) {
	instances := make([]*core.Instance, 0, len(configs))
	for _, cfg := range configs {
		inst, err := xray.CreateXrayInstance(cfg)
		if err != nil {
			closeXrayInstances(instances)
			return nil, err
		}
		if err := prepare(inst); err != nil {
			_ = inst.Close()
			closeXrayInstances(instances)
			return nil, err
		}
		if err := inst.Start(); err != nil {
			_ = inst.Close()
			closeXrayInstances(instances)
			return nil, err
		}
		instances = append(instances, inst)
	}
	return instances, nil
}

// Slots are cleared before the close so no reader picks up a pointer that is going away.
func closeXray() {
	stateMu.Lock()
	instance, gate := xrayInstance, xrayGate
	fullGates := xrayFullGates
	xrayInstance, xrayGate, xrayFullGates = nil, nil, nil
	stateMu.Unlock()

	if gate != nil {
		gate.Close()
	}
	for _, fullGate := range fullGates {
		fullGate.Close()
	}
	if instance != nil {
		instance.Close()
	}
}

// On failure the gates already opened are torn down; on success closeXray owns them.
func startXrayFullGates(configs []string, idle time.Duration, prepare func(*core.Instance) error) ([]*xray.Gate, error) {
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

	// Alone, not in the fan-out below: the first config loads the geo tables the rest share, so parallelizing races to load them all.
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

// One per start, never a package global: a probe box must not answer for the running instance.
type boxContextHolder struct {
	ctx atomic.Pointer[context.Context]
}

func (h *boxContextHolder) publish(box *boxbox.Box) {
	if box == nil {
		return
	}
	boxCtx := box.Context()
	h.ctx.Store(&boxCtx)
}

func (h *boxContextHolder) get() context.Context {
	if stored := h.ctx.Load(); stored != nil {
		return *stored
	}
	return nil
}

// Must run between core.New and Start; both settings keep the instance off an active TUN.
func xrayPreparer(dnsStrategy string, boxCtx xraydns.BoxProvider) func(*core.Instance) error {
	return func(instance *core.Instance) error {
		instance.SetEgress(currentEgress())
		if dnsStrategy == "" || boxCtx == nil {
			return nil
		}
		// One resolver per instance: it caches the dns-direct transport of the box it was prepared for.
		instance.SetOutboundDNS(xraydns.New(boxCtx), xinternet.ParseDomainStrategy(dnsStrategy))
		return nil
	}
}

func closeXrayInstances(instances []*core.Instance) {
	for _, inst := range instances {
		_ = inst.Close()
	}
}
