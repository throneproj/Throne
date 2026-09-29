package mobile

import (
	"context"
	"time"

	"ThroneCore/internal/probe"

	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/adapter"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
)

const testReportInterval = 200 * time.Millisecond

var errInstanceNotRunning = E.New("instance is not running")

// One request shape for every probe kind: the speed-test fields are ignored by the URL and IP tests.
// The tag and full-config lists stay unexported because gomobile binds no slice but []byte.
type TestRequest struct {
	CoreConfig              string
	XrayConfig              string
	NeedXray                bool
	XrayOutboundDNSStrategy string
	URL                     string
	MaxConcurrency          int32
	TimeoutMs               int32
	UseDefaultOutbound      bool
	TestCurrent             bool

	TestDownload       bool
	TestUpload         bool
	SimpleDownload     bool
	SimpleDownloadAddr string
	OnlyCountry        bool
	CountryConcurrency int32

	tags            []string
	xrayFullConfigs []string
	vpnEndpointTags []string
}

func (r *TestRequest) AddOutboundTag(tag string) {
	r.tags = append(r.tags, tag)
}

// URL tests only: an OpenVPN/OpenConnect endpoint tag whose tunnel state is reported when its test fails.
func (r *TestRequest) AddVPNEndpointTag(tag string) {
	r.vpnEndpointTags = append(r.vpnEndpointTags, tag)
}

func (r *TestRequest) AddXrayFullConfig(config string) {
	r.xrayFullConfigs = append(r.xrayFullConfigs, config)
}

type URLTestHandler interface {
	OnResult(tag string, latencyMs int32, err string)
	// After the results, once per AddVPNEndpointTag tag whose test failed: whether that tunnel is up anyway (the
	// desktop's TestResp.vpn_status).
	OnVPNStatus(tag string, connected bool, state string, err string)
	OnDone()
}

type IPTestHandler interface {
	OnResult(tag string, ip string, countryCode string, err string)
	OnDone()
}

type SpeedTestResult struct {
	Tag           string
	DlSpeed       string
	UlSpeed       string
	Latency       int32
	ServerName    string
	ServerCountry string
	Error         string
	Cancelled     bool
	DlBytes       int64
	UlBytes       int64
	Running       bool
}

type SpeedTestHandler interface {
	OnResult(result *SpeedTestResult)
	OnDone()
}

type testEnv struct {
	box   probe.Box
	tags  []string
	close func()
}

// Copy of rpc.prepareTestEnv. A probe env builds its own eager Xray instances and its own box
// (PlatformLogWriter nil: no cache.db sharing, no accounting) around a separate context holder;
// `current` measures the running instance instead and owns nothing.
func prepareTestEnv(current *Instance, testCurrent bool, platform PlatformInterface, request *TestRequest) (*testEnv, error) {
	holder := new(boxContextHolder)
	prepareXray := xrayPreparer(request.XrayOutboundDNSStrategy, holder.get)

	if testCurrent {
		if current == nil || !current.running() {
			return nil, errInstanceNotRunning
		}
		outTags := request.tags
		useDefaultOutbound := request.UseDefaultOutbound
		if _, exists := current.outbounds.Outbound("proxy"); exists {
			outTags = []string{"proxy"}
		} else {
			useDefaultOutbound = true
		}
		if useDefaultOutbound {
			outTags = []string{current.outbounds.Default().Tag()}
		}
		return &testEnv{box: current.handle, tags: outTags, close: func() {}}, nil
	}

	installProtector(platform)

	var cleanups []func()
	unwind := func() {
		for i := len(cleanups) - 1; i >= 0; i-- {
			cleanups[i]()
		}
	}

	if request.NeedXray {
		instance, err := startXrayInstance(request.XrayConfig, prepareXray)
		if err != nil {
			return nil, err
		}
		cleanups = append(cleanups, func() { _ = instance.Close() })
	}

	fullXray, err := startXrayFullConfigs(request.xrayFullConfigs, prepareXray)
	if err != nil {
		unwind()
		return nil, err
	}
	cleanups = append(cleanups, func() { closeXrayInstances(fullXray) })

	var platformInterface adapter.PlatformInterface
	if platform != nil {
		platformInterface = newPlatformInterfaceWrapper(platform)
	}
	ctx := newBoxContext(platform, platformInterface)
	options, err := parseConfig(ctx, request.CoreConfig)
	if err != nil {
		unwind()
		return nil, err
	}
	ctx, cancel := context.WithCancel(ctx)
	boxInstance, err := box.New(box.Options{
		Context: ctx,
		Options: options,
	})
	if err != nil {
		cancel()
		unwind()
		return nil, E.Cause(err, "create service")
	}
	holder.publish(ctx)
	cleanups = append(cleanups, func() { closeBoxWithTimeout(cancel, boxInstance, boxCloseTimeout, false) })
	if err = boxInstance.Start(); err != nil {
		unwind()
		return nil, E.Cause(err, "start service")
	}

	outTags := request.tags
	if request.UseDefaultOutbound {
		outTags = []string{boxInstance.Outbound().Default().Tag()}
	}
	return &testEnv{box: &boxHandle{ctx: ctx, Box: boxInstance}, tags: outTags, close: unwind}, nil
}

