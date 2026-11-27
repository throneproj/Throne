package mobile

import (
	"context"
	"encoding/json"
	stdlog "log"
	"net/netip"
	"runtime/debug"
	"slices"
	"sync/atomic"
	"time"

	"ThroneCore/internal/probe"
	"ThroneCore/internal/scan"
	"ThroneCore/internal/warpprobe"

	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/control"
	E "github.com/sagernet/sing/common/exceptions"
)

// The libcore.proto scan messages as JSON keyed by their field names; an omitted field keeps the proto default.
type scanEntry struct {
	CIDR string `json:"cidr"`
	Port int32  `json:"port"`
}

type scanTargetSpec struct {
	Entries          []scanEntry `json:"entries"`
	DefaultPorts     []int32     `json:"default_ports"`
	MaxHostsPerEntry int64       `json:"max_hosts_per_entry"`
	// Omitted = true, the proto default.
	Shuffle  *bool      `json:"shuffle"`
	Seed     jsonUint64 `json:"seed"`
	PortMode string     `json:"port_mode"`
}

type scanDNSOptions struct {
	Enabled    bool   `json:"enabled"`
	Protocol   string `json:"protocol"`
	ConfigJSON string `json:"config_json"`
}

type scanWARPOptions struct {
	Enabled    bool   `json:"enabled"`
	ConfigJSON string `json:"config_json"`
	Attempts   int32  `json:"attempts"`
	TimeoutMs  int32  `json:"timeout_ms"`
}

type scanProbeRequest struct {
	SessionID       string          `json:"session_id"`
	Spec            scanTargetSpec  `json:"spec"`
	Cursor          uint64          `json:"cursor"`
	MaxTargets      int32           `json:"max_targets"`
	Concurrency     int32           `json:"concurrency"`
	SpawnIntervalMs int32           `json:"spawn_interval_ms"`
	DNS             scanDNSOptions  `json:"dns"`
	WARP            scanWARPOptions `json:"warp"`
}

type scanProbeResult struct {
	Address        string `json:"address"`
	Port           int32  `json:"port"`
	Passed         bool   `json:"passed"`
	FailedPhase    string `json:"failed_phase"`
	Error          string `json:"error"`
	ProbePort      int32  `json:"probe_port"`
	LocalFailure   bool   `json:"local_failure"`
	DNSMs          int32  `json:"dns_ms"`
	DNSStage       string `json:"dns_stage"`
	Sent           int32  `json:"sent"`
	Answered       int32  `json:"answered"`
	HandshakeMs    int32  `json:"handshake_ms"`
	ShortNamesOnly bool   `json:"short_names_only"`
	DNSReason      string `json:"dns_reason"`
}

type scanProbeResponse struct {
	Error          string            `json:"error"`
	Total          uint64            `json:"total"`
	NextCursor     uint64            `json:"next_cursor"`
	Aborted        bool              `json:"aborted"`
	Results        []scanProbeResult `json:"results"`
	SampledEntries int32             `json:"sampled_entries"`
	InvalidEntries int32             `json:"invalid_entries"`
}

type scanEvent struct {
	Seq       int64  `json:"seq"`
	Kind      string `json:"kind"`
	Phase     string `json:"phase"`
	Target    string `json:"target"`
	LatencyMs int32  `json:"latency_ms"`
	Error     string `json:"error"`
}

type queryScanResponse struct {
	Events      []scanEvent `json:"events"`
	LastSeq     int64       `json:"last_seq"`
	Probed      int64       `json:"probed"`
	ProbePassed int64       `json:"probe_passed"`
	URLTested   int64       `json:"url_tested"`
	URLPassed   int64       `json:"url_passed"`
}

type scanCheckNetworkRequest struct {
	Targets   []string `json:"targets"`
	TimeoutMs int32    `json:"timeout_ms"`
	DNS       bool     `json:"dns"`
	// Android only: the physical network's DNS servers, IP literals; the desktop core finds its own.
	SystemResolvers []string `json:"system_resolvers"`
}

type scanCheckNetworkResponse struct {
	Up              bool     `json:"up"`
	Error           string   `json:"error"`
	SystemResolvers []string `json:"system_resolvers"`
}

// TestResp's results and vpn_status, the latter reduced to what StartURLTest reports.
type scanURLTestResult struct {
	OutboundTag string `json:"outbound_tag"`
	LatencyMs   int32  `json:"latency_ms"`
	Error       string `json:"error"`
}

