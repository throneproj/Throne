package rpc

import (
	"context"
	"errors"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/probe"
	"ThroneCore/internal/rulesets"
	"ThroneCore/internal/scan"
)

func milliseconds(ms int32) time.Duration {
	return time.Duration(ms) * time.Millisecond
}

func scanProbeRequest(in *gen.ScanProbeRequest) scan.ProbeRequest {
	spec := in.GetSpec()
	entries := make([]scan.Entry, 0, len(spec.GetEntries()))
	for _, entry := range spec.GetEntries() {
		entries = append(entries, scan.Entry{CIDR: entry.GetCidr(), Port: entry.GetPort()})
	}
	icmp, tcp, http, dns := in.GetIcmp(), in.GetTcp(), in.GetHttp(), in.GetDns()
	return scan.ProbeRequest{
		Spec: scan.Spec{
			Entries:          entries,
			DefaultPorts:     spec.GetDefaultPorts(),
			PortMode:         spec.GetPortMode(),
			MaxHostsPerEntry: spec.GetMaxHostsPerEntry(),
			Shuffle:          spec.GetShuffle(),
			Seed:             spec.GetSeed(),
		},
		Cursor:     in.GetCursor(),
		MaxTargets: int(in.GetMaxTargets()),
		ICMP: scan.ICMPOptions{
			Enabled: icmp.GetEnabled(),
			Timeout: milliseconds(icmp.GetTimeoutMs()),
			Count:   int(icmp.GetCount()),
		},
		TCP: scan.TCPOptions{
			Enabled:      tcp.GetEnabled(),
			Timeout:      milliseconds(tcp.GetTimeoutMs()),
			Attempts:     int(tcp.GetAttempts()),
			FallbackPort: tcp.GetFallbackPort(),
		},
		HTTP: scan.HTTPOptions{
			Enabled:               http.GetEnabled(),
			TLS:                   http.GetTls(),
			ServerName:            http.GetServerName(),
			Host:                  http.GetHost(),
			Path:                  http.GetPath(),
			Method:                http.GetMethod(),
			Version:               http.GetHttpVersion(),
			ALPN:                  http.GetAlpn(),
			MinVersion:            http.GetMinVersion(),
			MaxVersion:            http.GetMaxVersion(),
			Fingerprint:           http.GetFingerprint(),
			Insecure:              http.GetInsecure(),
			DisableSNI:            http.GetDisableSni(),
			Fragment:              http.GetFragment(),
			FragmentFallbackDelay: milliseconds(http.GetFragmentFallbackDelayMs()),
			RecordFragment:        http.GetRecordFragment(),
			MixedCaseSNI:          http.GetMixedCaseSni(),
			Timeout:               milliseconds(http.GetTimeoutMs()),
			FallbackPort:          http.GetFallbackPort(),
		},
		DNS: scan.DNSOptions{
			Enabled:    dns.GetEnabled(),
			Protocol:   dns.GetProtocol(),
			ConfigJSON: dns.GetConfigJson(),
			Level:      dns.GetLevel(),
			Timeout:    milliseconds(dns.GetTimeoutMs()),
		},
		Concurrency:   int(in.GetConcurrency()),
		SpawnInterval: milliseconds(in.GetSpawnIntervalMs()),
	}
}

func (s *server) ScanProbe(ctx context.Context, in *gen.ScanProbeRequest) (*gen.ScanProbeResponse, error) {
	request := scanProbeRequest(in)
	var session *scan.Session
	// Count-only touches no network: no session, so it still answers after StopScan.
	if request.MaxTargets > 0 {
		acquired, release, err := scan.Acquire(in.GetSessionId())
		if err != nil {
			return &gen.ScanProbeResponse{Error: To(err.Error())}, nil
		}
		defer release()
		session = acquired
		ctx = session.Context()
	}
	result, err := scan.Probe(ctx, session, scanEgress{}, request)
	out := &gen.ScanProbeResponse{
		Total:          To(result.Total),
		NextCursor:     To(result.NextCursor),
		Aborted:        To(result.Aborted),
		SampledEntries: To(int32(result.SampledEntries)),
		InvalidEntries: To(int32(result.InvalidEntries)),
		Results:        make([]*gen.ScanProbeResult, 0, len(result.Results)),
	}
	if err != nil {
		out.Error = To(err.Error())
	}
	for _, r := range result.Results {
		out.Results = append(out.Results, &gen.ScanProbeResult{
			Address:      To(r.Addr.String()),
			Port:         To(int32(r.Port)),
			Passed:       To(r.Passed),
			FailedPhase:  To(r.FailedPhase),
			Error:        To(r.Error),
			IcmpMs:       To(r.ICMPMs),
			TcpMs:        To(r.TCPMs),
			TlsMs:        To(r.TLSMs),
			HttpMs:       To(r.HTTPMs),
			HttpStatus:   To(int32(r.HTTPStatus)),
			ProbePort:    To(int32(r.ProbePort)),
			LocalFailure: To(r.LocalFailure),
			DnsMs:        To(r.DNSMs),
			MaxQname:     To(r.MaxQName),
			UploadMtu:    To(r.UploadMTU),
			DownloadMtu:  To(r.DownloadMTU),
			DnsStage:     To(r.DNSStage),
		})
	}
	return out, nil
}