func errorString(err error) string {
	if err == nil {
		return ""
	}
	return err.Error()
}

// Results reach the handler as they land, the way the desktop polls QueryURLTest / QueryIPTest / QueryCountryTest,
// by draining the probe's result buffer while the batch runs; the batch's return value then fills in whatever the
// drain missed (aborted tags are never published to the buffer).
type testReporter[T any] struct {
	source   func() []*T
	tagOf    func(*T) string
	emit     func(*T)
	tags     map[string]struct{}
	reported map[string]struct{}
}

func newTestReporter[T any](tags []string, source func() []*T, tagOf func(*T) string, emit func(*T)) *testReporter[T] {
	reporter := &testReporter[T]{
		source:   source,
		tagOf:    tagOf,
		emit:     emit,
		tags:     make(map[string]struct{}, len(tags)),
		reported: make(map[string]struct{}, len(tags)),
	}
	for _, tag := range tags {
		reporter.tags[tag] = struct{}{}
	}
	return reporter
}

func (r *testReporter[T]) report(result *T) {
	if result == nil {
		return
	}
	tag := r.tagOf(result)
	if _, ours := r.tags[tag]; !ours {
		return
	}
	if _, done := r.reported[tag]; done {
		return
	}
	r.reported[tag] = struct{}{}
	r.emit(result)
}

func (r *testReporter[T]) drain() {
	for _, result := range r.source() {
		r.report(result)
	}
}

func (r *testReporter[T]) run(batch func() []*T) {
	stop := make(chan struct{})
	stopped := make(chan struct{})
	go func() {
		defer close(stopped)
		ticker := time.NewTicker(testReportInterval)
		defer ticker.Stop()
		for {
			select {
			case <-stop:
				return
			case <-ticker.C:
				r.drain()
			}
		}
	}()
	results := batch()
	close(stop)
	<-stopped
	r.drain()
	for _, result := range results {
		r.report(result)
	}
}

// The desktop's collectVPNStatus without a wait, reduced to what a URL test verdict needs.
func vpnEndpointVerdict(box probe.Box, tag string) (connected bool, state string, err string) {
	endpoints := service.FromContext[adapter.EndpointManager](box.Context())
	if endpoints == nil {
		return false, "", "nil endpoint manager"
	}
	found, loaded := endpoints.Get(tag)
	if !loaded {
		return false, "", "endpoint not found: " + tag
	}
	switch typed := found.(type) {
	case adapter.OpenVPNEndpoint:
		status := typed.OpenVPNStatus()
		return status.State == adapter.OpenVPNStateConnected && status.TunnelInfo != nil, status.State, status.Error
	case adapter.OpenConnectEndpoint:
		status := typed.OpenConnectStatus()
		return status.State == adapter.OpenConnectStateConnected && status.TunnelInfo != nil, status.State, status.Error
	}
	return false, "", "endpoint is not an openvpn/openconnect client: " + tag
}

