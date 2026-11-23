package xray

import (
	"context"
	"fmt"
	"log"
	"runtime"
	"sync"
	"sync/atomic"
	"time"

	"ThroneCore/internal/xraydns"

	"github.com/xtls/xray-core/core"
	xinternet "github.com/xtls/xray-core/transport/internet"
)

// One per box, never shared: a probe box must not answer for the running instance.
type BoxContext struct {
	ctx atomic.Pointer[context.Context]
}

func (b *BoxContext) Publish(ctx context.Context) {
	b.ctx.Store(&ctx)
}

func (b *BoxContext) Get() context.Context {
	if stored := b.ctx.Load(); stored != nil {
		return *stored
	}
	return nil
}

// Must run between core.New and Start; egress (nil skips it) and the box's DNS both keep the instance off an active TUN.
func Preparer(egress func() (string, uint32), dnsStrategy string, boxCtx xraydns.BoxProvider) func(*core.Instance) error {
	return func(instance *core.Instance) error {
		if egress != nil {
			instance.SetEgress(egress())
		}
		if dnsStrategy == "" || boxCtx == nil {
			return nil
		}
		// One resolver per instance: it caches the dns-direct transport of the box it was prepared for.
		instance.SetOutboundDNS(xraydns.New(boxCtx), xinternet.ParseDomainStrategy(dnsStrategy))
		return nil
	}
}

type StackOptions struct {
	NeedXray bool
	Config   string
	// Lazy keeps Config's instance cold behind a gate; Idle 0 keeps it resident once started.
	Lazy bool
	Idle time.Duration
	// One gate per opaque full config; never merged into Config's instance.
	FullConfigs []string
	FullIdle    time.Duration
	DNSStrategy string
	Egress      func() (string, uint32)
}

type Stack struct {
	boxCtx BoxContext

	mu         sync.Mutex
	closed     bool
	recycledAt time.Time

	// What the eager instance was started from, to replace it in place.
	config  string
	prepare func(*core.Instance) error

	// At most one is set: instance when eager, gate when the profile asked it to stay cold.
	instance  *core.Instance
	gate      *Gate
	fullGates []*Gate
}

// Each sidecar reaches Instances as soon as it is up; a failed Start leaves the stack closed.
func (s *Stack) Start(options StackOptions) error {
	prepare := Preparer(options.Egress, options.DNSStrategy, s.boxCtx.Get)
	if options.NeedXray {
		if options.Lazy {
			gate, err := StartGate(options.Config, options.Idle, prepare)
			if err != nil {
				s.Close()
				return err
			}
			s.mu.Lock()
			s.gate = gate
			s.mu.Unlock()
		} else {
			instance, err := StartInstance(options.Config, prepare)
			if err != nil {
				s.Close()
				return err
			}
			s.mu.Lock()
			s.instance, s.config, s.prepare = instance, options.Config, prepare
			s.mu.Unlock()
		}
	}
	gates, err := startGates(options.FullConfigs, options.FullIdle, prepare)
	if err != nil {
		s.Close()
		return err
	}
	s.mu.Lock()
	s.fullGates = gates
	s.mu.Unlock()
	return nil
}

// Between the box's creation and its Start: a remote rule-set fetched during Start may already dial through Xray.
func (s *Stack) PublishBoxContext(ctx context.Context) {
	s.boxCtx.Publish(ctx)
}

// Shorter than the configs: a gated instance is absent between activations.
func (s *Stack) Instances() []*core.Instance {
	if s == nil {
		return nil
	}
	s.mu.Lock()
	if s.closed {
		s.mu.Unlock()
		return nil
	}
	instance, gate, fullGates := s.instance, s.gate, s.fullGates
	s.mu.Unlock()

	var instances []*core.Instance
	if instance != nil {
		instances = append(instances, instance)
	} else if gate != nil {
		if live := gate.Instance(); live != nil {
			instances = append(instances, live)
		}
	}
	for _, fullGate := range fullGates {
		if live := fullGate.Instance(); live != nil {
			instances = append(instances, live)
		}
	}
	return instances
}

// Retires every live instance at most once per cooldown: a gate starts a fresh one on demand, the eager one restarts at once as no gate holds its ports.
func (s *Stack) Recycle(cooldown time.Duration) {
	if s == nil {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.closed || time.Since(s.recycledAt) < cooldown {
		return
	}
	if s.gate != nil {
		s.gate.Recycle()
	}
	for _, gate := range s.fullGates {
		gate.Recycle()
	}
	if s.instance != nil {
		instance, err := RestartInstance(s.instance, s.config, s.prepare)
		s.instance = instance
		if err != nil {
			FailureLog(fmt.Sprintf("Xray: cannot restart the instance for a network reset: %v", err))
		} else {
			log.Println("Xray: restarted the instance for a network reset")
		}
	}
	s.recycledAt = time.Now()
}

// Waits out a Recycle in progress; the sidecars are taken out before they close, so Instances never returns a dying one.
func (s *Stack) Close() {
	if s == nil {
		return
	}
	s.mu.Lock()
	if s.closed {
		s.mu.Unlock()
		return
	}
	s.closed = true
	instance, gate, fullGates := s.instance, s.gate, s.fullGates
	s.instance, s.gate, s.fullGates = nil, nil, nil
	s.mu.Unlock()

	if gate != nil {
		gate.Close()
	}
	for _, fullGate := range fullGates {
		fullGate.Close()
	}
	if instance != nil {
		_ = instance.Close()
	}
}

// On failure the gates already opened are closed.
func startGates(configs []string, idle time.Duration, prepare func(*core.Instance) error) ([]*Gate, error) {
	if len(configs) == 0 {
		return nil, nil
	}
	gates := make([]*Gate, len(configs))
	errs := make([]error, len(configs))

	unwind := func() {
		for _, opened := range gates {
			if opened != nil {
				opened.Close()
			}
		}
	}

	// Alone, not in the fan-out below: the first config loads the geo tables the rest share, so parallelizing races to load them all.
	gates[0], errs[0] = StartGate(configs[0], idle, prepare)
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
			gates[i], errs[i] = StartGate(configs[i], idle, prepare)
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
