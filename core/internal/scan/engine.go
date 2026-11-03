package scan

import (
	"context"
	"errors"
	"fmt"
	"math/rand/v2"
	"net"
	"net/netip"
	"sync"
	"sync/atomic"
	"time"
)

type ICMPOptions struct {
	Enabled bool
	Timeout time.Duration
	Count   int
}

type TCPOptions struct {
	Enabled      bool
	Timeout      time.Duration
	Attempts     int
	FallbackPort int32
}

type HTTPOptions struct {
	Enabled               bool
	TLS                   bool
	ServerName            string
	Host                  string
	Path                  string
	Method                string
	Version               string
	ALPN                  []string
	MinVersion            string
	MaxVersion            string
	Fingerprint           string
	Insecure              bool
	DisableSNI            bool
	Fragment              bool
	FragmentFallbackDelay time.Duration
	RecordFragment        bool
	MixedCaseSNI          bool
	Timeout               time.Duration
	FallbackPort          int32
}

type ProbeRequest struct {
	Spec          Spec
	Cursor        uint64
	MaxTargets    int
	ICMP          ICMPOptions
	TCP           TCPOptions
	HTTP          HTTPOptions
	DNS           DNSOptions
	Concurrency   int
	SpawnInterval time.Duration
}

type Result struct {
	Addr         netip.Addr
	Port         uint16
	Passed       bool
	FailedPhase  string
	Error        string
	ICMPMs       int32
	TCPMs        int32
	TLSMs        int32
	HTTPMs       int32
	HTTPStatus   int
	DNSMs        int32
	MaxQName     int32
	UploadMTU    int32
	DownloadMTU  int32
	DNSStage     string
	ProbePort    uint16
	LocalFailure bool

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
	icmp ICMPOptions
	tcp  TCPOptions
	http *httpPlan
	dns  *dnsPlan
}

func clampTimeout(timeout, fallback time.Duration) time.Duration {
	if timeout <= 0 {
		return fallback
	}
	return min(max(timeout, minPhaseTimeout), maxPhaseTimeout)
}

func phasePort(fallback int32) uint16 {
	if fallback < 1 || fallback > 0xffff {
		return 443
	}
	return uint16(fallback)
}

func newProbePlan(ctx context.Context, req ProbeRequest) (*probePlan, error) {
	if req.DNS.Enabled {
		dns, err := newDNSPlan(req.DNS)
		if err != nil {
			return nil, err
		}
		return &probePlan{dns: dns}, nil
	}
	plan := &probePlan{icmp: req.ICMP, tcp: req.TCP}
	plan.icmp.Timeout = clampTimeout(plan.icmp.Timeout, time.Second)
	plan.icmp.Count = min(max(plan.icmp.Count, 1), 10)
	plan.tcp.Timeout = clampTimeout(plan.tcp.Timeout, 2*time.Second)
	plan.tcp.Attempts = min(max(plan.tcp.Attempts, 1), 10)
	if req.HTTP.Enabled {
		httpPlan, err := newHTTPPlan(ctx, req.HTTP)
		if err != nil {
			return nil, err
		}
		plan.http = httpPlan
	}
	return plan, nil
}

func (p *probePlan) anyPhase() bool {
	return p.dns != nil || p.icmp.Enabled || p.tcp.Enabled || p.http != nil
}