func (s *server) QueryScan(ctx context.Context, in *gen.QueryScanRequest) (*gen.QueryScanResponse, error) {
	out := &gen.QueryScanResponse{}
	session := scan.Lookup(in.GetSessionId())
	if session == nil {
		return out, nil
	}
	events, lastSeq := session.Events(in.GetAfterSeq())
	counters := session.Counters()
	out.LastSeq = To(lastSeq)
	out.Probed = To(counters.Probed)
	out.ProbePassed = To(counters.ProbePassed)
	out.UrlTested = To(counters.URLTested)
	out.UrlPassed = To(counters.URLPassed)
	out.Events = make([]*gen.ScanEvent, 0, len(events))
	for _, event := range events {
		out.Events = append(out.Events, &gen.ScanEvent{
			Seq:       To(event.Seq),
			Kind:      To(event.Kind),
			Phase:     To(event.Phase),
			Target:    To(event.Target),
			LatencyMs: To(event.LatencyMs),
			Error:     To(event.Error),
		})
	}
	return out, nil
}

func (s *server) StopScan(ctx context.Context, in *gen.StopScanRequest) (*gen.EmptyResp, error) {
	scan.Stop(in.GetSessionId())
	return &gen.EmptyResp{}, nil
}

// StopScan aborts it, StopTest never does, and results reach only the session.
func (s *server) ScanURLTest(ctx context.Context, in *gen.ScanURLTestRequest) (*gen.TestResp, error) {
	test := in.GetTest()
	if test == nil {
		return nil, errors.New("missing test request")
	}
	session, release, err := scan.Acquire(in.GetSessionId())
	if err != nil {
		return nil, err
	}
	defer release()
	egressLost, stopWatch := watchScanEgress()
	defer stopWatch()
	if egressLost() {
		return nil, scan.ErrEgressLost
	}
	env, err := prepareTestEnv(false, test.GetNeedXray(), test.GetXrayConfig(),
		test.XrayFullConfigs, test.GetConfig(), test.OutboundTags, test.GetUseDefaultOutbound(),
		test.GetXrayOutboundDnsStrategy())
	if err != nil {
		return nil, err
	}
	defer env.close()

	scanCtx := session.Context()
	labels := in.GetTargetLabels()
	labelOf := make(map[string]string, len(env.tags))
	for idx, tag := range env.tags {
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
	var results []*probe.URLTestResult
	if bodyBytes := in.GetBodyBytes(); bodyBytes > 0 {
		results = probe.BatchDownloadTestTo(scanCtx, env.box, env.tags, test.GetUrl(), bodyBytes, int(test.GetMaxConcurrency()),
			milliseconds(test.GetTestTimeoutMs()), publish)
	} else {
		results = probe.BatchURLTestTo(scanCtx, env.box, env.tags, test.GetUrl(), int(test.GetMaxConcurrency()),
			in.GetWarmLatency(), milliseconds(test.GetTestTimeoutMs()), publish)
	}
	if scanCtx.Err() == nil && egressLost() {
		return nil, scan.ErrEgressLost
	}

	res := make([]*gen.URLTestResp, 0, len(results))
	failed := make(map[string]bool, len(results))
	aborted := scanCtx.Err() != nil
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
			// Teardown errors after StopScan say nothing about the target; the GUI must not drop it as dead.
			if aborted {
				errStr = probe.ErrTestAborted.Error()
			}
		}
		failed[env.tags[idx]] = errStr != ""
		res = append(res, &gen.URLTestResp{
			OutboundTag: To(env.tags[idx]),
			LatencyMs:   To(int32(data.Duration.Milliseconds())),
			Error:       To(errStr),
		})
	}
	out := &gen.TestResp{Results: res}
	var pending []string
	for _, tag := range test.VpnEndpointTags {
		if failed[tag] {
			pending = append(pending, tag)
		}
	}
	if len(pending) > 0 {
		out.VpnStatus = collectVPNStatus(scanCtx, env.box, pending, 0)
	}
	return out, nil
}

func (s *server) ScanCheckNetwork(ctx context.Context, in *gen.ScanCheckNetworkRequest) (*gen.ScanCheckNetworkResponse, error) {
	up, err := scan.CheckNetwork(ctx, scanEgress{}, in.GetTargets(), milliseconds(in.GetTimeoutMs()))
	out := &gen.ScanCheckNetworkResponse{Up: To(up)}
	if err != nil {
		out.Error = To(err.Error())
	}
	return out, nil
}

func (s *server) ParseRuleSet(ctx context.Context, in *gen.ParseRuleSetRequest) (*gen.ParseRuleSetResponse, error) {
	cidrs, skipped, version, err := rulesets.ParseIPCIDRs(in.GetContent())
	out := &gen.ParseRuleSetResponse{
		Cidrs:        cidrs,
		SkippedRules: To(int32(skipped)),
		Version:      To(int32(version)),
	}
	if err != nil {
		out.Error = To(err.Error())
	}
	return out, nil
}
