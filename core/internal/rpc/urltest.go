package rpc

import (
	"context"
	"errors"
	"net/netip"
	"sync"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/netmon"
	"ThroneCore/internal/probe"
	"ThroneCore/internal/scan"
)

const (
	localNetworkCheckTimeout = 2 * time.Second
	localNetworkVerdictTTL   = 2 * time.Second
)

// Held across the check, so concurrent Test calls wait for one probe and share its verdict.
var localNetwork struct {
	sync.Mutex
	checkedAt time.Time
	up        bool
}

func localNetworkUp() bool {
	localNetwork.Lock()
	defer localNetwork.Unlock()
	if time.Since(localNetwork.checkedAt) >= localNetworkVerdictTTL {
		localNetwork.up = checkLocalNetwork()
		localNetwork.checkedAt = time.Now()
	}
	return localNetwork.up
}

func checkLocalNetwork() bool {
	monitor := netmon.Monitor()
	// Without the monitor the egress is unknown: no verdict, so failures stand.
	if monitor == nil {
		return true
	}
	if monitor.DefaultInterface() == nil {
		return false
	}
	resolvers := systemResolvers()
	targets := make([]netip.AddrPort, 0, len(resolvers))
	for _, resolver := range resolvers {
		targets = append(targets, netip.AddrPortFrom(resolver, 53))
	}
	up, _ := scan.CheckInternetDNS(context.Background(), scanEgress{}, targets, localNetworkCheckTimeout)
	return up
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (*gen.TestResp, error) {
	env, err := prepareTestEnv(in.GetTestCurrent(), in.GetNeedXray(), in.GetXrayConfig(),
		in.XrayFullConfigs, in.GetConfig(), in.OutboundTags, in.GetUseDefaultOutbound(),
		in.GetXrayOutboundDnsStrategy())
	if err != nil {
		if errors.Is(err, errInstanceNotRunning) {
			return &gen.TestResp{Results: []*gen.URLTestResp{{
				OutboundTag: To("proxy"),
				LatencyMs:   To(int32(0)),
				Error:       To(err.Error()),
			}}}, nil
		}
		return nil, err
	}
	defer env.Close()

	// Held, not re-read: StopTest rearms a fresh context, uncancelled.
	testCtx := probe.TestContext()
	if in.GetTestCurrent() {
		testCtx = probe.LiveInstance(testCtx)
	}

	egressLost, stopWatch := watchScanEgress()
	defer stopWatch()

	// A muxed config needs a warm connection; the live instance already is one.
	twice := !in.GetTestCurrent()
	results := probe.BatchURLTest(testCtx, env.Box, env.Tags, in.GetUrl(),
		int(in.GetMaxConcurrency()), twice, time.Duration(in.GetTestTimeoutMs())*time.Millisecond)

	res := make([]*gen.URLTestResp, 0, len(results))
	failed := make(map[string]bool, len(results))
	var failures []*gen.URLTestResp
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		failed[env.Tags[idx]] = errStr != ""
		result := &gen.URLTestResp{
			OutboundTag: To(env.Tags[idx]),
			LatencyMs:   To(int32(data.Duration.Milliseconds())),
			Error:       To(errStr),
		}
		res = append(res, result)
		if errStr != "" && !errors.Is(data.Error, probe.ErrTestAborted) && !errors.Is(data.Error, context.Canceled) {
			failures = append(failures, result)
		}
	}

	out := &gen.TestResp{Results: res}
	// `defer env.Close()` tears the box down on return, so this cannot wait.
	var pending []string
	for _, tag := range in.VpnEndpointTags {
		if failed[tag] {
			pending = append(pending, tag)
		}
	}
	if len(pending) > 0 {
		// A snapshot: the probe already sat out the handshake, so a tunnel that settles only now never carried it.
		out.VpnStatus = collectVPNStatus(testCtx, env.Box, pending, 0)
	}
	if len(failures) > 0 && (egressLost() || !localNetworkUp()) {
		for _, result := range failures {
			result.Inconclusive = To(true)
		}
	}
	return out, nil
}

func (s *server) StopTest(ctx context.Context, in *gen.EmptyReq) (*gen.EmptyResp, error) {
	probe.CancelTests()

	return &gen.EmptyResp{}, nil
}

func (s *server) QueryURLTest(ctx context.Context, in *gen.EmptyReq) (out *gen.QueryURLTestResponse, _ error) {
	results := probe.URLReporter.Results()
	out = &gen.QueryURLTestResponse{}
	for _, r := range results {
		errStr := ""
		if r.Error != nil {
			errStr = r.Error.Error()
		}
		out.Results = append(out.Results, &gen.URLTestResp{
			OutboundTag: To(r.Tag),
			LatencyMs:   To(int32(r.Duration.Milliseconds())),
			Error:       To(errStr),
		})
	}
	return
}

func (s *server) IPTest(ctx context.Context, in *gen.IPTestRequest) (*gen.IPTestResp, error) {
	// Always builds its own box: there is no test-current variant of an IP test.
	const current = false
	env, err := prepareTestEnv(current, in.GetNeedXray(), in.GetXrayConfig(),
		in.XrayFullConfigs, in.GetConfig(), in.OutboundTags, in.GetUseDefaultOutbound(),
		in.GetXrayOutboundDnsStrategy())
	if err != nil {
		return nil, err
	}
	defer env.Close()

	timeout := time.Duration(in.GetTestTimeoutMs()) * time.Millisecond
	results := probe.BatchIPTest(probe.TestContext(), env.Box, env.Tags,
		int(in.GetMaxConcurrency()), !current, timeout, in.GetProvider())

	res := make([]*gen.IPTestRes, 0, len(results))
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		res = append(res, &gen.IPTestRes{
			OutboundTag: To(env.Tags[idx]),
			Ip:          To(data.Result.IP),
			CountryCode: To(data.Result.CountryCode),
			Error:       To(errStr),
		})
	}
	return &gen.IPTestResp{Results: res}, nil
}

func (s *server) QueryIPTest(ctx context.Context, in *gen.EmptyReq) (out *gen.QueryIPTestResponse, _ error) {
	results := probe.IPReporter.Results()
	out = &gen.QueryIPTestResponse{}
	for _, r := range results {
		errStr := ""
		if r.Error != nil {
			errStr = r.Error.Error()
		}
		out.Results = append(out.Results, &gen.IPTestRes{
			OutboundTag: To(r.Tag),
			Ip:          To(r.Result.IP),
			CountryCode: To(r.Result.CountryCode),
			Error:       To(errStr),
		})
	}
	return
}