type scanVPNStatus struct {
	Tag       string `json:"tag"`
	Connected bool   `json:"connected"`
	State     string `json:"state"`
	Error     string `json:"error"`
}

type scanURLTestResponse struct {
	Results   []scanURLTestResult `json:"results"`
	VPNStatus []scanVPNStatus     `json:"vpn_status"`
}

// A Kotlin Long has no unsigned form, so a seed past 2^63 arrives negative; its bits are the seed.
type jsonUint64 uint64

func (v *jsonUint64) UnmarshalJSON(data []byte) error {
	var signed int64
	if json.Unmarshal(data, &signed) == nil {
		*v = jsonUint64(signed)
		return nil
	}
	return json.Unmarshal(data, (*uint64)(v))
}

func milliseconds(ms int32) time.Duration {
	return time.Duration(ms) * time.Millisecond
}

func scanJSON(value any) string {
	data, _ := json.Marshal(value)
	return string(data)
}

// A panic inside a gomobile call takes the whole process down; the desktop's dispatcher recovers every RPC.
func recoverScan(method string, fail func(error)) {
	if r := recover(); r != nil {
		stdlog.Printf("panic in %s: %v\n%s", method, r, debug.Stack())
		fail(E.New("core panic in ", method, ": ", r))
	}
}

func activeRunningInstance() *Instance {
	instance := activeInstance.Load()
	if instance == nil || !instance.running() {
		return nil
	}
	return instance
}

func runningInstanceMonitor() tun.DefaultInterfaceMonitor {
	if instance := activeRunningInstance(); instance != nil {
		return instance.interfaceMonitor
	}
	return nil
}

// The desktop's scanEgress: with no box running no Tun of ours is up and the OS routes; while one runs, protect takes
// scan sockets around its Tun, and a monitor without a default interface refuses rather than leak into it.
type scanEgress struct{}

func (scanEgress) Control() (control.Func, error) {
	instance := activeRunningInstance()
	if instance == nil {
		return nil, nil
	}
	if monitor := instance.interfaceMonitor; monitor != nil && monitor.DefaultInterface() == nil {
		return nil, scan.ErrNoEgress
	}
	// NewInstance installs the protector before Start; Close releases it only after the instance stops running.
	return protectDial, nil
}

