package scan

import (
	"context"
	"net/netip"
	"time"

	"ThroneCore/internal/warpprobe"
)

const (
	defaultWARPAttempts = 3
	maxWARPAttempts     = 10
)

type WARPOptions struct {
	Enabled    bool
	ConfigJSON string
	Attempts   int
	Timeout    time.Duration
}

type warpPlan struct {
	prober   *warpprobe.Prober
	attempts int
	timeout  time.Duration
}

func newWARPPlan(ctx context.Context, options WARPOptions) (*warpPlan, error) {
	prober, err := warpprobe.New(ctx, options.ConfigJSON)
	if err != nil {
		return nil, err
	}
	attempts := options.Attempts
	if attempts <= 0 {
		attempts = defaultWARPAttempts
	}
	return &warpPlan{
		prober:   prober,
		attempts: min(attempts, maxWARPAttempts),
		timeout:  clampTimeout(options.Timeout, time.Second),
	}, nil
}

func (r *probeRun) warpTarget(ctx context.Context, target Target) Result {
	plan := r.plan.warp
	result := Result{Addr: target.Addr, Port: target.Port}
	port := target.Port
	if port == 0 {
		port = plan.prober.DefaultPort()
	}
	result.ProbePort = port
	display := DisplayTarget(target.Addr, port)
	r.session.emit(EventStart, PhaseWARP, display, 0, "")
	controlFunc, err := r.egress.Control()
	if err != nil {
		return r.fail(ctx, &result, PhaseWARP, display, err)
	}
	probeCtx, cancel := context.WithTimeout(ctx, time.Duration(plan.attempts)*plan.timeout+time.Second)
	defer cancel()
	probed := plan.prober.Probe(probeCtx, netip.AddrPortFrom(target.Addr.Unmap(), port), controlFunc, plan.attempts, plan.timeout)
	result.Sent, result.Answered = int32(probed.Sent), int32(probed.Answered)
	if probed.Answered == 0 {
		err = probed.Err
		if err == nil {
			err = contextOr(probeCtx, context.DeadlineExceeded)
		}
		return r.fail(ctx, &result, PhaseWARP, display, err)
	}
	result.HandshakeMs = int32(max(probed.Median.Milliseconds(), 1))
	r.session.emit(EventOK, PhaseWARP, display, probed.Median, "")
	result.Passed = true
	return result
}