// current is only consulted when request.TestCurrent is set; platform wires protect, the interface
// monitor and the local DNS transport into the probe box and may be nil off-device.
func StartURLTest(current *Instance, platform PlatformInterface, request *TestRequest, handler URLTestHandler) error {
	if request == nil || handler == nil {
		return E.New("nil request or handler")
	}
	env, err := prepareTestEnv(current, request.TestCurrent, platform, request)
	if err != nil {
		return err
	}
	// Held, not re-read: StopTests rearms a fresh context.
	testCtx := probe.TestContext()
	// A muxed config needs a warm connection; the live instance already is one.
	twice := !request.TestCurrent
	timeout := time.Duration(request.TimeoutMs) * time.Millisecond
	go func() {
		defer handler.OnDone()
		defer env.close()
		failed := make(map[string]bool, len(env.tags))
		newTestReporter(env.tags, probe.URLReporter.Results,
			func(result *probe.URLTestResult) string { return result.Tag },
			func(result *probe.URLTestResult) {
				failed[result.Tag] = result.Error != nil
				handler.OnResult(result.Tag, int32(result.Duration.Milliseconds()), errorString(result.Error))
			},
		).run(func() []*probe.URLTestResult {
			return probe.BatchURLTest(testCtx, env.box, env.tags, request.URL, int(request.MaxConcurrency), twice, timeout)
		})
		// A snapshot before the box closes: the probe already sat out the handshake.
		for _, tag := range request.vpnEndpointTags {
			if failed[tag] {
				connected, state, err := vpnEndpointVerdict(env.box, tag)
				handler.OnVPNStatus(tag, connected, state, err)
			}
		}
	}()
	return nil
}

// Always builds its own box: there is no test-current variant of an IP test.
func StartIPTest(platform PlatformInterface, request *TestRequest, handler IPTestHandler) error {
	if request == nil || handler == nil {
		return E.New("nil request or handler")
	}
	env, err := prepareTestEnv(nil, false, platform, request)
	if err != nil {
		return err
	}
	testCtx := probe.TestContext()
	timeout := time.Duration(request.TimeoutMs) * time.Millisecond
	go func() {
		defer handler.OnDone()
		defer env.close()
		newTestReporter(env.tags, probe.IPReporter.Results,
			func(result *probe.IPTestResult) string { return result.Tag },
			func(result *probe.IPTestResult) {
				handler.OnResult(result.Tag, result.Result.IP, result.Result.CountryCode, errorString(result.Error))
			},
		).run(func() []*probe.IPTestResult {
			return probe.BatchIPTest(testCtx, env.box, env.tags, int(request.MaxConcurrency), true, timeout)
		})
	}()
	return nil
}

func flattenSpeedTestResult(result probe.SpeedTestResult, running bool) *SpeedTestResult {
	return &SpeedTestResult{
		Tag:           result.Tag,
		DlSpeed:       result.DlSpeed,
		UlSpeed:       result.UlSpeed,
		Latency:       result.Latency,
		ServerName:    result.ServerName,
		ServerCountry: result.ServerCountry,
		Error:         errorString(result.Error),
		Cancelled:     result.Cancelled,
		DlBytes:       result.DlBytes,
		UlBytes:       result.UlBytes,
		Running:       running,
	}
}

func StartSpeedTest(current *Instance, platform PlatformInterface, request *TestRequest, handler SpeedTestHandler) error {
	if request == nil || handler == nil {
		return E.New("nil request or handler")
	}
	if !request.TestDownload && !request.TestUpload && !request.SimpleDownload && !request.OnlyCountry {
		return E.New("cannot run empty test")
	}
	env, err := prepareTestEnv(current, request.TestCurrent, platform, request)
	if err != nil {
		return err
	}
	testCtx := probe.TestContext()
	timeout := time.Duration(request.TimeoutMs) * time.Millisecond
	go func() {
		defer handler.OnDone()
		defer env.close()
		emit := func(result *probe.SpeedTestResult) { handler.OnResult(flattenSpeedTestResult(*result, false)) }
		batch := func() []*probe.SpeedTestResult {
			return probe.BatchSpeedTest(testCtx, env.box, env.tags,
				request.TestDownload, request.TestUpload, request.SimpleDownload, request.SimpleDownloadAddr,
				timeout, request.OnlyCountry, request.CountryConcurrency)
		}
		if !request.OnlyCountry {
			// The other modes measure one tag at a time and publish nothing before the batch returns.
			for _, result := range batch() {
				emit(result)
			}
			return
		}
		newTestReporter(env.tags, probe.CountryResults.Results,
			func(result *probe.SpeedTestResult) string { return result.Tag }, emit).run(batch)
	}()
	return nil
}

// Live progress of the speed test in flight; Running is false once it has finished.
func QuerySpeedTest() *SpeedTestResult {
	result, running := probe.SpTQuerier.Result()
	return flattenSpeedTestResult(result, running)
}

func StopTests() {
	probe.CancelTests()
}