// The desktop's watchScanEgress over one box's monitor: latches any default-interface loss until stop.
func watchDefaultInterface(monitor tun.DefaultInterfaceMonitor) (lost func() bool, stop func()) {
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

// The caller's DNS servers as port-53 targets, filtered like the desktop's systemResolvers.
func systemResolverTargets(addresses []string) []netip.AddrPort {
	targets := make([]netip.AddrPort, 0, len(addresses))
	for _, address := range addresses {
		addr, err := netip.ParseAddr(address)
		if err != nil {
			continue
		}
		target := netip.AddrPortFrom(addr.Unmap(), 53)
		if !usableResolver(target.Addr()) || slices.Contains(targets, target) {
			continue
		}
		targets = append(targets, target)
	}
	return targets
}

func usableResolver(addr netip.Addr) bool {
	if !addr.IsValid() || addr.IsUnspecified() || addr.IsLoopback() || addr.IsLinkLocalUnicast() || addr.IsMulticast() || addr.Zone() != "" {
		return false
	}
	// fec0::/10, the deprecated site-local range.
	return !addr.Is6() || addr.As16()[0] != 0xfe || addr.As16()[1]&0xc0 != 0xc0
}

func (in *scanProbeRequest) probeRequest() scan.ProbeRequest {
	entries := make([]scan.Entry, 0, len(in.Spec.Entries))
	for _, entry := range in.Spec.Entries {
		entries = append(entries, scan.Entry{CIDR: entry.CIDR, Port: entry.Port})
	}
	return scan.ProbeRequest{
		Spec: scan.Spec{
			Entries:          entries,
			DefaultPorts:     in.Spec.DefaultPorts,
			PortMode:         in.Spec.PortMode,
			MaxHostsPerEntry: in.Spec.MaxHostsPerEntry,
			Shuffle:          in.Spec.Shuffle == nil || *in.Spec.Shuffle,
			Seed:             uint64(in.Spec.Seed),
		},
		Cursor:     in.Cursor,
		MaxTargets: int(in.MaxTargets),
		DNS: scan.DNSOptions{
			Enabled:    in.DNS.Enabled,
			Protocol:   in.DNS.Protocol,
			ConfigJSON: in.DNS.ConfigJSON,
		},
		WARP: scan.WARPOptions{
			Enabled:    in.WARP.Enabled,
			ConfigJSON: in.WARP.ConfigJSON,
			Attempts:   int(in.WARP.Attempts),
			Timeout:    milliseconds(in.WARP.TimeoutMs),
		},
		Concurrency:   int(in.Concurrency),
		SpawnInterval: milliseconds(in.SpawnIntervalMs),
	}
}

// ScanProbe takes a ScanProbeRequest as JSON and answers a ScanProbeResponse as JSON; every failure, a malformed
// request included, comes back in its error. Blocks until the chunk is probed or StopScan aborts it.
func ScanProbe(requestJSON string) (out string) {
	failed := func(err error) string {
		return scanJSON(scanProbeResponse{Error: err.Error(), Results: []scanProbeResult{}})
	}
	defer recoverScan("ScanProbe", func(err error) { out = failed(err) })
	var in scanProbeRequest
	if err := json.Unmarshal([]byte(requestJSON), &in); err != nil {
		return failed(E.Cause(err, "decode scan probe request"))
	}
	request := in.probeRequest()
	ctx := context.Background()
	var session *scan.Session
	// Count-only touches no network: no session, so it still answers after StopScan.
	if request.MaxTargets > 0 {
		acquired, release, err := scan.Acquire(in.SessionID)
		if err != nil {
			return failed(err)
		}
		defer release()
		session = acquired
		ctx = session.Context()
		if instance := activeRunningInstance(); instance != nil && request.WARP.Enabled {
			ctx = warpprobe.WithServices(ctx, instance.handle.Context())
		}
	}
	result, err := scan.Probe(ctx, session, scanEgress{}, request)
	response := scanProbeResponse{
		Total:          result.Total,
		NextCursor:     result.NextCursor,
		Aborted:        result.Aborted,
		Results:        make([]scanProbeResult, 0, len(result.Results)),
		SampledEntries: int32(result.SampledEntries),
		InvalidEntries: int32(result.InvalidEntries),
	}
	if err != nil {
		response.Error = err.Error()
	}
	for _, r := range result.Results {
		response.Results = append(response.Results, scanProbeResult{
			Address:        r.Addr.String(),
			Port:           int32(r.Port),
			Passed:         r.Passed,
			FailedPhase:    r.FailedPhase,
			Error:          r.Error,
			ProbePort:      int32(r.ProbePort),
			LocalFailure:   r.LocalFailure,
			DNSMs:          r.DNSMs,
			DNSStage:       r.DNSStage,
			Sent:           r.Sent,
			Answered:       r.Answered,
			HandshakeMs:    r.HandshakeMs,
			ShortNamesOnly: r.ShortNamesOnly,
			DNSReason:      r.DNSReason,
		})
	}
	return scanJSON(response)
}

// QueryScan answers a QueryScanResponse as JSON: the session's events after afterSeq and its cumulative counters, all
// empty for a session never seen. A stopped session still reports its final events.
func QueryScan(sessionID string, afterSeq int64) string {
	response := queryScanResponse{Events: []scanEvent{}}
	session := scan.Lookup(sessionID)
	if session == nil {
		return scanJSON(response)
	}
	events, lastSeq := session.Events(afterSeq)
	counters := session.Counters()
	response.LastSeq = lastSeq
	response.Probed = counters.Probed
	response.ProbePassed = counters.ProbePassed
	response.URLTested = counters.URLTested
	response.URLPassed = counters.URLPassed
	for _, event := range events {
		response.Events = append(response.Events, scanEvent{
			Seq:       event.Seq,
			Kind:      event.Kind,
			Phase:     event.Phase,
			Target:    event.Target,
			LatencyMs: event.LatencyMs,
			Error:     event.Error,
		})
	}
	return scanJSON(response)
}

// StopScan aborts the session's probes and URL tests in flight, and every later one that names it is refused.
func StopScan(sessionID string) {
	scan.Stop(sessionID)
}

// ScanURLTest is the desktop's ScanURLTest over a probe box (TestCurrent is ignored): StopScan aborts it, StopTests
// never does, and results reach the session. labelsJSON is a JSON string array aligned with the outbound tags, their
// targets in the session's events. Returns {"results":[...],"vpn_status":[...]} like TestResp, vpn_status as
// {"tag","connected","state","error"} for failed AddVPNEndpointTag tags.
func ScanURLTest(platform PlatformInterface, sessionID string, request *TestRequest, labelsJSON string, warmLatency bool) (out string, err error) {
	defer recoverScan("ScanURLTest", func(panicErr error) { out, err = "", panicErr })
	if request == nil {
		return "", E.New("missing test request")
	}
	var labels []string
	if labelsJSON != "" {
		if err = json.Unmarshal([]byte(labelsJSON), &labels); err != nil {
			return "", E.Cause(err, "decode target labels")
		}
	}
	session, release, err := scan.Acquire(sessionID)
	if err != nil {
		return "", err
	}
	defer release()
	egressLost, stopWatch := watchDefaultInterface(runningInstanceMonitor())
	defer stopWatch()
	if egressLost() {
		return "", scan.ErrEgressLost
	}
	scanCtx := session.Context()
	env, err := prepareTestEnv(scanCtx, nil, false, platform, request)
	if err != nil {
		return "", err
	}
	defer env.Close()

	labelOf := make(map[string]string, len(env.Tags))
	for idx, tag := range env.Tags {
		labelOf[tag] = tag
		if idx < len(labels) && labels[idx] != "" {
			labelOf[tag] = labels[idx]
		}
	}
	publish := func(result *probe.URLTestResult) {
		// A probe cut short by StopScan or an egress outage is not a measurement.
		if result.Error != nil && (scanCtx.Err() != nil || egressLost()) {
			return
		}
		session.RecordURL(labelOf[result.Tag], result.Duration, result.Error)
	}
	results := probe.BatchURLTestTo(scanCtx, env.Box, env.Tags, request.URL, int(request.MaxConcurrency),
		warmLatency, milliseconds(request.TimeoutMs), publish)
	if scanCtx.Err() == nil && egressLost() {
		return "", scan.ErrEgressLost
	}

	response := scanURLTestResponse{
		Results:   make([]scanURLTestResult, 0, len(results)),
		VPNStatus: []scanVPNStatus{},
	}
	failed := make(map[string]bool, len(results))
	aborted := scanCtx.Err() != nil
	for idx, data := range results {
		errText := ""
		if data.Error != nil {
			errText = data.Error.Error()
			// Teardown errors after StopScan say nothing about the target; the caller must not drop it as dead.
			if aborted {
				errText = probe.ErrTestAborted.Error()
			}
		}
		failed[env.Tags[idx]] = errText != ""
		response.Results = append(response.Results, scanURLTestResult{
			OutboundTag: env.Tags[idx],
			LatencyMs:   int32(data.Duration.Milliseconds()),
			Error:       errText,
		})
	}
	// A snapshot before the box closes, as StartURLTest takes it.
	for _, tag := range request.vpnEndpointTags {
		if failed[tag] {
			connected, state, errText := vpnEndpointVerdict(env.Box, tag)
			response.VPNStatus = append(response.VPNStatus, scanVPNStatus{Tag: tag, Connected: connected, State: state, Error: errText})
		}
	}
	return scanJSON(response), nil
}

// ScanCheckNetwork takes a ScanCheckNetworkRequest as JSON and answers a ScanCheckNetworkResponse as JSON. With "dns"
// set it asks the request's "system_resolvers" (the physical network's DNS servers, IP literals) on port 53 instead of
// the targets, and the response lists the ones asked; up when none is usable.
func ScanCheckNetwork(requestJSON string) (out string) {
	failed := func(err error) string {
		return scanJSON(scanCheckNetworkResponse{Error: err.Error(), SystemResolvers: []string{}})
	}
	defer recoverScan("ScanCheckNetwork", func(err error) { out = failed(err) })
	var in scanCheckNetworkRequest
	if err := json.Unmarshal([]byte(requestJSON), &in); err != nil {
		return failed(E.Cause(err, "decode network check request"))
	}
	response := scanCheckNetworkResponse{SystemResolvers: []string{}}
	var (
		up  bool
		err error
	)
	if in.DNS {
		targets := systemResolverTargets(in.SystemResolvers)
		for _, target := range targets {
			response.SystemResolvers = append(response.SystemResolvers, target.Addr().String())
		}
		up, err = scan.CheckNetworkDNS(context.Background(), scanEgress{}, targets, milliseconds(in.TimeoutMs))
	} else {
		up, err = scan.CheckNetwork(context.Background(), scanEgress{}, in.Targets, milliseconds(in.TimeoutMs))
	}
	response.Up = up
	if err != nil {
		response.Error = err.Error()
	}
	return scanJSON(response)
}
