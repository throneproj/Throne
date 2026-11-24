package scan

import (
	"context"
	"errors"
	"fmt"
	"math/rand/v2"
	"net/netip"
	"sync"
	"sync/atomic"
	"time"
)

type ProbeRequest struct {
	Spec          Spec
	Cursor        uint64
	MaxTargets    int
	DNS           DNSOptions
	WARP          WARPOptions
	Concurrency   int
	SpawnInterval time.Duration
}

type Result struct {
	Addr           netip.Addr
	Port           uint16
	Passed         bool
	FailedPhase    string
	Error          string
	ProbePort      uint16
	LocalFailure   bool
	DNSMs          int32
	DNSStage       string
	DNSReason      string
	ShortNamesOnly bool
	Sent           int32
	Answered       int32
	HandshakeMs    int32

	cause error
}

type ProbeResponse struct {
	Total          uint64
	NextCursor     uint64
	Aborted        bool
	Results        []Result
	SampledEntries int
	InvalidEntries int
}

const (
	defaultConcurrency = 64
	maxConcurrency     = 1000
	maxChunkTargets    = 1 << 16
	minPhaseTimeout    = 100 * time.Millisecond
	maxPhaseTimeout    = 60 * time.Second
	maxSpawnInterval   = 10 * time.Second
)

type probePlan struct {
	dns  dnsProbe
	warp *warpPlan
}

func clampTimeout(timeout, fallback time.Duration) time.Duration {
	if timeout <= 0 {
		return fallback
	}
	return min(max(timeout, minPhaseTimeout), maxPhaseTimeout)
}

func newProbePlan(ctx context.Context, req ProbeRequest) (*probePlan, error) {
	switch {
	case req.DNS.Enabled && req.WARP.Enabled:
		return nil, errors.New("the dns and warp phases cannot run together")
	case req.DNS.Enabled:
		dns, err := newDNSProbe(req.DNS)
		if err != nil {
			return nil, err
		}
		return &probePlan{dns: dns}, nil
	case req.WARP.Enabled:
		warp, err := newWARPPlan(ctx, req.WARP)
		if err != nil {
			return nil, err
		}
		return &probePlan{warp: warp}, nil
	}
	return &probePlan{}, nil
}

func (p *probePlan) anyPhase() bool {
	return p.dns != nil || p.warp != nil
}

func (p *probePlan) phase() string {
	if p.warp != nil {
		return PhaseWARP
	}
	return PhaseDNS
}

// MaxTargets 0 only counts; a setup error still fills Total.
func Probe(ctx context.Context, session *Session, egress Egress, req ProbeRequest) (ProbeResponse, error) {
	targets := NewTargets(req.Spec)
	total := targets.Total()
	response := ProbeResponse{
		Total:          total,
		NextCursor:     min(req.Cursor, total),
		SampledEntries: targets.SampledEntries,
		InvalidEntries: targets.InvalidEntries,
	}
	if req.MaxTargets <= 0 || req.Cursor >= total {
		return response, nil
	}
	start := req.Cursor
	count := int(min(uint64(min(req.MaxTargets, maxChunkTargets)), total-start))

	plan, err := newProbePlan(ctx, req)
	if err != nil {
		return response, err
	}
	if !plan.anyPhase() {
		response.Results = make([]Result, 0, count)
		for i := range count {
			target := targets.At(start + uint64(i))
			response.Results = append(response.Results, Result{Addr: target.Addr, Port: target.Port, Passed: true})
			session.recordProbe(true)
		}
		response.NextCursor = start + uint64(count)
		return response, nil
	}

	run := &probeRun{plan: plan, session: session, egress: egress}
	finished := make([]bool, count)
	var resultsMu sync.Mutex
	results := make([]Result, 0, count)
	runCtx, cancelRun := context.WithCancel(ctx)
	defer cancelRun()
	var egressLost atomic.Bool

	concurrency := req.Concurrency
	if concurrency <= 0 {
		concurrency = defaultConcurrency
	}
	concurrency = min(concurrency, maxConcurrency)
	spawnInterval := min(max(req.SpawnInterval, 0), maxSpawnInterval)
	slots := make(chan struct{}, concurrency)
	var wg sync.WaitGroup
launch:
	for i := range count {
		if i > 0 && spawnInterval > 0 && !sleepContext(runCtx, jitter(spawnInterval)) {
			break
		}
		select {
		case slots <- struct{}{}:
		case <-runCtx.Done():
			break launch
		}
		if runCtx.Err() != nil {
			<-slots
			break
		}
		target := targets.At(start + uint64(i))
		wg.Add(1)
		go func(index int, target Target) {
			defer wg.Done()
			defer func() { <-slots }()
			result := run.safeTarget(runCtx, target)
			if errors.Is(result.cause, ErrNoEgress) {
				egressLost.Store(true)
				cancelRun()
				return
			}
			// A failure caused by the abort is not a measurement.
			if !result.Passed && runCtx.Err() != nil {
				return
			}
			session.recordProbe(result.Passed)
			resultsMu.Lock()
			finished[index] = true
			results = append(results, result)
			resultsMu.Unlock()
		}(i, target)
	}
	wg.Wait()

	response.Results = results
	response.NextCursor = start + uint64(count)
	for i, done := range finished {
		if !done {
			response.NextCursor = start + uint64(i)
			response.Aborted = true
			break
		}
	}
	if egressLost.Load() {
		return response, ErrEgressLost
	}
	return response, nil
}

func jitter(interval time.Duration) time.Duration {
	return interval*3/4 + time.Duration(rand.Int64N(int64(interval/2)+1))
}

func sleepContext(ctx context.Context, d time.Duration) bool {
	timer := time.NewTimer(d)
	defer timer.Stop()
	select {
	case <-timer.C:
		return true
	case <-ctx.Done():
		return false
	}
}

type probeRun struct {
	plan    *probePlan
	session *Session
	egress  Egress
}

func (r *probeRun) fail(ctx context.Context, result *Result, phase, display string, err error) Result {
	// Sockets bound to a lost egress fail like a dead target; only the egress itself tells them apart.
	if _, egressErr := r.egress.Control(); errors.Is(egressErr, ErrNoEgress) {
		err = egressErr
	}
	result.Passed = false
	result.FailedPhase = phase
	result.Error = errorText(err)
	result.cause = err
	if ctx.Err() == nil && !errors.Is(err, ErrNoEgress) {
		result.LocalFailure = localFailure(r.egress, result.Addr, err)
		r.session.emit(EventFail, phase, display, 0, result.Error)
	}
	return *result
}

// Probe goroutines are outside the dispatcher's recover; a hostile server must not crash the core.
func (r *probeRun) safeTarget(ctx context.Context, target Target) (result Result) {
	defer func() {
		if recovered := recover(); recovered != nil {
			result = Result{Addr: target.Addr, Port: target.Port, FailedPhase: r.plan.phase(), Error: fmt.Sprint("probe panicked: ", recovered)}
		}
	}()
	if r.plan.warp != nil {
		return r.warpTarget(ctx, target)
	}
	return r.dnsTarget(ctx, target)
}