func (p *probePlan) lastPhase() string {
	switch {
	case p.dns != nil:
		return PhaseDNS
	case p.http != nil && p.http.method != "":
		return PhaseHTTP
	case p.http != nil:
		return PhaseTLS
	case p.tcp.Enabled:
		return PhaseTCP
	default:
		return PhaseICMP
	}
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

	if plan.icmp.Enabled {
		if err = icmpCheck(targets, start, count); err != nil {
			return response, err
		}
	}

	run := &probeRun{plan: plan, session: session, egress: egress, pings: make(map[netip.Addr]*pingOutcome)}
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

func icmpCheck(targets *Targets, start uint64, count int) error {
	var checked [2]bool
	for i := range count {
		family := 0
		if targets.At(start + uint64(i)).Addr.Is6() {
			family = 1
		}
		if checked[family] {
			continue
		}
		checked[family] = true
		if err := icmpAvailable(family == 1); err != nil {
			return err
		}
		if checked[0] && checked[1] {
			break
		}
	}
	return nil
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

type pingOutcome struct {
	done    chan struct{}
	latency time.Duration
	err     error
}

type probeRun struct {
	plan    *probePlan
	session *Session
	egress  Egress

	pingsMu sync.Mutex
	pings   map[netip.Addr]*pingOutcome
}

func (r *probeRun) fail(ctx context.Context, result *Result, phase, display string, err error) Result {
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
			result = Result{Addr: target.Addr, Port: target.Port, FailedPhase: r.plan.lastPhase(), Error: fmt.Sprint("probe panicked: ", recovered)}
		}
	}()
	return r.target(ctx, target)
}

func (r *probeRun) target(ctx context.Context, target Target) Result {
	if r.plan.dns != nil {
		return r.dnsTarget(ctx, target)
	}
	result := Result{Addr: target.Addr, Port: target.Port}
	if r.plan.icmp.Enabled {
		display := DisplayTarget(target.Addr, target.Port)
		latency, err := r.ping(ctx, target.Addr, display)
		if err != nil {
			return r.fail(ctx, &result, PhaseICMP, display, err)
		}
		result.ICMPMs = int32(latency.Milliseconds())
	}

	var handover net.Conn
	if r.plan.tcp.Enabled {
		port := target.Port
		if port == 0 {
			port = phasePort(r.plan.tcp.FallbackPort)
		}
		result.ProbePort = port
		display := DisplayTarget(target.Addr, port)
		r.session.emit(EventStart, PhaseTCP, display, 0, "")
		conn, latency, err := r.connect(ctx, target.Addr, port)
		if err != nil {
			return r.fail(ctx, &result, PhaseTCP, display, err)
		}
		result.TCPMs = int32(latency.Milliseconds())
		r.session.emit(EventOK, PhaseTCP, display, latency, "")
		if r.plan.http != nil && r.plan.http.version != httpVersion3 && r.plan.http.port(target.Port) == port {
			handover = conn
		} else {
			_ = conn.Close()
		}
	}

	if r.plan.http != nil {
		port := r.plan.http.port(target.Port)
		result.ProbePort = port
		display := DisplayTarget(target.Addr, port)
		if phase, err := r.plan.http.probe(ctx, r.session, r.egress, target.Addr, port, display, handover, &result); err != nil {
			return r.fail(ctx, &result, phase, display, err)
		}
	}
	result.Passed = true
	return result
}

func (r *probeRun) connect(ctx context.Context, addr netip.Addr, port uint16) (net.Conn, time.Duration, error) {
	var lastErr error
	for range r.plan.tcp.Attempts {
		conn, latency, err := dialTCP(ctx, r.egress, addr, port, r.plan.tcp.Timeout)
		if err == nil {
			return conn, latency, nil
		}
		lastErr = err
		if ctx.Err() != nil {
			break
		}
	}
	return nil, 0, lastErr
}

func (r *probeRun) ping(ctx context.Context, addr netip.Addr, display string) (time.Duration, error) {
	r.pingsMu.Lock()
	outcome, pending := r.pings[addr]
	if !pending {
		outcome = &pingOutcome{done: make(chan struct{})}
		r.pings[addr] = outcome
	}
	r.pingsMu.Unlock()
	if pending {
		select {
		case <-outcome.done:
			return outcome.latency, outcome.err
		case <-ctx.Done():
			return 0, ctx.Err()
		}
	}
	defer close(outcome.done)
	r.session.emit(EventStart, PhaseICMP, display, 0, "")
	outcome.latency, outcome.err = ping(ctx, r.egress, addr, r.plan.icmp.Timeout, r.plan.icmp.Count)
	if outcome.err == nil {
		r.session.emit(EventOK, PhaseICMP, display, outcome.latency, "")
	}
	return outcome.latency, outcome.err
}
